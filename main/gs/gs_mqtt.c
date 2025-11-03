#include "gs_mqtt.h"

#include <stdlib.h>
#include <string.h>

#include "cc_log.h"
#include "cc_http.h"

#include "gs_wifi.h"
#include "gs_bind.h"
#include "gs_device.h"

#include "cc_list.h"
#include "cc_tmr_task.h"

#include "cc_hal_sys.h"
#include "cc_hal_os.h"
#include "cc_hal_kvs.h"
#include "cc_hal_network.h"

#include "cJSON.h"

static char *TAG = "gs_mqtt";

#define MQTT_KVS_KEY    "gs_mqtt"

CC_EVENT_DEFINE_BASE(GS_MQTT_EVENT);

#define MSG_LEN_MAX             (512)
#define GS_MQTT_HOST_BUF_MAX_LEN    33

typedef struct{
    uint8_t host[GS_MQTT_HOST_BUF_MAX_LEN];
    char host_len;
    uint16_t port;
}_mqtt_config_t;

static _mqtt_config_t g_mqtt_config = {0};

static void *g_mqtt_handle = NULL;
static cc_list_node *g_mqtt_msg_cb_list = NULL;

static char g_mqtt_topic_prefix[52] = "";

static char g_mqtt_connect_status = 0;

typedef struct{
	char *topic;
	uint8_t *data;
	uint16_t len;
	uint8_t qos;
	uint8_t retain;
    uint32_t tm;
}_msg_ctx_t;

static cc_list_node *g_msg_list = NULL;

static cc_err_t __mqtt_get_host(void);

static cc_err_t __mqtt_save_config(void){
    cc_err_t err = CC_FAIL;

    err = cc_hal_kvs_set(MQTT_KVS_KEY, &g_mqtt_config, sizeof(_mqtt_config_t));
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
        return err;
    }   

    CC_LOGI(TAG, "mqtt_save_config host: %.*s port: %d", g_mqtt_config.host_len, g_mqtt_config.host, g_mqtt_config.port);

    return CC_OK;
}

cc_err_t gs_mqtt_reset_config(void){
    cc_err_t err = CC_FAIL;

    memset(&g_mqtt_config, 0, sizeof(g_mqtt_config));
    err = cc_hal_kvs_set(MQTT_KVS_KEY, &g_mqtt_config, sizeof(_mqtt_config_t));
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
        return err;
    }   

    CC_LOGI(TAG, "gs_mqtt_reset_config");

    return CC_OK;
}

static void __mqtt_birth(void){
    char msg[128];
    char sw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
    char hw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";

    gs_device_get_version(sw_version, hw_version);

    sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0002\",\"seq_no\":\"%s\"}", sw_version, gs_mqtt_generate_seq());
    gs_mqtt_publish(PUB_TOPIC_PROPERTY_POST, (uint8_t *)msg, strlen(msg), GS_MQTT_QOS0, 0);

    sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0003\",\"type\":\"02\",\"data\":\"%d\",\"seq_no\":\"%s\"}", sw_version, cc_hal_wifi_get_connect_rssi(), gs_mqtt_generate_seq());
    gs_mqtt_publish(PUB_TOPIC_PROPERTY_POST, (uint8_t *)msg, strlen(msg), GS_MQTT_QOS0, 0);    
}

