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

#include "esp_mac.h"
#include "esp_pm.h"

#include "ulp.h"
#include "ulp_main.h"
#include "esp_sleep.h"

static const char *TAG = "app";

static void init_ulp_program(void);

EventGroupHandle_t app_event_group = NULL;

static void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM)); // for low power
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
    // esp_log_level_set("*", ESP_LOG_VERBOSE); // Set all logs to verbose

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_ULP) {
        printf("Not ULP wakeup, initializing ULP\n");
        // init_ulp_program();
    } else {
        printf("ULP wakeup, saving pulse count\n");
        // update_pulse_count();
    }

    esp_pm_config_t pm_config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        .light_sleep_enable = false
    };
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

    /* Initialize NVS partition */
    nvs_init();

    

    /* Initialize TCP/IP */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Initialize the event loop */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
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

    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, NULL));
    


    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (!provisioned) {
        app_prov_start();
    } else {
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");
        // app_prov_stop();
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &prov_event_handler, NULL));
        wifi_init_sta();
    }

    /* Wait for Wi-Fi connection */
    xEventGroupWaitBits(app_event_group, WIFI_CONNECTED_BIT, true, true, portMAX_DELAY);

    ESP_LOGI(TAG, "Connected to wifi");

    app_sntp_start();
    app_mqtt_start();

    BaseType_t xReturned;
    TaskHandle_t xHandle = NULL;

    xReturned = xTaskCreate(
        app_mqtt_task,
        "mqtt_task",
        8192,
        NULL,
        5,
        &xHandle);

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
    } else {
        ESP_LOGI(TAG, "MQTT task created successfully");
    }
}
