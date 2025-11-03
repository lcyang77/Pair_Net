#ifndef __CC_HAL_WIFI_H__
#define __CC_HAL_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"
#include "cc_event.h"

CC_EVENT_DECLARE_BASE(CC_HAL_WIFI_EVENT);

#define CC_HAL_WIFI_IP4_STR_LEN        16

#define CC_HAL_WIFI_SCAN_MAX_AP          32

typedef enum{
    CC_HAL_WIFI_CONNECT_ERR_UNKNOW = 100,
	CC_HAL_WIFI_CONNECT_ERR_AP_NOT_FOUND,
	CC_HAL_WIFI_CONNECT_ERR_AUTH_FAIL,
}cc_hal_wifi_connect_err_t;

typedef enum{
    CC_HAL_WIFI_EVENT_STA_CONNECTED = 0,
    CC_HAL_WIFI_EVENT_STA_DISCONNECTED,
    CC_HAL_WIFI_EVENT_STA_CONNECT_FAIL,
    CC_HAL_WIFI_EVENT_STA_GOT_IP,
    CC_HAL_WIFI_EVENT_STA_DHCP_TIMEOUT,
    CC_HAL_WIFI_EVENT_STA_AP_JOIN,
    CC_HAL_WIFI_EVENT_STA_AP_LEAVE,
    CC_HAL_WIFI_EVENT_SCAN_START,
    CC_HAL_WIFI_EVENT_SCAN_DONE,
}cc_hal_wifi_event_t;

typedef enum{   
    CC_HAL_WIFI_MODE_NULL = 0,  /**< null mode */
    CC_HAL_WIFI_MODE_STA,       /**< WiFi station mode */
    CC_HAL_WIFI_MODE_AP,        /**< WiFi soft-AP mode */
    CC_HAL_WIFI_MODE_APSTA,     /**< WiFi station + soft-AP mode */
    CC_HAL_WIFI_MODE_MAX
}cc_hal_wifi_mode_t;

typedef enum {
    CC_HAL_WIFI_IF_STA = 0,
    CC_HAL_WIFI_IF_AP,
} cc_hal_wifi_interface_t;

typedef enum {
    CC_HAL_WIFI_AUTH_OPEN = 0,         /**< authenticate mode : open */
    CC_HAL_WIFI_AUTH_WEP,              /**< authenticate mode : WEP */
    CC_HAL_WIFI_AUTH_WPA_PSK,          /**< authenticate mode : WPA_PSK */
    CC_HAL_WIFI_AUTH_WPA2_PSK,         /**< authenticate mode : WPA2_PSK */
    CC_HAL_WIFI_AUTH_WPA_WPA2_PSK,     /**< authenticate mode : WPA_WPA2_PSK */
    CC_HAL_WIFI_AUTH_WPA2_ENTERPRISE,  /**< authenticate mode : WPA2_ENTERPRISE */
    CC_HAL_WIFI_AUTH_WPA3_PSK,         /**< authenticate mode : WPA3_PSK */
    CC_HAL_WIFI_AUTH_WPA2_WPA3_PSK,    /**< authenticate mode : WPA2_WPA3_PSK */
    CC_HAL_WIFI_AUTH_WAPI_PSK,         /**< authenticate mode : WAPI_PSK */
    CC_HAL_WIFI_AUTH_OWE,              /**< authenticate mode : OWE */
    CC_HAL_WIFI_AUTH_MAX
} cc_hal_wifi_auth_mode_t;

typedef struct {
	char ip[CC_HAL_WIFI_IP4_STR_LEN];        /**< Local IP address. */
	char mask[CC_HAL_WIFI_IP4_STR_LEN];      /**< Netmask. */
	char gateway[CC_HAL_WIFI_IP4_STR_LEN];   /**< Gateway IP address. */
} cc_hal_wifi_ap_ip4_config_t;

typedef struct {
    uint8_t ssid[32];           /**< SSID of ESP32 soft-AP. If ssid_len field is 0, this must be a Null terminated string. Otherwise, length is set according to ssid_len. */
    uint8_t password[64];       /**< Password of ESP32 soft-AP. */
    uint8_t ssid_len;           /**< Optional length of SSID field. */
    uint8_t password_len;       /**< Optional length of Password field. */
    uint8_t channel;            /**< Channel of ESP32 soft-AP */
    cc_hal_wifi_auth_mode_t authmode;  /**< Auth mode of ESP32 soft-AP. Do not support AUTH_WEP in soft-AP mode */
    cc_hal_wifi_ap_ip4_config_t ip4_config;
    uint8_t ssid_hidden;        /**< Broadcast SSID or not, default 0, broadcast the SSID */
    uint8_t max_connection;     /**< Max number of stations allowed to connect in, default 4, max 10 */
} cc_hal_wifi_ap_config_t;

typedef struct {
    uint8_t ssid[32];      /**< SSID of target AP. */
    uint8_t password[64];  /**< Password of target AP. */
    uint8_t ssid_len;           /**< Optional length of SSID field. */
    uint8_t password_len;       /**< Optional length of Password field. */
    uint8_t channel;       /**< channel of target AP. Set to 1~13 to scan starting from the specified channel before connecting to AP. If the channel of AP is unknown, set it to 0.*/
    uint8_t bssid_set;        /**< whether set MAC address of target AP or not. Generally, station_config.bssid_set needs to be 0; and it needs to be 1 only when users need to check the MAC address of the AP.*/
    uint8_t bssid[6];     /**< MAC address of target AP*/
 } cc_hal_wifi_sta_config_t;

 typedef struct{
    uint8_t ssid[32];
    uint8_t ssid_len;
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
}cc_hal_wifi_ap_info_t;

uint8_t cc_hal_wifi_sta_get_connect_status(void);

cc_err_t cc_hal_wifi_sta_get_connect_ip(char *ip);

int8_t cc_hal_wifi_get_connect_rssi(void);

cc_err_t cl_hal_wifi_sta_get_mac(uint8_t *mac);

cc_hal_wifi_mode_t cc_hal_wifi_get_mode(void);
cc_err_t cc_hal_wifi_set_mode(cc_hal_wifi_mode_t mode);

cc_err_t cc_hal_wifi_ap_start_setup(cc_hal_wifi_ap_config_t *ap_config);
cc_err_t cc_hal_wifi_ap_stop(void);

cc_err_t cc_hal_wifi_sta_start_connect(cc_hal_wifi_sta_config_t *wifi_config);
cc_err_t cc_hal_wifi_sta_disconnect(void);

uint8_t cc_hal_wifi_get_scan_ap_result_numbers(void);
cc_err_t cc_hal_wifi_get_scan_ap_result(cc_hal_wifi_ap_info_t *ap_list, uint8_t ap_num);
cc_err_t cc_hal_wifi_scan_start(void);

void cc_hal_wifi_init(void);

#ifdef __cplusplus
}
#endif

#endif
