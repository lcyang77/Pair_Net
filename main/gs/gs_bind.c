#include "gs_bind.h"

#include <string.h>

#include "cc_log.h"
#include "cc_timer.h"
#include "cc_tmr_task.h"
#include "cc_hal_network.h"
#include "cc_hal_ble.h"
#include "cc_hal_sys.h"
#include "cc_hal_wifi.h"
#include "cc_hal_kvs.h"

#include "gs_wifi.h"
#include "gs_mqtt.h"
#include "gs_device.h"

#include "captive_portal.h"

#include "cJSON.h"

static char *TAG = "gs_bind";

CC_EVENT_DEFINE_BASE(GS_BIND_EVENT);

#define GS_BIND_KVS_KEY    "gs_bind"
#define GS_BIND_BOOT_CGF_KVS_KEY    "gs_bind_boot"

static cc_tcps_t g_tcp_server = {0};

static uint8_t g_curr_cfg_mode = GS_BIND_CFG_MODE_NULL;
static uint8_t g_curr_bind_connect_mode = GS_BIND_CFG_MODE_NULL;
static cc_timer_handle_t g_sta_connect_timer_handle = NULL;
static cc_timer_handle_t g_bind_timeout_timer_handle = NULL;

static uint32_t g_bind_status = 0;
static uint32_t g_boot_auto_start_cfg = GS_BIND_CFG_MODE_NULL;

static void __ap_bind_cfg_ap_server_start(void);

static void __cfg_event_handler(void* handler_args, cc_event_base_t base_event, int32_t id, void* event_data);

static void __ble_deinit(void *arg){
    cc_hal_ble_deinit();
}

static void __timer_cb_for_wifi_start_connect(void *arg){
    cc_hal_tcps_delete(&g_tcp_server);
    g_tcp_server.port = 0;
    gs_wifi_ap_stap();

    captive_portal_stop();

    gs_wifi_sta_start_connect();
}

static void __timer_cb_for_bind_timeout(void *arg){
    g_curr_cfg_mode = GS_BIND_CFG_MODE_NULL;
    g_curr_bind_connect_mode = GS_BIND_CFG_MODE_NULL;

    CC_LOGI(TAG, "bind timeout");

    if(g_tcp_server.port != 0){
        gs_wifi_ap_stap();
        captive_portal_stop();
        cc_hal_tcps_delete(&g_tcp_server);
    }

    gs_bind_err_t err = GS_BIND_ERR_TIMEOUT;
    cc_event_post(GS_BIND_EVENT, GS_BIND_EVENT_FAIL, &err, sizeof(err));

    cc_event_unregister_handler(GS_WIFI_EVENT, __cfg_event_handler);
    cc_event_unregister_handler(GS_MQTT_EVENT, __cfg_event_handler);
}

static void __save_bind_status(void){
    cc_err_t err = CC_FAIL;
    err = cc_hal_kvs_set(GS_BIND_KVS_KEY, &g_bind_status, sizeof(g_bind_status));
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
    } 
}

//{ssid:xxxxxx,password:xxxxxxxx,token:xxxxxxxx}
static cc_err_t __parse_ble_bind_info(char *data, char len){
    cJSON *root_obj = NULL, *ssid_obj = NULL, *password_obj = NULL, *token_obj = NULL;

    if(NULL == data || len == 0){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return CC_ERR_INVALID_ARG;
    }

    CC_LOGD(TAG, "__parse_ble_bind_info: %.*s", len, data);

    root_obj = cJSON_Parse((const char *)data);
    if(NULL == root_obj){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return CC_ERR_INVALID_ARG;
    }

    ssid_obj = cJSON_GetObjectItem(root_obj, "ssid");
    password_obj = cJSON_GetObjectItem(root_obj, "password");
    token_obj = cJSON_GetObjectItem(root_obj, "token");

    if(NULL == ssid_obj || NULL == password_obj || NULL == token_obj
        || ssid_obj->type != cJSON_String || password_obj->type != cJSON_String || token_obj->type != cJSON_String){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        cJSON_Delete(root_obj);
        return CC_ERR_INVALID_ARG;
    }
    
    CC_LOGD(TAG, "__parse_ble_bind_info ssid: %s password: %s token: %s", ssid_obj->valuestring, password_obj->valuestring, token_obj->valuestring);
    
    g_curr_bind_connect_mode = GS_BIND_CFG_MODE_BLE;

    gs_device_save_token(token_obj->valuestring);

    gs_wifi_config_t wifi_config = {0};
    strcpy((char *)wifi_config.ssid, ssid_obj->valuestring);
    wifi_config.ssid_len = strlen(ssid_obj->valuestring);
    strcpy((char *)wifi_config.password, password_obj->valuestring);
    wifi_config.password_len = strlen(password_obj->valuestring);
    gs_wifi_sta_save_config(&wifi_config);

    if(g_sta_connect_timer_handle == NULL){
        cc_timer_config_t timer_config = {
            .arg = NULL,
            .callback = __timer_cb_for_wifi_start_connect,
            .type = CC_TIMER_TYPE_SW
        };

        g_sta_connect_timer_handle = cc_timer_create(&timer_config);
    }
    if(g_sta_connect_timer_handle){
        cc_timer_start_once(g_sta_connect_timer_handle, CC_TIMMER_MS(500));
    } 

    cJSON_Delete(root_obj);

    return CC_OK;
}