static void __event_handler(void* handler_args, cc_event_base_t base_event, int32_t id, void* event_data){

    CC_LOGD(TAG, "__event_handler: %s event: %d", base_event, id);

    static uint8_t in_bind = 0;

    if(base_event == GS_WIFI_EVENT){
        switch (id)
        {
        case GS_WIFI_EVENT_STA_CONNECTED:

            break;
        case GS_WIFI_EVENT_STA_GOT_IP:
            if(g_mqtt_config.host_len > 0 && g_mqtt_config.host_len < GS_MQTT_HOST_BUF_MAX_LEN){
                if(gs_bind_get_bind_status() || gs_bind_has_cfg_mode()){
                    gs_mqtt_start_connect();
                }
            }else{
                if(in_bind){
                    char product_key[GS_PRODUCT_KEY_BUF_MAX_LEN];
                    if(gs_device_get_product_key(product_key) == CC_OK){
                        __mqtt_get_host();
                    }else{
                        gs_device_get_license();
                    }
                }
            }
            break;
        default:
            break;
        }
    }else if(base_event == GS_BIND_EVENT){
        switch (id)
        {
        case GS_BIND_EVENT_START:
            in_bind = 1;
            memset(&g_mqtt_config, 0, sizeof(g_mqtt_config));
            break;
        case GS_BIND_EVENT_SUCCESS:
            if(gs_bind_get_bind_status()){
                __mqtt_birth();
            }
        case GS_BIND_EVENT_FAIL:
            in_bind = 0;
            break;
        default:
            break;
        }
    }else if(base_event == GS_DEVICE_EVENT){
        switch (id)
        {
        case GS_DEVICE_EVENT_GET_LICENSE_SUCCESS:
            if(g_mqtt_config.host_len > 0 && g_mqtt_config.host_len < GS_MQTT_HOST_BUF_MAX_LEN){
                gs_mqtt_start_connect();
            }else{
                __mqtt_get_host();
            }
            break;
        case GS_DEVICE_EVENT_GET_LICENSE_FAIL:
            break;
        default:
            break;
        }
    }else if(base_event == GS_MQTT_EVENT){
        switch (id)
        {
        case GS_MQTT_EVENT_CONNECTED:
            if(gs_bind_get_bind_status()){
                __mqtt_birth();
            }
            break;
        default:
            break;
        }
    }
}

static void __get_mqtt_host_http_cb(void *arg, int resp_code, uint8_t *buf, uint16_t len){
    CC_LOGD(TAG, "_get_list_http_cb recv len: %d  data: %.*s", len, len, buf);

    cJSON *root_obj = NULL, *ip_obj = NULL, *port_obj = NULL;

    if(resp_code == -1){
        goto exit;
    }else if(resp_code == 0){
        return;
    }

    if(resp_code != 200){
        CC_LOGE(TAG, "resp_code: %d", resp_code);
        goto exit;
    }

    buf[len] = '\0';
    root_obj = cJSON_Parse((char *)buf);
    if(root_obj == NULL){
        CC_LOGE(TAG, "cJSON_Parse error");
        goto exit;
    }

    ip_obj = cJSON_GetObjectItem(root_obj, "mqttip");
    port_obj = cJSON_GetObjectItem(root_obj, "port");
    if(NULL == ip_obj || NULL == port_obj 
            || ip_obj->type != cJSON_String || port_obj->type != cJSON_String){
        CC_LOGE(TAG, "data error");
        goto exit;      
    }

    strcpy((char *)g_mqtt_config.host, ip_obj->valuestring);
    g_mqtt_config.host_len = strlen((char *)g_mqtt_config.host);
    g_mqtt_config.port = atoi(port_obj->valuestring);

    CC_LOGI(TAG, "get mqtt host: %s  port: %d", g_mqtt_config.host, g_mqtt_config.port);

exit:
    if(g_mqtt_config.host_len == 0){
        strcpy((char *)g_mqtt_config.host, "120.25.207.32");
        g_mqtt_config.host_len = strlen("120.25.207.32");
        g_mqtt_config.port = 1883;
    }

    __mqtt_save_config();

    char product_key[GS_PRODUCT_KEY_BUF_MAX_LEN];
    if(gs_device_get_product_key(product_key) == CC_OK){
        gs_mqtt_start_connect();
    }

    if(root_obj){
        cJSON_Delete(root_obj);
    }
}

static cc_err_t __mqtt_get_host(void){

    char url_buf[128] = "";

    char device_name[GS_DEVICE_NAME_BUF_MAX_LEN] = "";
    gs_device_get_device_name(device_name);

    CC_LOGI(TAG, "get mqtt host start");
    //TODO
    sprintf(url_buf, "http://gaoshi.wdaoyun.cn/mqtt/getMqtt.php?device_name=%s", device_name);
    cc_http_simple_get(url_buf, __get_mqtt_host_http_cb, NULL);

    return CC_OK;
}

static cc_err_t __connect_cb(void *arg){
    g_mqtt_connect_status = 1;
    cc_event_post(GS_MQTT_EVENT, GS_MQTT_EVENT_CONNECTED, NULL, 0);
    return CC_OK;
}

