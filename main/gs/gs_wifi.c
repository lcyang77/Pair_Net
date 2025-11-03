#include "gs_wifi.h"

#include <string.h>
#include <stdlib.h>

#include "cc_log.h"
#include "cc_hal_sys.h"
#include "cc_hal_kvs.h"
#include "cc_hal_wifi.h"

#define WIFI_KVS_KEY    "gs_wifi"

static char *TAG = "gs_wifi";

CC_EVENT_DEFINE_BASE(GS_WIFI_EVENT);

static gs_wifi_config_t g_wifi_sta_config = {0};

static void __event_handler(void* handler_args, cc_event_base_t base_event, int32_t id, void* event_data){

	if(base_event == CC_HAL_WIFI_EVENT){
		switch (id) {
        case CC_HAL_WIFI_EVENT_STA_CONNECTED:
            CC_LOGI(TAG, "wifi connect");
            cc_event_post(GS_WIFI_EVENT, GS_WIFI_EVENT_STA_CONNECTED, NULL, 0);
            break;
        case CC_HAL_WIFI_EVENT_STA_DISCONNECTED:
            CC_LOGI(TAG, "wifi disconnect");
            cc_event_post(GS_WIFI_EVENT, GS_WIFI_EVENT_STA_DISCONNECTED, NULL, 0);
            break;
        case CC_HAL_WIFI_EVENT_SCAN_DONE:
            cc_event_post(GS_WIFI_EVENT, GS_WIFI_EVENT_SCAN_DONE, NULL, 0);
            break;
		case CC_HAL_WIFI_EVENT_STA_GOT_IP:
            CC_LOGI(TAG, "wifi got ip: %s", event_data);
            cc_event_post(GS_WIFI_EVENT, GS_WIFI_EVENT_STA_GOT_IP, NULL, 0);
            break;
		}
	}
}

uint8_t gs_wifi_get_scan_ap_result_numbers(void){
    return cc_hal_wifi_get_scan_ap_result_numbers();
}

cc_err_t gs_wifi_get_scan_ap_result(gs_wifi_ap_info_t *ap_list, uint8_t ap_num){
    return cc_hal_wifi_get_scan_ap_result(ap_list, ap_num);
}

cc_err_t gs_wifi_sta_save_config(gs_wifi_config_t *wifi_config){
    cc_err_t err = CC_FAIL;

    err = cc_hal_kvs_set(WIFI_KVS_KEY, wifi_config, sizeof(gs_wifi_config_t));
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
        return err;
    }   

    memcpy(&g_wifi_sta_config, wifi_config, sizeof(gs_wifi_config_t));

    CC_LOGI(TAG, "gs_wifi_sta_save_config ssid: %.*s password: %.*s", wifi_config->ssid_len ,wifi_config->ssid, wifi_config->password_len, wifi_config->password);

    return CC_OK;
}

cc_err_t gs_wifi_sta_get_config(gs_wifi_config_t *wifi_config){
    memcpy(wifi_config, &g_wifi_sta_config, sizeof(gs_wifi_config_t));
    return CC_OK;
}

cc_err_t gs_wifi_sta_start_connect(void){
    cc_err_t err = CC_FAIL;

    if(g_wifi_sta_config.ssid_len == 0){
        CC_LOGE(TAG, "wifi not set config");
        return CC_FAIL;
    }

    cc_hal_wifi_set_mode(CC_HAL_WIFI_MODE_STA);

    cc_hal_wifi_sta_config_t config = {0};
    memset(&config, 0, sizeof(config));

    memcpy(config.ssid, g_wifi_sta_config.ssid, g_wifi_sta_config.ssid_len);
    config.ssid[g_wifi_sta_config.ssid_len] = '\0';
    config.ssid_len = g_wifi_sta_config.ssid_len;

    memcpy(config.password, g_wifi_sta_config.password, g_wifi_sta_config.password_len);
    config.password[g_wifi_sta_config.password_len] = '\0';
    config.password_len = g_wifi_sta_config.password_len;

    err = cc_hal_wifi_sta_start_connect(&config);
    
    if(err == CC_OK){
        CC_LOGI(TAG, "gs_wifi_sta_start_connect ssid: %s, password: %s", config.ssid, config.password);
        cc_event_post(GS_WIFI_EVENT, GS_WIFI_EVENT_STA_START, NULL, 0);
    }

    return err;
}


cc_err_t gs_wifi_ap_start_setup(void){
    cc_err_t err = CC_FAIL;
    
    // cc_hal_wifi_set_mode(CC_HAL_WIFI_MODE_AP);

    cc_hal_wifi_ap_config_t config = {0};
    memset(&config, 0, sizeof(config));

    strcpy((char *)config.ssid, "cloudhome-gs-10");
    config.ssid_len = strlen("cloudhome-gs-10");

    config.password_len = 0;
    config.authmode = CC_HAL_WIFI_AUTH_OPEN;
    config.channel = 1;
    config.ssid_hidden = 0;
    config.max_connection = 2;
    config.authmode = CC_HAL_WIFI_AUTH_OPEN;

    strcpy((char *)config.ip4_config.ip, "192.168.6.1");
	strcpy((char *)config.ip4_config.mask, "255.255.255.0");
	strcpy((char *)config.ip4_config.gateway, "192.168.6.1");

    err = cc_hal_wifi_ap_start_setup(&config);
    if(err == CC_OK){
        CC_LOGI(TAG, "gs_wifi_ap_start_for_conifg ssid: %.*s, password: %.*s", config.ssid_len, config.ssid, config.password_len, config.password);
    }
    return err;
}


cc_err_t gs_wifi_ap_stap(void){
    return cc_hal_wifi_ap_stop();
}

cc_err_t gs_wifi_scan_start(void){
    return cc_hal_wifi_scan_start();
}

void gs_wifi_init(void){

    size_t len = 0;

    memset(&g_wifi_sta_config, 0, sizeof(gs_wifi_config_t));

    len = sizeof(gs_wifi_config_t);
    cc_hal_kvs_get(WIFI_KVS_KEY, &g_wifi_sta_config, &len);

    if(len > 0){
        CC_LOGI(TAG, "read wifi config ssid: %.*s, password: %.*s", g_wifi_sta_config.ssid_len, g_wifi_sta_config.ssid, g_wifi_sta_config.password_len, g_wifi_sta_config.password);
    }else{
        CC_LOGI(TAG, "wifi not config");
    }

    cc_event_register_handler(CC_HAL_WIFI_EVENT, __event_handler);
}