static cc_err_t __parse_ap_bind_info(char *data, uint16_t len, char *ret_data, uint16_t ret_data_len, uint16_t *ret_len){
    cJSON *root_obj = NULL, *cmd_obj = NULL, *ssid_obj = NULL, *password_obj = NULL, *token_obj = NULL;
    uint8_t status = 1;

    *ret_len = 0;
     
    if(NULL == data || len == 0){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return CC_ERR_INVALID_ARG;
    }

    CC_LOGD(TAG, "__parse_ap_bind_info: %.*s", len, data);

    root_obj = cJSON_Parse((const char *)data);
    if(NULL == root_obj){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return CC_ERR_INVALID_ARG;
    }

    cmd_obj = cJSON_GetObjectItem(root_obj, "cmd_type");
    if(NULL == cmd_obj || cmd_obj->type != cJSON_String){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        cJSON_Delete(root_obj);
        return CC_ERR_INVALID_ARG;
    }

    if(strcmp(cmd_obj->valuestring, "1") == 0){
        ssid_obj = cJSON_GetObjectItem(root_obj, "ssid");
        password_obj = cJSON_GetObjectItem(root_obj, "password");
        token_obj = cJSON_GetObjectItem(root_obj, "token");
        if(NULL == ssid_obj || NULL == password_obj || NULL == token_obj
                || ssid_obj->type != cJSON_String || password_obj->type != cJSON_String || token_obj->type != cJSON_String){
            CC_LOGE(TAG, "not found");
            status = 1;
        }else{
            CC_LOGD(TAG, "__parse_ap_bind_info ssid: %s password: %s token: %s", ssid_obj->valuestring, password_obj->valuestring, token_obj->valuestring);

            status = 0;

            gs_device_save_token(token_obj->valuestring);

            gs_wifi_config_t wifi_config = {0};
            strcpy((char *)wifi_config.ssid, ssid_obj->valuestring);
            wifi_config.ssid_len = strlen(ssid_obj->valuestring);
            strcpy((char *)wifi_config.password, password_obj->valuestring);
            wifi_config.password_len = strlen(password_obj->valuestring);
            gs_wifi_sta_save_config(&wifi_config);

            if(g_sta_connect_timer_handle == NULL){
                cc_timer_config_t timer_config = {
                    .arg = NULL,
                    .callback = __timer_cb_for_wifi_start_connect,
                    .type = CC_TIMER_TYPE_SW
                };

                g_sta_connect_timer_handle = cc_timer_create(&timer_config);
            }
            if(g_sta_connect_timer_handle){
                cc_timer_start_once(g_sta_connect_timer_handle, CC_TIMMER_MS(500));
            } 
        }

        g_curr_bind_connect_mode = GS_BIND_CFG_MODE_AP;

        char product_key[GS_PRODUCT_KEY_BUF_MAX_LEN] = "";
        char device_name[GS_DEVICE_NAME_BUF_MAX_LEN] = "";
        gs_device_get_product_key(product_key);
        gs_device_get_device_name(device_name);
        char sw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
        char hw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
        gs_device_get_version(sw_version, hw_version);
        snprintf(ret_data, ret_data_len, "{\"cmd_type\":2,\"product_key\":\"%s\",\"device_name\":\"%s\",\"status\":\"%d\", \"ver\":\"%s\"}", product_key, device_name, (status == CC_OK)?0:1, sw_version);
        *ret_len = strlen(ret_data);
    }else if(strcmp(cmd_obj->valuestring, "3") == 0){
        token_obj = cJSON_GetObjectItem(root_obj, "token");
        if(NULL == token_obj || token_obj->type != cJSON_String){
            CC_LOGE(TAG, "not found");
            status = 1;
        }else{
            status = 0;
        }

        char ap_list_str[(33+2+1)*12+1] = "";
        if(status == 0){
            uint8_t ap_num = 0;
            ap_num = gs_wifi_get_scan_ap_result_numbers();
            if(ap_num > 0){
                if(ap_num > 12){
                    ap_num = 12;
                }
                gs_wifi_ap_info_t *ap_list = cc_hal_sys_malloc(sizeof(gs_wifi_ap_info_t) * ap_num);
                if (ap_num > 0 && ap_list == NULL){
                    CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
                    status = 1;
                }else{
                    gs_wifi_get_scan_ap_result(ap_list, ap_num);
                    uint8_t frist_is_json = 0;
                    for (size_t i = 0; i < ap_num; i++){
                        if(ap_list[i].ssid_len > 0){
                            if(frist_is_json){
                                strcat(ap_list_str, ",");
                            }else{
                                frist_is_json = 1;
                            }
                            char ap_name[33+2] = ""; 
                            sprintf(ap_name, "\"%.*s\"", ap_list[i].ssid_len, (char *)ap_list[i].ssid);
                            strcat(ap_list_str, ap_name);
                        }
                    }
                    cc_hal_sys_free(ap_list);
                }
            }else{
                status = 1;
            }
        }

        snprintf(ret_data, ret_data_len, "{\"cmd_type\":4,\"wifi_list\":[%s],\"status\":\"%d\"}", ap_list_str, (status == CC_OK)?0:1);
        *ret_len = strlen(ret_data);
    }

    cJSON_Delete(root_obj);

    return CC_OK;
}

