#ifndef SENSORS_H_
#define SENSORS_H_

#include "esp_timer.h"
#include "sample_batch.pb.h"

#define MEASUREMENT_INTERVAL_MS 1000

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

void app_sensors_init(void);
void app_sensors_read(Sample*);
// void app_sensors_read(app_sensors_sample_t*);

#endif