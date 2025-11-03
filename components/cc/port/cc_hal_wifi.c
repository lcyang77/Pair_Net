#include "cc_hal_wifi.h"

#include "cc_log.h"

#include "cc_hal_sys.h"

#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <netdb.h>

static char *TAG = "cc_hal_wifi";

CC_EVENT_DEFINE_BASE(CC_HAL_WIFI_EVENT);

static uint8_t g_ap_list_num = 0;
static cc_hal_wifi_ap_info_t *g_ap_list = NULL;

static uint8_t g_connect_status = 0;

static void __event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id) {
            case WIFI_EVENT_STA_CONNECTED:
                // CC_LOGD(TAG, "STA connected to %s", ((wifi_event_sta_connected_t *)event_data)->ssid);
                if(g_connect_status == 0){
                    g_connect_status = 1;
                    cc_event_post(CC_HAL_WIFI_EVENT, CC_HAL_WIFI_EVENT_STA_CONNECTED, NULL, 0);
                }
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                // CC_LOGD(TAG, "STA disconnected, reason(%d)", ((wifi_event_sta_disconnected_t *)event_data)->disconnect_reason);
                if(g_connect_status == 1){
                    g_connect_status = 0;
                    cc_event_post(CC_HAL_WIFI_EVENT, CC_HAL_WIFI_EVENT_STA_DISCONNECTED, NULL, 0);
                }else{
                    cc_hal_wifi_connect_err_t connect_err = CC_HAL_WIFI_CONNECT_ERR_UNKNOW;
                    // wifi_err_reason_t reason = ((wifi_event_sta_disconnected_t *)event_data)->disconnect_reason;
                    // if(reason == WIFI_REASON_NO_AP_FOUND){
                    //     connect_err = CC_HAL_WIFI_CONNECT_ERR_AP_NOT_FOUND;
                    //     CC_LOGW_CODE(TAG, CC_HAL_WIFI_CONNECT_ERR_AP_NOT_FOUND);
                    // }else if(reason == WIFI_REASON_WRONG_PASSWORD){
                    //     connect_err = CC_HAL_WIFI_CONNECT_ERR_AUTH_FAIL;
                    //     CC_LOGW_CODE(TAG, WIFI_REASON_WRONG_PASSWORD);
                    // }
                    cc_event_post(CC_HAL_WIFI_EVENT, CC_HAL_WIFI_EVENT_STA_CONNECT_FAIL, &connect_err, sizeof(cc_hal_wifi_connect_err_t));
                }
                esp_wifi_connect();
                break;
            case WIFI_EVENT_SCAN_DONE:{
                CC_LOGD(TAG, "scan done");

                wifi_ap_record_t *ap_info = NULL;
                uint16_t ap_num = CC_HAL_WIFI_SCAN_MAX_AP;
                
                ap_info = cc_hal_sys_malloc(sizeof(wifi_ap_record_t)*CC_HAL_WIFI_SCAN_MAX_AP);
                if (ap_info == NULL){
                    CC_LOGE(TAG, "Malloc Fail!");
                    return;
                }
                ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_info));

                g_ap_list_num = ap_num;

                if(g_ap_list){
                    cc_hal_sys_free(g_ap_list);
                }

                g_ap_list = cc_hal_sys_malloc(sizeof(cc_hal_wifi_ap_info_t) * g_ap_list_num);
                if (g_ap_list_num > 0 && g_ap_list == NULL){
                    CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
                    return;
                }

                for(size_t i = 0; i < g_ap_list_num; i++)
                {
                    strcpy((char *)g_ap_list[i].ssid, (char *)ap_info[i].ssid);
                    g_ap_list[i].ssid_len = strlen((char *)ap_info[i].ssid);
                    g_ap_list[i].rssi = ap_info[i].rssi;
                    g_ap_list[i].channel = ap_info[i].primary;
                }
                cc_hal_sys_free(ap_info);

                cc_event_post(CC_HAL_WIFI_EVENT, CC_HAL_WIFI_EVENT_SCAN_DONE, NULL, 0);
                break;
            }
        }
	}else if(event_base == IP_EVENT){
		switch (event_id) {
            case IP_EVENT_STA_GOT_IP:{
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                char ip[16] = {0};
                sprintf(ip, IPSTR, IP2STR(&event->ip_info.ip));
                CC_LOGI(TAG, "Got IP: %s", ip);
                cc_event_post(CC_HAL_WIFI_EVENT, CC_HAL_WIFI_EVENT_STA_GOT_IP, ip, strlen(ip) + 1);
                break;
            }
        }
	}
}

int8_t cc_hal_wifi_get_connect_rssi(void){
    wifi_ap_record_t ap_info;
    ap_info.rssi = 0;
    esp_wifi_sta_get_ap_info(&ap_info);
    return (int8_t)ap_info.rssi;
}

cc_err_t cl_hal_wifi_sta_get_mac(uint8_t *mac){
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);
    return (err == ESP_OK) ? CC_OK : CC_FAIL;
}

uint8_t cc_hal_wifi_sta_get_connect_status(void){
    return g_connect_status;
}

