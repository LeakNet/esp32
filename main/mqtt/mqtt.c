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

#include <cJSON.h>
#include <zlib.h>

#define QOS0 0
#define QOS1 1
#define QOS2 2

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
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        // ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void app_mqtt_init(void) {

    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtts://irrigo.xyz:8883",
        .broker.verification.certificate = (const char *)server_cert_pem_start,
        .broker.verification.certificate_len = server_cert_pem_end - server_cert_pem_start,
        .credentials = {
            .authentication = {
                .certificate = (const char *)client_cert_pem_start,
                .certificate_len = client_cert_pem_end - client_cert_pem_start,

                .key = (const char *)client_key_pem_start,
                .key_len = client_key_pem_end - client_key_pem_start
            },
        }
    };

    // ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
}

void app_mqtt_start(void) {
    esp_mqtt_client_start(client);
}

void app_mqtt_send_sensor_data(sensors_data_point_t* data, size_t num_values) {

    cJSON *json_data = cJSON_CreateArray();

    for (int i = 0; i < num_values; i++) {
        cJSON *data_object = cJSON_CreateObject();

        char pressure[32];
        char flow[32];
        snprintf(pressure, sizeof(pressure), "%.3f", data[i].pressure);
        snprintf(flow, sizeof(flow), "%.3f", data[i].flow);

        cJSON_AddStringToObject(data_object, "pressure", pressure);
        cJSON_AddStringToObject(data_object, "flow", flow);
        cJSON_AddNumberToObject(data_object, "timestamp", data[i].timestamp);
    
        cJSON_AddItemToArray(json_data, data_object);
    }

    char *json_string = cJSON_PrintUnformatted(json_data);

    // Add compression

    if (json_string) {
        ESP_LOGI(TAG, "SENSOR DATA: %s", json_string);
    }

    int ret = esp_mqtt_client_publish(client, "sensor/data", json_string, 0, QOS2, 0);
    ESP_LOGI(TAG, "esp_mqtt_client_publish: %d", ret);

    // Free the JSON object and the string
    cJSON_Delete(json_data);
    free(json_string);
}