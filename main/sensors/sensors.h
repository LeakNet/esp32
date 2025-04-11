#ifndef SENSORS_H_
#define SENSORS_H_

#include "esp_timer.h"
#include "sample_batch.pb.h"

#define MEASUREMENT_INTERVAL_MS 1000

void app_sensors_init(void);
void app_sensors_read(Sample*);
// void app_sensors_read(app_sensors_sample_t*);

#endif