cc_err_t cc_hal_wifi_sta_get_connect_ip(char *ip){
    esp_netif_ip_info_t local_ip;   

    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
    uint32_t ip_addr = (uint32_t)local_ip.ip.addr;
    sprintf((char *)ip, "%u.%u.%u.%u",
        (uint8_t)(ip_addr & 0xFF),
        (uint8_t)((ip_addr >> 8) & 0xFF),
        (uint8_t)((ip_addr >> 16) & 0xFF),
        (uint8_t)((ip_addr >> 24) & 0xFF)
    );
    return CC_OK;
}

cc_hal_wifi_mode_t cc_hal_wifi_get_mode(void){
    wifi_mode_t mode;
    if(esp_wifi_get_mode(&mode) == ESP_OK){
        switch (mode)
        {
        case WIFI_MODE_STA:
            return CC_HAL_WIFI_MODE_STA;
            break;
        case WIFI_MODE_AP:
            return CC_HAL_WIFI_MODE_AP;
            break;
        case WIFI_MODE_APSTA:
            return CC_HAL_WIFI_MODE_APSTA;
            break;
        default:
            break;
        }
    }
    return CC_HAL_WIFI_MODE_NULL;
}

cc_err_t cc_hal_wifi_set_mode(cc_hal_wifi_mode_t mode){
    if(mode >= CC_HAL_WIFI_MODE_MAX){
        return CC_ERR_INVALID_ARG;
    }

    switch (mode)
    {
    case CC_HAL_WIFI_MODE_STA:
        esp_wifi_set_mode(WIFI_MODE_STA);
        break;
    case CC_HAL_WIFI_MODE_AP:
        esp_wifi_set_mode(WIFI_MODE_AP);
        break;
    case CC_HAL_WIFI_MODE_APSTA:
        esp_wifi_set_mode(WIFI_MODE_APSTA);
        break;
    default:
        return CC_ERR_NOT_SUPPORTED;
    }

    return CC_OK;
}

cc_err_t cc_hal_wifi_ap_start_setup(cc_hal_wifi_ap_config_t *ap_config){

    wifi_config_t wifi_config = {
        .ap = {           
            .channel = ap_config->channel,
            .max_connection = ap_config->max_connection,
            .ssid_hidden=ap_config->ssid_hidden,
            .authmode = ap_config->authmode
        },
    };

    CC_LOGD(TAG, "ap_start_setup ssid :%s", ap_config->ssid);
    
    memcpy(wifi_config.ap.ssid, ap_config->ssid, ap_config->ssid_len+1);
    wifi_config.ap.ssid_len = ap_config->ssid_len;
    memcpy(wifi_config.ap.password, ap_config->password, ap_config->password_len+1);

    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif && strlen(ap_config->ip4_config.ip) > 0)
    {
        esp_netif_ip_info_t ip_info;
        memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));

        ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
        ip_info.ip.addr = esp_ip4addr_aton((const char *)ap_config->ip4_config.ip);
        ip_info.netmask.addr = esp_ip4addr_aton((const char *)ap_config->ip4_config.mask);
        ip_info.gw.addr = esp_ip4addr_aton((const char *)ap_config->ip4_config.gateway);
        esp_netif_set_ip_info(ap_netif, &ip_info);
        ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    }

    esp_wifi_set_mode(WIFI_MODE_AP);

    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);

    // esp_wifi_start();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return CC_OK;
}

cc_err_t cc_hal_wifi_ap_stop(void){

    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
        esp_wifi_stop();
    }

    return CC_OK;
}

cc_err_t cc_hal_wifi_sta_start_connect(cc_hal_wifi_sta_config_t *sta_config){

    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, sta_config->ssid, sta_config->ssid_len + 1);
    memcpy(wifi_config.sta.password, sta_config->password, sta_config->password_len + 1);
    wifi_config.sta.threshold.authmode = (sta_config->password_len == 0)?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK;

    CC_LOGI(TAG, "Connecting to Wi-Fi:%s, PSW:%s",wifi_config.sta.ssid, wifi_config.sta.password);

    esp_wifi_set_mode(WIFI_MODE_STA);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    return CC_OK;
}

cc_err_t cc_hal_wifi_sta_disconnect(void){
    esp_wifi_disconnect();
    return CC_OK;
}

uint8_t cc_hal_wifi_get_scan_ap_result_numbers(void){
    return g_ap_list_num;
}

cc_err_t cc_hal_wifi_get_scan_ap_result(cc_hal_wifi_ap_info_t *ap_list, uint8_t ap_num){

    uint16_t get_num = 0;

    if(g_ap_list_num == 0 || ap_list == NULL){
        CC_LOGE(TAG, "wifi not scan");
        return CC_FAIL;
    }

    get_num = ap_num < g_ap_list_num?ap_num:g_ap_list_num;

    memcpy(ap_list, g_ap_list, sizeof(cc_hal_wifi_ap_info_t) * get_num);

    return CC_OK;
}

cc_err_t cc_hal_wifi_scan_start(void){
    cc_hal_wifi_set_mode(CC_HAL_WIFI_MODE_STA);

    esp_wifi_start();

    esp_wifi_disconnect();
    esp_wifi_scan_start(NULL, false);
    
    return CC_OK;
}

void cc_hal_wifi_init(void){

    esp_netif_init();

    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &__event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &__event_handler, NULL);
}
