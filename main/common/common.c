#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "common.h"

static const char* TAG = "COMMON";

esp_err_t app_nvs_save_str(const char* key, const char* value) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, key, value);
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


esp_err_t app_nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(nvs_get_str(nvs_handle, key, out_value, length));

    nvs_close(nvs_handle);
    return ESP_OK;
}