static void __cfg_event_handler(void* handler_args, cc_event_base_t base_event, int32_t id, void* event_data){

    if(base_event == GS_WIFI_EVENT){
        CC_LOGD(TAG, "__wifi_handler: event: %d", id);
        switch (id)
        {
        case GS_WIFI_EVENT_STA_CONNECTED:
            if(g_sta_connect_timer_handle){
                cc_timer_delete(&g_sta_connect_timer_handle);
            }
            break;
        case GS_WIFI_EVENT_STA_GOT_IP:
            break;
        case GS_WIFI_EVENT_SCAN_DONE:
            if(g_bind_timeout_timer_handle != NULL){
                __ap_bind_cfg_ap_server_start(); 
            }
            break;
        default:
            break;
        }
    }else if(base_event == GS_MQTT_EVENT){
        CC_LOGD(TAG, "__mqtt_handler: event: %d", id);
        switch (id)
        {
        case GS_MQTT_EVENT_CONNECTED:
            if(g_curr_bind_connect_mode != GS_BIND_CFG_MODE_NULL){

                char msg[256] = "";
                char token[GS_TOKEN_BUF_MAX_LEN] = "";
                char sw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
                char hw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";

                gs_device_get_version(sw_version, hw_version);

                gs_device_get_token(token);
                
                if(g_curr_bind_connect_mode & GS_BIND_CFG_MODE_BLE){
                    g_curr_bind_connect_mode -= GS_BIND_CFG_MODE_BLE;

                    sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0001\",\"sta\":\"00\",\"token\":\"%s\",\"seq_no\":\"%s\"}", sw_version, token, gs_mqtt_generate_seq());
                    CC_LOGD(TAG, "success_info: %s", msg);
                    cc_hal_ble_send((uint8_t *)msg, strlen(msg));
                    
                }
                if(g_curr_bind_connect_mode & GS_BIND_CFG_MODE_AP){
                    g_curr_bind_connect_mode -= GS_BIND_CFG_MODE_AP;
                }

                sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0001\",\"sta\":\"00\",\"token\":\"%s\",\"seq_no\":\"%s\"}", sw_version, token, gs_mqtt_generate_seq());
                gs_mqtt_publish(PUB_TOPIC_PROPERTY_POST, (uint8_t *)msg, strlen(msg), 0, 0);

                sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0003\",\"type\":\"03\",\"data\":\"%s\",\"seq_no\":\"%s\"}", sw_version, sw_version, gs_mqtt_generate_seq());
                gs_mqtt_publish(PUB_TOPIC_PROPERTY_POST, (uint8_t *)msg, strlen(msg), GS_MQTT_QOS0, 0);
                
                // sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0002\",\"seq_no\":\"%s\"}", sw_version, gs_mqtt_generate_seq());
                // gs_mqtt_publish(PUB_TOPIC_PROPERTY_POST, (uint8_t *)msg, strlen(msg), GS_MQTT_QOS0, 0);

                // sprintf(msg, "{\"ver\":\"%s\",\"act\":\"0003\",\"type\":\"02\",\"data\":\"%d\",\"seq_no\":\"%s\"}", sw_version, cc_hal_wifi_get_connect_rssi(), gs_mqtt_generate_seq());
                // gs_mqtt_publish(PUB_TOPIC_PROPERTY_POST, (uint8_t *)msg, strlen(msg), GS_MQTT_QOS0, 0);

                g_curr_cfg_mode = GS_BIND_CFG_MODE_NULL;
                // g_curr_bind_connect_mode = GS_BIND_CFG_MODE_NULL;

                if(g_bind_timeout_timer_handle){
                    cc_timer_delete(&g_bind_timeout_timer_handle);
                }

                g_bind_status = 1;
                __save_bind_status();

                cc_event_post(GS_BIND_EVENT, GS_BIND_EVENT_SUCCESS, NULL, 0);

                cc_event_unregister_handler(GS_WIFI_EVENT, __cfg_event_handler);
                cc_event_unregister_handler(GS_MQTT_EVENT, __cfg_event_handler);

                cc_timer_simple_one(CC_TIMER_TYPE_SW, __ble_deinit, CC_TIMMER_MS(3000), NULL);
            }
            break;
        
        default:
            break;
        }
    }
}