static cc_err_t __msg_cb(void *arg, char *topic, char *data, uint16_t len, cc_mqtt_qos_t qos, uint8_t retain){
    CC_LOGD(TAG, "topic: %s, data: %.*s", topic, len, data);
    if(g_mqtt_msg_cb_list){
        if(strstr(topic, g_mqtt_topic_prefix)){
            char topic[128] = "";
            uint8_t prefix_len = strlen(g_mqtt_topic_prefix);
            memcpy(topic, topic + prefix_len, strlen(topic) - prefix_len);
            topic[strlen(topic) - prefix_len] = '\0';
            cc_list_node *node = g_mqtt_msg_cb_list;
            while (node){
                gs_mqtt_msg_cb_t msg_cb = node->data;
                if(msg_cb){
                    msg_cb(topic, qos, retain, data, len);
                }
                node = node->next;
            }
        }
    }
    return CC_OK;
}

static cc_err_t __disconnect_cb(void *arg){
    g_mqtt_connect_status = 0;
    cc_event_post(GS_MQTT_EVENT, GS_MQTT_EVENT_DISCONNECTED, NULL, 0);
    return CC_OK;
}

char *gs_mqtt_generate_seq(void){
    static char seq[33] = {0};
    for (uint16_t i = 0; i < 32; i++){   
        seq[i] = '0' + (uint8_t)cc_hal_sys_get_rand(10);
    }
    seq[32] = '\0';
    return seq;
}

cc_err_t gs_mqtt_subscribe(const char *topic, gs_mqtt_qos_t qos){
    char full_topic[128] = "";

    if(NULL == g_mqtt_handle){
        CC_LOGE(TAG, "mqtt not connect");
        return CC_FAIL;
    }
    snprintf(full_topic, 128, "%s%s", g_mqtt_topic_prefix, topic);
    cc_hal_mqtt_subscribe((cc_mqtt_t *)g_mqtt_handle, full_topic, qos);
    return CC_OK;
}

void __mqtt_publish_task(uint32_t interval, void *arg){
    
    cc_list_node *node = NULL;

    uint32_t now_tm = cc_hal_sys_get_ms();

    node = g_msg_list;
    while(node){
        _msg_ctx_t* msg_ctx = node->data;

        if(g_mqtt_connect_status){
            gs_mqtt_publish(msg_ctx->topic, msg_ctx->data, msg_ctx->len, msg_ctx->qos, msg_ctx->retain);
        }

        if(now_tm - msg_ctx->tm > 10000 || g_mqtt_connect_status){
            cc_list_node *del_node = node;
            node = node->next;
            cc_hal_sys_free(msg_ctx->topic);
            cc_hal_sys_free(msg_ctx->data);
            cc_hal_sys_free(msg_ctx);
            cc_list_remove(&g_msg_list, del_node);
        }else{
            node = node->next;
        }
    }
    
    if(g_msg_list == NULL){
        cc_tmr_task_delete(__mqtt_publish_task);
    }
}

cc_err_t gs_mqtt_publish(const char *topic, uint8_t *data, uint16_t len, uint8_t qos, uint8_t retain){
    char full_topic[128] = "";

    if(g_mqtt_connect_status == 0){
        CC_LOGW(TAG, "gs_mqtt not connect delay publish topic: %s data: %.*s", topic, len, data);
        _msg_ctx_t* msg_ctx = cc_hal_sys_malloc(sizeof(_msg_ctx_t));
        if(msg_ctx == NULL){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            return CC_ERR_NO_MEM;
        }
        msg_ctx->topic = cc_hal_sys_malloc(strlen(topic) + 1);
        msg_ctx->data = cc_hal_sys_malloc(len);
        if(msg_ctx->topic == NULL || msg_ctx->data == NULL){
            cc_hal_sys_free(msg_ctx);
            if(msg_ctx->topic){
                cc_hal_sys_free(msg_ctx->topic);
            }
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            return CC_ERR_NO_MEM;
        }
        strcpy(msg_ctx->topic, topic);
        
        memcpy(msg_ctx->data, data, len);
        msg_ctx->len = len;
        msg_ctx->qos = qos;
        msg_ctx->retain = retain;
        msg_ctx->tm = cc_hal_sys_get_ms();

        if(g_msg_list == NULL){
            g_msg_list = cc_list_create(msg_ctx);
            cc_tmr_task_create(__mqtt_publish_task, 100, NULL);
        }else{
            cc_list_insert_end(g_msg_list, msg_ctx);
        }
    }else{
        CC_LOGD(TAG, "gs_mqtt_publish topic: %s data: %.*s", topic, len, data);
    
        if(NULL == g_mqtt_handle){
            CC_LOGE(TAG, "mqtt not connect");
            return CC_FAIL;
        }
        
        snprintf(full_topic, 128, "%s%s", g_mqtt_topic_prefix, topic);
        cc_hal_mqtt_publish((cc_mqtt_t *)g_mqtt_handle, full_topic, (char *)data, len, qos, retain);
    }

    return CC_OK;
}

