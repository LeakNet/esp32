#ifndef MQTT_H_
#define MQTT_H_

#include "sensors.h"
#include <stddef.h>
#include "mqtt_client.h"

#define BUFFER_SIZE 100
#define PUBLISH_INTERVAL 10000

#define MQTT_CLIENT_ID_NVS_KEY "mqtt_client_id"

extern esp_mqtt_client_handle_t client; 

void app_mqtt_init(void);
void app_mqtt_start(void);
void app_mqtt_task(void *pvParameters);

#endif