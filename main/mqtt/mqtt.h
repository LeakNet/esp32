#ifndef MQTT_H_
#define MQTT_H_

#include "sensors.h"
#include <stddef.h>

#define BUFFER_SIZE 100
#define PUBLISH_INTERVAL 10000

void app_mqtt_init(void);
void app_mqtt_start(void);
void app_mqtt_send_sensor_data(sensors_data_point_t*, size_t);

#endif