void __ble_recv_cb(uint8_t *data, uint16_t len){
    static uint8_t recv_len = 0;
    static char recv_buf[256] = "";

    CC_LOGD(TAG, "ble recv data: %.*s", len, data);
    if(len > 20){
        __parse_ble_bind_info((char *)data, len);
        recv_len = 0;
    }else if(recv_len == 0 && ((char *)data)[0] == '{'){
        memcpy(recv_buf + recv_len, data, len);
        recv_len += len;
    }else if(len < 20 || ((char *)data)[len-1] == '}'){
        memcpy(recv_buf + recv_len, data, len);
        recv_len += len;
        recv_buf[recv_len] = '\0';

        __parse_ble_bind_info(recv_buf, recv_len);
        recv_len = 0;
    }else if(recv_len > 0){
        memcpy(recv_buf + recv_len, data, len);
        recv_len += len;
    }
}

static void __ble_bind_cfg_start(void){
    uint8_t adv_data[] = {0x02, 0x01, 0x06, 0x16, 0x09, 0x42, 0x4c, 0x45, 0x2d, 0x63, 0x6c, 0x6f, 0x75, 0x64, 0x68, 0x6f, 0x6d, 0x65, 0x2d, 0x67, 0x73, 0x2d, 0x01, 0x02, 0x03, 0x04};

    cc_hal_ble_init(__ble_recv_cb);
    
    uint8_t mac[GS_BLE_MAC_MAX_LEN] = {0};
    gs_device_get_ble_mac(mac);

    char name[22] = "";
    sprintf(name, "BLE-cloudhome-gs-%02x%02x", mac[4], mac[5]);
    memcpy(&adv_data[5], name, 21);

    cc_hal_ble_start_advzertising(adv_data, sizeof(adv_data), NULL, 0);
}

static cc_err_t __tcps_connect_cb(void *arg, uint8_t client_id){
	CC_LOGD(TAG, "__tcps_connect_cb: %d", client_id);
	return CC_OK;
}

static cc_err_t __tcps_disconnect_cb(void *arg, uint8_t client_id){
	CC_LOGD(TAG, "__tcps_disconnect_cb: %d", client_id);
	return CC_OK;
}

static cc_err_t __tcps_recv_cb(void *arg, uint8_t client_id, uint8_t *buf, uint16_t len){
    CC_LOGD(TAG, "__tcps_recv_cb: %.*s", len, buf);
    cc_err_t err = CC_FAIL;

    char ret_data[768] = {0};
    uint16_t ret_len = 512;

    err = __parse_ap_bind_info((char *)buf, len, ret_data, ret_len, &ret_len);
    if(err == CC_OK){
        CC_LOGD(TAG, "ret_data: %.*s", ret_len, ret_data);
        cc_hal_tcps_send(&g_tcp_server, client_id, ret_data, ret_len);
    }
    return CC_OK;
}

static void __ap_bind_cfg_ap_server_start(void){

    gs_wifi_ap_start_setup();

    captive_portal_start();

    if(g_tcp_server.port == 0){
        g_tcp_server.host = "0.0.0.0";
        g_tcp_server.port = 8266;
        g_tcp_server.max_client = 1;
        g_tcp_server.poll_ms = 0;
        g_tcp_server.poll_cb = NULL;
        g_tcp_server.connect_cb = __tcps_connect_cb;
        g_tcp_server.disconnect_cb = __tcps_disconnect_cb;
        g_tcp_server.recv_cb = __tcps_recv_cb;

        cc_hal_tcps_create(&g_tcp_server);
    }
}

