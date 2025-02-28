#ifndef SENSORS_H_
#define SENSORS_H_

#include "esp_timer.h"

#define MEASUREMENT_INTERVAL_MS 1000

typedef struct {
    float pressure;
    float flow;
    uint64_t timestamp;
} sensors_data_point_t;

void app_sensors_init(void);
void app_sensors_read(sensors_data_point_t*);

#endif