cc_err_t gs_mqtt_register_msg_cb(gs_mqtt_msg_cb_t cb){

    if(NULL == g_mqtt_msg_cb_list){
        g_mqtt_msg_cb_list = cc_list_create(cb);
    }else{
        cc_list_insert_end(g_mqtt_msg_cb_list, cb);
    }
    
    return CC_OK;
}

uint8_t gs_mqtt_connect_status(void){
    return g_mqtt_connect_status;
}

cc_err_t gs_mqtt_start_connect(void){
    char product_key[GS_PRODUCT_KEY_BUF_MAX_LEN];
    if(gs_device_get_product_key(product_key) != CC_OK){
        // return CC_FAIL;
        gs_device_get_license();
        return CC_OK;
    }
    if(g_mqtt_config.host_len == 0 || strlen((char *)g_mqtt_config.host) == 0){
        __mqtt_get_host();
        return CC_OK;
    }
    if(g_mqtt_handle == NULL){
        static char device_name[GS_DEVICE_NAME_BUF_MAX_LEN] = "";
        static char product_key[GS_PRODUCT_KEY_BUF_MAX_LEN] = "";
        static char device_secret[GS_DEVICE_SECRET_BUF_MAX_LEN] = "";

        if(gs_device_get_product_key(product_key) != CC_OK){
            CC_LOGE_CODE(TAG, CC_FAIL);
            return CC_FAIL;
        }
        if(gs_device_get_device_name(device_name) != CC_OK){
            CC_LOGE_CODE(TAG, CC_FAIL);
            return CC_FAIL;
        }
        if(gs_device_get_device_secret(device_secret) != CC_OK){
            CC_LOGE_CODE(TAG, CC_FAIL);
            return CC_FAIL;
        }

        sprintf(g_mqtt_topic_prefix, "/sys/%s/%s", product_key, device_name);

        cc_mqtt_t *mqtt = cc_hal_sys_malloc(sizeof(cc_mqtt_t));
        if(NULL == mqtt){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            return CC_ERR_NO_MEM;
        }

        mqtt->host = (char *)g_mqtt_config.host;
        mqtt->port = g_mqtt_config.port;

        mqtt->client_id = device_name;
        mqtt->username = device_name;
        mqtt->password = device_secret;

        mqtt->connect_cb = __connect_cb;
        mqtt->disconnect_cb = __disconnect_cb;
        mqtt->msg_cb = __msg_cb;

        g_mqtt_handle = mqtt;

        if(cc_hal_mqtt_create(mqtt) != CC_OK){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            g_mqtt_handle = NULL;
            cc_hal_sys_free(mqtt);
            return CC_ERR_NO_MEM;
        }

    }
    return CC_OK;
}

cc_err_t gs_mqtt_init(void){

    size_t len = 0;
    
    memset(&g_mqtt_config, 0, sizeof(g_mqtt_config));

    len = sizeof(_mqtt_config_t);
    if(cc_hal_kvs_get(MQTT_KVS_KEY, &g_mqtt_config, &len) != CC_OK 
            || g_mqtt_config.host_len == 0 || g_mqtt_config.host_len > GS_MQTT_HOST_BUF_MAX_LEN){
        
        CC_LOGI(TAG, "mqtt not config");

        strcpy((char *)g_mqtt_config.host, "");
        g_mqtt_config.host_len = 0;
        g_mqtt_config.port = 1883;
    }else{
        CC_LOGI(TAG, "read mqtt config %s:%d", g_mqtt_config.host, g_mqtt_config.port);
    }

    cc_event_register_handler(GS_WIFI_EVENT, __event_handler);
    cc_event_register_handler(GS_BIND_EVENT, __event_handler);
    cc_event_register_handler(GS_DEVICE_EVENT, __event_handler);
    cc_event_register_handler(GS_MQTT_EVENT, __event_handler);
    
    return CC_OK;
}