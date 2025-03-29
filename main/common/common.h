#ifndef COMMON_H
#define COMMON_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs.h"

#define NVS_USER_ID_KEY "user-id"

static const int WIFI_CONNECTED_BIT = BIT0;
static const int MQTT_CONNECTED_BIT = BIT1;
static const int TIME_SYNCED_BIT = BIT2;

extern EventGroupHandle_t app_event_group;

esp_err_t app_nvs_save_str(const char* key, const char* value);
esp_err_t app_nvs_get_str(const char *key, char *out_value, size_t *length);
esp_err_t app_nvs_erase_key(const char* key);

esp_err_t app_get_device_id(char* out, size_t* length);

#endif