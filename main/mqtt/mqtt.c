#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "mqtt_client.h"

#include "esp_log.h"

#include <wifi_provisioning/manager.h>

#include <math.h>

#include "pb_encode.h"
#include "sample_batch.pb.h"
#include "sensors.h"
#include "common.h"
#include "prov.h"
#include "mqtt.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "ulp.h"
#include "ulp_main.h"
#include "esp_sleep.h"


#define QOS0 0
#define QOS1 1
#define QOS2 2

#define MAX_RECONNECTION_RETRIES 3
static int connection_retries = 0;

static const char* TAG = "MQTT"; 

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

// mTLS certificates for MQTT
extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_pem_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_pem_end");

extern const uint8_t client_key_pem_start[] asm("_binary_client_key_pem_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_pem_end");

extern const uint8_t server_cert_pem_start[] asm("_binary_ca_crt_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_crt_pem_end");

esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    // ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    // int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(app_event_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(app_event_group, MQTT_CONNECTED_BIT);

            // if (++connection_retries >= MAX_RECONNECTION_RETRIES) {
            //     ESP_LOGI(TAG, "Max retries reached. Stopping MQTT client and allowing reprovisioning");
            //     wifi_prov_mgr_reset_sm_state_for_reprovision();
            //     connection_retries = 0;
            // } else {
            //     ESP_LOGI(TAG, "Reconnecting to MQTT broker...");
            //     esp_mqtt_client_reconnect(client);
            // }
            // vTaskDelay(30000 / portTICK_PERIOD_MS);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGI(TAG, "Failed to connect to ");
            }
            
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

void app_mqtt_init(void) {

    char device_id[12];
    size_t device_id_length;
    app_get_device_id(device_id, &device_id_length);

    const esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = "mqtts://irrigo.xyz:8883",
        .broker.verification.certificate = (const char *)server_cert_pem_start,
        // .broker.verification.certificate_len = server_cert_pem_end - server_cert_pem_start,
        .credentials = {
            .authentication = {
                .certificate = (const char *)client_cert_pem_start,
                // .certificate_len = client_cert_pem_end - client_cert_pem_start,
                .key = (const char *)client_key_pem_start,
                // .key_len = client_key_pem_end - client_key_pem_start,
            },
            .client_id = device_id
        }
        
    };

    client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

void app_mqtt_start(void) {
    esp_mqtt_client_start(client);
}

void app_mqtt_task(void *pvParameters) {
    SampleBatch batch = SampleBatch_init_zero;
    uint8_t buffer[SampleBatch_size];

    while (1) {
        xEventGroupWaitBits(app_event_group, MQTT_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        app_sensors_read(&batch.samples[batch.samples_count]);
        batch.samples_count++;
        
        if (batch.samples_count == SAMPLE_BATCH_SIZE) {
            pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
            pb_encode(&stream, SampleBatch_fields, &batch);

            ESP_LOGI(TAG, "Sending %d bytes", stream.bytes_written);
            ESP_LOGI(TAG, "Sending %d samples", batch.samples_count);

            esp_mqtt_client_publish(client, "data", (const char*)buffer, stream.bytes_written, 2, 0);
            batch.samples_count = 0;

            float PressureError = 0.5;
            int Sleep = 1;
            for (int i = 0; i < SAMPLE_BATCH_SIZE; ++i) { if (batch.samples[i].flow != 0 || batch.samples[i].pressure < (batch.samples[0].pressure - PressureError) || batch.samples[i].pressure > (batch.samples[0].pressure - PressureError)) { Sleep = 0; break; } }
            if (Sleep) {
                /* Init ULP program */
                ESP_ERROR_CHECK(ulp_load_binary(0, &(ulp_main_bin_start[0]), (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t)));

                gpio_num_t gpio_num = FLOW_SENSOR_PIN;
                int rtcio_num = rtc_io_number_get(gpio_num);
                assert(rtc_gpio_is_valid_gpio(gpio_num) && "GPIO used for pulse counting must be an RTC IO");

                /* These variables exist in auto-generated ulp_main.h */
                ulp_next_edge = 0;
                ulp_io_number = rtcio_num;
                ulp_edge_count_to_wake_up = 10;

                /* Setup for the RTC pin */
                rtc_gpio_init(gpio_num);
                rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
                rtc_gpio_pulldown_dis(gpio_num);
                rtc_gpio_pullup_dis(gpio_num);
                rtc_gpio_hold_en(gpio_num);

#if CONFIG_IDF_TARGET_ESP32
                rtc_gpio_isolate(GPIO_NUM_12);
                rtc_gpio_isolate(GPIO_NUM_15);
#endif // CONFIG_IDF_TARGET_ESP32

                /* Start ULP program */
                ulp_set_wakeup_period(0, 20000);
                ESP_ERROR_CHECK(ulp_run(&ulp_entry - RTC_SLOW_MEM));

                esp_sleep_enable_ulp_wakeup();
                ESP_LOGI("Deep Sleep", "Deep sleep due to inactivity (f: %f, p: %f)", batch.samples[0].flow, batch.samples[0].pressure);
                esp_deep_sleep_start();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_INTERVAL_MS));
    }
}