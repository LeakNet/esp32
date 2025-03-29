#ifndef SENSORS_H_
#define SENSORS_H_

#include "esp_timer.h"

#define MEASUREMENT_INTERVAL_MS 1000

typedef struct app_sensors_sample_t {
    float pressure;
    float flow;
    uint64_t timestamp;
} app_sensors_sample_t;

void app_sensors_init(void);
void app_sensors_read(app_sensors_sample_t*);
void app_sensors_task(void *pvParameters);

#endif