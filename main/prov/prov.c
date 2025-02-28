#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include "common.h"

static const char* TAG = "prov";

#define BLE_ENDPOINT_NAME "user"

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

void app_wifi_init() {
    /* Initialize Wi-Fi including netif with default config */
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
}


static esp_err_t custom_endpoint_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
    uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if (inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }

    app_nvs_save_str(NVS_USER_ID_KEY, (char*)inbuf);

    char response[] = "OK";
    *outbuf = (uint8_t *)strdup(response);
    *outlen = strlen(response) + 1;

    return ESP_OK;
}

void app_prov_init() {

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
        .wifi_prov_conn_cfg = {
           .wifi_conn_attempts =  CONFIG_EXAMPLE_PROV_MGR_CONNECTION_CNT,
        },
        .scheme = wifi_prov_scheme_ble,
        // .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
        // .app_event_handler = wifi_prov_event_handler,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));
}

void print_uuid(const uint8_t uuid[16]) {
    printf("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
        uuid[15], uuid[14], uuid[13], uuid[12],  // First 8 characters (32 bits)
        uuid[11], uuid[10],                      // Next 4 characters (16 bits)
        uuid[9], uuid[8],                        // Next 4 characters (16 bits)
        uuid[7], uuid[6],                        // Next 4 characters (16 bits)
        uuid[5], uuid[4], uuid[3], uuid[2], uuid[1], uuid[0]  // Last 12 characters (48 bits)
    );
}

void app_prov_start() {
    ESP_LOGI(TAG, "Starting provisioning");

    char service_name[12];
    get_device_service_name(service_name, sizeof(service_name));
    
    // wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
    // const char *pop = "abcd1234";
    // wifi_prov_security1_params_t *sec_params = pop;

    wifi_prov_security_t security = WIFI_PROV_SECURITY_0;

    uint8_t custom_service_uuid[] = {
        /* LSB <---------------------------------------
        * ---------------------------------------> MSB */
        0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };

    print_uuid((const uint8_t*)custom_service_uuid);

    wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

    wifi_prov_mgr_endpoint_create(BLE_ENDPOINT_NAME);

    /* Do not stop and de-init provisioning even after success,
    * so that we can restart it later. */
    wifi_prov_mgr_disable_auto_stop(1000);

    /* Start provisioning service */
    // ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *) sec_params, service_name, NULL));
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, NULL, service_name, NULL));

    wifi_prov_mgr_endpoint_register(BLE_ENDPOINT_NAME, custom_endpoint_handler, NULL);
}

void app_prov_stop() {
    wifi_prov_mgr_stop_provisioning();
    wifi_prov_mgr_deinit();
    ESP_LOGI(TAG, "Provisioning stopped");
}