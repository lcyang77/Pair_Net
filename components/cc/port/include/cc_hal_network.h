#ifndef __CC_HAL_NETWORK_H__
#define __CC_HAL_NETWORK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

typedef struct {
    uint8_t ip_type;
    union {
        struct {
            uint32_t addr;  
        }ip_v4;
        struct {
            uint32_t addr[4];  
        }ip_v6;
    }addr;
    uint16_t port;
}cc_addr_t;

enum{
    CC_HTTP_METHOD_GET = 0,
    CC_HTTP_METHOD_POST,
};

enum{
    CC_HTTP_EVENT_ERROR = 0,        //data: (uint8_t *)err
    CC_HTTP_EVENT_DNS_GET_IP,       //data: ip[4]
    CC_HTTP_EVENT_CONNECTED,
    CC_HTTP_EVENT_HEADER_SENTED,    //todo
    CC_HTTP_EVENT_RECV_HEADER,      //todo
    CC_HTTP_EVENT_RECV_DATA,        //todo
    CC_HTTP_EVENT_FINISH,           //data: (uint8_t *)response_code
};

/** @brief   This structure defines the httpclient_t structure   */
typedef struct {
    char *url;                /**< hTTP or HTTPS port        */
    uint8_t method;
    char *auth_user;                /**< username for basic authentication         */
    char *auth_password;            /**< password for basic authentication         */
    char *header;                   /**< request custom header     */
    uint8_t *post_buf;              /**< user data to be posted. */
    uint16_t post_buf_len;            /**< post data length. */
    char *resp_buf;          /**< buffer to store the response body data. */
    uint16_t resp_buf_len;        /**< response body buffer length. */
    uint16_t resp_len;          
    void *arg;
    void (* event_cb)(void *arg, uint8_t event, void *data);
    cc_err_t (* connect_cb)(void *arg);
    cc_err_t (* recv_cb)(void *arg, uint8_t *, uint16_t); 
    cc_err_t (* close_cb)(void *arg, uint16_t); 
} cc_http_t;

cc_err_t cc_hal_http_connect(cc_http_t *client);

enum{
    CC_IP_TYPE_IPv4 = 0,
    CC_IP_TYPE_IPv6 = 1,
    CC_IP_TYPE_ANY  = 2,
};

typedef struct {
    char *host;
    uint16_t port;
    uint8_t ip_type;
    uint8_t timeout_ms;
    uint8_t nonblock;
    uint8_t use_send_rbuffer;
    uint16_t send_timeout;
    uint16_t poll_ms;
    void *arg;
    cc_err_t (*poll_cb)(void *arg);
    cc_err_t (*connect_cb)(void *arg);
    cc_err_t (*recv_cb)(void *arg, uint8_t *, uint16_t); 
    cc_err_t (*disconnect_cb)(void *arg);
} cc_tcpc_t;

cc_err_t cc_hal_tcpc_create(cc_tcpc_t *client);
cc_err_t cc_hal_tcpc_delete(cc_tcpc_t *client);
cc_err_t cc_hal_tcpc_reconnect(cc_tcpc_t *client);
cc_err_t cc_hal_tcpc_send(cc_tcpc_t *client, char *data, uint16_t len);


typedef struct {
    char *host;
    uint16_t port;
    uint8_t ip_type;
    uint8_t max_client;
    uint8_t timeout_ms;
    uint16_t poll_ms;
    void *arg;
    cc_err_t (*poll_cb)(void *arg);
    cc_err_t (*connect_cb)(void *arg, uint8_t);
    cc_err_t (*recv_cb)(void *arg, uint8_t, uint8_t *, uint16_t); 
    cc_err_t (*disconnect_cb)(void *arg, uint8_t); 
} cc_tcps_t;

cc_err_t cc_hal_tcps_create(cc_tcps_t *server);
cc_err_t cc_hal_tcps_delete(cc_tcps_t *server);
cc_err_t cc_hal_tcps_send(cc_tcps_t *server, uint8_t client_id, char *data, uint16_t len);

#define cc_udp_addr_t   cc_addr_t

typedef struct {
    char *host;
    uint16_t port;
    uint8_t ip_type;
    uint16_t poll_ms;
    void *arg;
    cc_err_t (*poll_cb)(void *arg);
    cc_err_t (*recv_cb)(void *arg, cc_udp_addr_t, uint8_t *, uint16_t); 
} cc_udp_t;

cc_err_t cc_hal_udp_create(cc_udp_t *udp);
cc_err_t cc_hal_udp_listen(cc_udp_t *udp);
cc_err_t cc_hal_udp_delete(cc_udp_t *udp);
cc_err_t cc_hal_udp_send(cc_udp_t *udp, cc_udp_addr_t addr, uint8_t *data, uint16_t len);


typedef enum {
    CC_MQTT_QOS0 = 0,
    CC_MQTT_QOS1,
    CC_MQTT_QOS2
} cc_mqtt_qos_t;

typedef struct {
    char *host;
    uint16_t port;
    char *client_id;
    char *username;
    char *password;
    void *arg;
    cc_err_t (*connect_cb)(void *arg);
    cc_err_t (*msg_cb)(void *arg, char *topic, char *data, uint16_t len, cc_mqtt_qos_t qos, uint8_t retain); 
    cc_err_t (*disconnect_cb)(void *arg);
} cc_mqtt_t;

cc_err_t cc_hal_mqtt_create(cc_mqtt_t *mqtt);
cc_err_t cc_hal_mqtt_delete(cc_mqtt_t *mqtt);
cc_err_t cc_hal_mqtt_subscribe(cc_mqtt_t *mqtt, char *topic, cc_mqtt_qos_t qos);
cc_err_t cc_hal_mqtt_publish(cc_mqtt_t *mqtt, char *topic, char *msg, uint16_t len, cc_mqtt_qos_t qos, uint8_t retain);


#ifdef __cplusplus
}
#endif

#endif