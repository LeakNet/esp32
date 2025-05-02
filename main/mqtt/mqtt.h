#ifndef MQTT_H_
#define MQTT_H_

#define QOS0 0
#define QOS1 1
#define QOS2 2

#define NO_RETAIN 0
#define RETAIN 1

void app_mqtt_init(void);
void app_mqtt_start(void);
void app_mqtt_task(void *pvParameters);

#endif