#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "sys/time.h"
#include "esp_timer.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "sensors.h"
#include "common.h"
#include "sample_batch.pb.h"

static const char* TAG = "sensors";

#define ESP_INTR_FLAG_DEFAULT 0

// Pin Definitions
#define PRESSURE_SENSOR_CHANNEL ADC_CHANNEL_5 // GPIO33 (ADC1)
#define PRESSURE_SENSOR_ADC_WIDTH ADC_BITWIDTH_12
#define PRESSURE_SENSOR_ADC_ATTENUATION ADC_ATTEN_DB_12
#define PRESSURE_SENSOR_ADC_MAX_VALUE ((1 << PRESSURE_SENSOR_ADC_WIDTH) - 1)

#define PRESSURE_RANGE 0.5  // Maximum pressure in MPa (0–0.5MPa)

/* FLOW SENSOR */
#define FLOW_SENSOR_PIN GPIO_NUM_25
#define FLOW_SENSOR_PULSES_PER_LITER 6.6
#define MAX_FLOW 30

volatile uint32_t flow_sensor_pulse_count = 0;

void IRAM_ATTR flow_sensor_isr_handler(void *arg) {
    flow_sensor_pulse_count++;
}

// Define the reference voltage
#define REFERENCE_VOLTAGE 3.3

static adc_oneshot_unit_handle_t adc1_handle;

static adc_cali_handle_t pressure_sensor_cali_handle;

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

float fround(float val) {
    int charsNeeded = 1 + snprintf(NULL, 0, "%.3f", val);
    char* buffer = malloc(charsNeeded);
    snprintf(buffer, charsNeeded, "%.3f", val);
    double result = atof(buffer);
    free(buffer);
    return result;
}

float normalize_pressure(int pressure_raw) {
    float pressure = ((float)pressure_raw / PRESSURE_SENSOR_ADC_MAX_VALUE) * PRESSURE_RANGE;
    return fround(pressure);
}

float normalize_flow(uint32_t pulse_count) {
    float flow = ((float)pulse_count / (MEASUREMENT_INTERVAL_MS / 1000.0)) / FLOW_SENSOR_PULSES_PER_LITER;
    flow = flow / MAX_FLOW;
    return fround(flow);
}

// Function to Initialize Pressure Sensor ADC
void app_sensors_init(void) {

    /* PRESSURE SENSOR */
    adc_oneshot_unit_init_cfg_t init_config_adc1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config_adc1, &adc1_handle));

    adc_oneshot_chan_cfg_t config_adc1 = {
        .atten = PRESSURE_SENSOR_ADC_ATTENUATION,
        .bitwidth = PRESSURE_SENSOR_ADC_WIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, PRESSURE_SENSOR_CHANNEL, &config_adc1));
    bool do_pressure_sensor_calibration = adc_calibration_init(ADC_UNIT_1, PRESSURE_SENSOR_CHANNEL, PRESSURE_SENSOR_ADC_ATTENUATION, &pressure_sensor_cali_handle);
    ESP_LOGI(TAG, "Pressure calibrated: %d", do_pressure_sensor_calibration);

    /* FLOW SENSOR */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << FLOW_SENSOR_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE, // Detect rising edge
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(FLOW_SENSOR_PIN, flow_sensor_isr_handler, NULL);
}

void app_sensors_read(Sample* sample) {

    /* PRESSURE */
    int raw_pressure;

    ESP_ERROR_CHECK(adc_oneshot_get_calibrated_result(adc1_handle, pressure_sensor_cali_handle, PRESSURE_SENSOR_CHANNEL, &raw_pressure));

    struct timeval tv;
    gettimeofday(&tv, NULL);

    sample->timestamp = ((uint64_t)tv.tv_sec * 1000);
    sample->pressure = normalize_pressure(raw_pressure);
    sample->flow = normalize_flow(flow_sensor_pulse_count);

    ESP_LOGI(TAG, "Timestamp: %llu, Pressure: %.4f MPa, Flow: %.4f Liter/Minute", sample->timestamp, sample->pressure, sample->flow);

    flow_sensor_pulse_count = 0;
}

static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

// Available in ESP32-C3
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
     if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}