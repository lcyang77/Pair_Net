#ifndef __GS_WIFI_H__
#define __GS_WIFI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#include "cc_event.h"

#include "cc_hal_wifi.h"

#define GS_WIFI_SSID_BUF_MAX_LEN                33
#define GS_WIFI_PASSWORD_BUF_MAX_LEN            65

CC_EVENT_DECLARE_BASE(GS_WIFI_EVENT);

typedef enum{
    GS_WIFI_EVENT_STA_START = 0,
    GS_WIFI_EVENT_STA_CONNECTED,
    GS_WIFI_EVENT_STA_DISCONNECTED,
    GS_WIFI_EVENT_STA_GOT_IP,
    GS_WIFI_EVENT_STA_DHCP_TIMEOUT,
    GS_WIFI_EVENT_STA_AP_JOIN,
    GS_WIFI_EVENT_STA_AP_LEAVE,
    GS_WIFI_EVENT_SCAN_START,
    GS_WIFI_EVENT_SCAN_DONE,
}gs_wifi_event_t;

typedef struct{
    uint8_t ssid[GS_WIFI_SSID_BUF_MAX_LEN];
    uint8_t ssid_len;
    uint8_t password[GS_WIFI_PASSWORD_BUF_MAX_LEN];
    uint8_t password_len;
}gs_wifi_config_t;

#define gs_wifi_ap_info_t           cc_hal_wifi_ap_info_t

cc_err_t gs_wifi_sta_save_config(gs_wifi_config_t *wifi_config);
cc_err_t gs_wifi_sta_get_config(gs_wifi_config_t *wifi_config);

cc_err_t gs_wifi_sta_start_connect(void);

cc_err_t gs_wifi_ap_stap(void);
cc_err_t gs_wifi_ap_start_setup(void);

uint8_t gs_wifi_get_scan_ap_result_numbers(void);
cc_err_t gs_wifi_get_scan_ap_result(gs_wifi_ap_info_t *ap_list, uint8_t ap_num);
cc_err_t gs_wifi_scan_start(void);

void gs_wifi_init(void);

#ifdef __cplusplus
}
#endif

#endif