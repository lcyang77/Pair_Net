#ifndef __GS_MQTT_H__
#define __GS_MQTT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"
#include "cc_event.h"

CC_EVENT_DECLARE_BASE(GS_MQTT_EVENT);

#define PUB_TOPIC_PROPERTY_POST                     "/event/property/post"

typedef enum{
    GS_MQTT_EVENT_CONNECTED = 0,
    GS_MQTT_EVENT_DISCONNECTED,
}gs_mqtt_event_t;

typedef enum {
    GS_MQTT_QOS0 = 0,
    GS_MQTT_QOS1,
    GS_MQTT_QOS2
} gs_mqtt_qos_t;

typedef void (*gs_mqtt_msg_cb_t)(const char *topic, uint8_t qos, uint8_t retain, char *data, uint32_t len);

cc_err_t gs_mqtt_init(void);

cc_err_t gs_mqtt_reset_config(void);

uint8_t gs_mqtt_connect_status(void);

cc_err_t gs_mqtt_start_connect(void);

char *gs_mqtt_generate_seq(void);

cc_err_t gs_mqtt_register_msg_cb(gs_mqtt_msg_cb_t cb);

cc_err_t gs_mqtt_subscribe(const char *topic, gs_mqtt_qos_t qos);
cc_err_t gs_mqtt_publish(const char *topic, uint8_t *data, uint16_t len, uint8_t qos, uint8_t retain);

#ifdef __cplusplus
}
#endif

#endif