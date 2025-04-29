#ifndef MQTT_H_
#define MQTT_H_

#include "sensors.h"
#include <stddef.h>
#include "mqtt_client.h"

#define MQTT_CLIENT_ID_NVS_KEY "mqtt_client_id"

#define QOS0 0
#define QOS1 1
#define QOS2 2

extern esp_mqtt_client_handle_t client; 

void app_mqtt_init(void);
void app_mqtt_start(void);
void app_mqtt_task(void *pvParameters);

#endif