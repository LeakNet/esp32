#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "common.h"
#include "mqtt.h"

static const char* TAG = "COMMON";

static esp_err_t get_nvs_handle(nvs_handle* handle) {
    esp_err_t err = nvs_open("storage", NVS_READWRITE, handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t app_nvs_save_str(const char* key, const char* value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = get_nvs_handle(&nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(nvs_handle, key, value);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting string in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error commiting changes to NVS: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);

    return err;
}

esp_err_t app_nvs_erase_key(const char* key) {
    nvs_handle_t nvs_handle;
    esp_err_t err = get_nvs_handle(&nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_erase_key(nvs_handle, key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error saving to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    } else {
        nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t app_nvs_get_str(const char *key, char *out_value, size_t *length) {
    nvs_handle_t nvs_handle;
    esp_err_t err = get_nvs_handle(&nvs_handle);
    if (err != ESP_OK) return err;

    err = nvs_get_str(nvs_handle, key, out_value, length);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting key from NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

esp_err_t app_get_device_id(char* out, size_t* length) {
    return app_nvs_get_str(MQTT_CLIENT_ID_NVS_KEY, out, length);
}