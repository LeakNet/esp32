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

#include "sdkconfig.h"

#include "common.h"
#include "prov.h"
#include "sntp.h"
#include "mqtt.h"
#include "sensors.h"

#include "mbedtls/esp_debug.h"
#include "esp_mac.h"

static const char *TAG = "app";

EventGroupHandle_t app_event_group = NULL;
EventGroupHandle_t wifi_event_group = NULL;

static void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
}

void app_main(void)
{
    
    /* Initialize NVS partition */
    nvs_init();

    char client_id[11];
    uint8_t eth_mac[6];
    const char *client_id_prefix = "ESP-";
    esp_read_mac(eth_mac, ESP_MAC_WIFI_STA);
    snprintf(client_id, sizeof(client_id), "%s%02X%02X%02X",
            client_id_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);

    ESP_LOGI(TAG, "Client id: %s", client_id);

    app_nvs_save_str(MQTT_CLIENT_ID_NVS_KEY, client_id);

    char test[11];
    size_t test_size;
    app_nvs_get_str(MQTT_CLIENT_ID_NVS_KEY, test, &test_size);
    ESP_LOGI(TAG, "Client id: %s", test);

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();
    if (wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group!");
        return;
    }

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, NULL));

    app_event_group = xEventGroupCreate();
    if (app_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group!");
        return;
    }
    
    app_sntp_init();
    app_sensors_init();
    app_mqtt_init();
    app_wifi_init();
    app_prov_init();

    bool provisioned = false;
#ifdef CONFIG_EXAMPLE_RESET_PROVISIONED
    wifi_prov_mgr_reset_provisioning();
#else
    /* Let's find out if the device is provisioned */
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

#endif

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned) {
        app_prov_start();
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        /* We don't need the manager as device is already provisioned,
         * so let's release it's resources */
        app_prov_stop();

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
        /* Start Wi-Fi station */
        wifi_init_sta();
    }

    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to wifi");


    /* Resetting provisioning state machine to enable re-provisioning */
    // wifi_prov_mgr_reset_sm_state_for_reprovision();

    app_sntp_start();

    app_mqtt_start();
}
