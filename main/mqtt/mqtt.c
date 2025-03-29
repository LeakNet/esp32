#include "mqtt.h"
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

#include "sensors.h"
#include "common.h"

#include <cJSON.h>
#include <zlib.h>

#include <wifi_provisioning/manager.h>
#include "prov.h"

#include <math.h>

#define QOS0 0
#define QOS1 1
#define QOS2 2

#define MAX_RECONNECTION_RETRIES 3
static int connection_retries = 0;

static const char* TAG = "MQTT"; 

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
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

            if (++connection_retries >= MAX_RECONNECTION_RETRIES) {
                ESP_LOGI(TAG, "Max retries reached. Stopping MQTT client and allowing reprovisioning");
                wifi_prov_mgr_reset_sm_state_for_reprovision();
                connection_retries = 0;
            } else {
                ESP_LOGI(TAG, "Reconnecting to MQTT broker...");
                esp_mqtt_client_reconnect(client);
            }
            vTaskDelay(30000 / portTICK_PERIOD_MS);

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

    char client_id[12];
    size_t client_id_length;
    app_get_device_id(client_id, &client_id_length);

    const esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = "mqtts://irrigo.xyz:8883",
        .broker.verification.certificate = (const char *)server_cert_pem_start,
        .broker.verification.certificate_len = server_cert_pem_end - server_cert_pem_start,
        .network = {
            .disable_auto_reconnect = true
        },
        .credentials = {
            .authentication = {
                .certificate = (const char *)client_cert_pem_start,
                .certificate_len = client_cert_pem_end - client_cert_pem_start,
    
                .key = (const char *)client_key_pem_start,
                .key_len = client_key_pem_end - client_key_pem_start,
            },
            .client_id = client_id
        }
        
    };

    client = esp_mqtt_client_init(&mqtt_config);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

void app_mqtt_start(void) {
    esp_mqtt_client_start(client);
}

double dround(double val) {
    int charsNeeded = 1 + snprintf(NULL, 0, "%.3f", val);
    char* buffer = malloc(charsNeeded);
    snprintf(buffer, charsNeeded, "%.3f", val);
    double result = atof(buffer);
    free(buffer);
    return result;
}

void app_mqtt_send_sensor_data(app_sensors_sample_t* data, size_t num_values) {

    cJSON *json_data = cJSON_CreateArray();

    for (int i = 0; i < num_values; i++) {
        cJSON *data_object = cJSON_CreateObject();

        cJSON_AddNumberToObject(data_object, "pressure", dround(data[i].pressure));
        cJSON_AddNumberToObject(data_object, "flow", dround(data[i].flow));
        cJSON_AddNumberToObject(data_object, "timestamp", data[i].timestamp);
        cJSON_AddItemToArray(json_data, data_object);
    }

    char *json_string = cJSON_PrintUnformatted(json_data);

    ESP_LOGI(TAG, "SENSOR DATA: %s", json_string);

    uLong input_length = strlen(json_string) + 1;
    uLong compressed_size = compressBound(input_length);

    unsigned char* compressed_data = (unsigned char *)malloc(compressed_size);
    if (compressed_data == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return;
    }

    int result = compress(compressed_data, &compressed_size, (const Bytef *)json_string, input_length);
    if (result != Z_OK) {
        ESP_LOGE(TAG, "Compression failed: %d\n", result);
        free(compressed_data);
        return;
    }

    int ret = esp_mqtt_client_publish(client, "data", (const char*)compressed_data, 0, QOS2, 0);
    ESP_LOGI(TAG, "esp_mqtt_client_publish: %d", ret);

    cJSON_Delete(json_data);
    free(json_string);
    free(compressed_data);
}