static void __ap_bind_cfg_start(void){

    gs_wifi_scan_start();
}

uint8_t gs_bind_has_cfg_mode(void){
    if((g_curr_cfg_mode & GS_BIND_CFG_MODE_BLE) || (g_curr_cfg_mode & GS_BIND_CFG_MODE_AP)){
        return 1;
    }
    
    return 0;
}

cc_err_t gs_bind_stop_cfg_mode(void){

    if(NULL == g_bind_timeout_timer_handle){
        //todo
        return CC_FAIL;
    }

    if(g_bind_timeout_timer_handle){
        if(g_curr_cfg_mode & GS_BIND_CFG_MODE_AP){
            gs_wifi_ap_stap();
            captive_portal_stop();
            cc_hal_tcps_delete(&g_tcp_server);
        }

        cc_timer_delete(&g_bind_timeout_timer_handle);
    }

    if(g_curr_cfg_mode & GS_BIND_CFG_MODE_BLE){
        cc_timer_simple_one(CC_TIMER_TYPE_SW, __ble_deinit, CC_TIMMER_MS(200), NULL);
    }

    g_curr_cfg_mode = GS_BIND_CFG_MODE_NULL;
    g_curr_bind_connect_mode = GS_BIND_CFG_MODE_NULL;

    cc_event_post(GS_BIND_EVENT, GS_BIND_EVENT_STOP, NULL, 0);

    return CC_OK;       

}

cc_err_t gs_bind_start_cfg_mode(uint8_t mode){
    CC_LOGD(TAG, "gs_bind_start_cfg_mode: %d", mode);

    if(mode & GS_BIND_CFG_MODE_BLE){
        __ble_bind_cfg_start();
    }

    if(mode & GS_BIND_CFG_MODE_AP){
        __ap_bind_cfg_start();
    }

    g_curr_cfg_mode = mode;

    cc_event_post(GS_BIND_EVENT, GS_BIND_EVENT_START, &mode, sizeof(mode));

    if(g_bind_timeout_timer_handle == NULL){
        cc_timer_config_t timer_config = {
            .arg = NULL,
            .callback = __timer_cb_for_bind_timeout,
            .type = CC_TIMER_TYPE_SW
        };

        g_bind_timeout_timer_handle = cc_timer_create(&timer_config);
    }
    if(g_bind_timeout_timer_handle){
        cc_timer_start_once(g_bind_timeout_timer_handle, CC_TIMMER_MS(2*60000));
    }

    cc_event_register_handler(GS_WIFI_EVENT, __cfg_event_handler);
    cc_event_register_handler(GS_MQTT_EVENT, __cfg_event_handler);

    return CC_OK;
}

uint8_t gs_bind_get_boot_auto_start_cfg_mode(void){
    return (uint8_t)g_boot_auto_start_cfg;
}

cc_err_t gs_bind_set_boot_auto_start_cfg_mode(uint8_t mode){
    uint32_t boot_auto_start_cfg = mode;
    cc_err_t err = cc_hal_kvs_set(GS_BIND_BOOT_CGF_KVS_KEY, &boot_auto_start_cfg, sizeof(boot_auto_start_cfg));
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
        return err;
    } 
    g_boot_auto_start_cfg = mode;
    return CC_OK;
}

cc_err_t gs_bind_reset_bind_status(void){
    gs_wifi_config_t wifi_config = {0};
    memset(&wifi_config, 0, sizeof(gs_wifi_config_t));


    wifi_config.ssid_len = 0;
    wifi_config.password_len = 0;

    gs_wifi_sta_save_config(&wifi_config);

    gs_mqtt_reset_config();

    g_bind_status = 0;
    __save_bind_status();

    return CC_OK;
}

uint8_t gs_bind_get_bind_status(void){
    return g_bind_status;
}

void gs_bind_init(void){
    size_t len = 0;

    memset(&g_tcp_server, 0, sizeof(cc_tcps_t));

    len = sizeof(g_bind_status);
    cc_hal_kvs_get(GS_BIND_KVS_KEY, &g_bind_status, &len);

    len = sizeof(g_boot_auto_start_cfg);
    cc_hal_kvs_get(GS_BIND_BOOT_CGF_KVS_KEY, &g_boot_auto_start_cfg, &len);
}