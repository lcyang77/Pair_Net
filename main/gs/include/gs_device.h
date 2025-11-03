#ifndef __GS_DEVICE_H__
#define __GS_DEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"
#include "cc_event.h"

CC_EVENT_DECLARE_BASE(GS_DEVICE_EVENT);


#define CONFIG_GAOSI_LICENSE_UNAME  "xxxxxxxxxxx"
#define CONFIG_GAOSI_LICENSE_UKEY   "xxxxxxxxxxxxxxxxxxxx"

typedef enum{
    GS_DEVICE_EVENT_GET_LICENSE_SUCCESS = 0,
    GS_DEVICE_EVENT_GET_LICENSE_FAIL,
}gs_device_event_t;

#define GS_TOKEN_BUF_MAX_LEN                33
#define GS_DEVICE_SECRET_BUF_MAX_LEN        33
#define GS_PRODUCT_KEY_BUF_MAX_LEN          12
#define GS_DEVICE_NAME_BUF_MAX_LEN          17

#define GS_DEVICE_VERSION_BUF_MAX_LEN       17

#define GS_BLE_MAC_MAX_LEN                  6

cc_err_t gs_device_save_token(char *token);
cc_err_t gs_device_get_token(char *token);

cc_err_t gs_device_get_product_key(char *product_key);
cc_err_t gs_device_get_device_name(char *device_name);
cc_err_t gs_device_get_device_secret(char *device_secret);

cc_err_t gs_device_get_ble_mac(uint8_t *mac);
cc_err_t gs_device_get_wifi_mac(uint8_t *mac);

cc_err_t gs_device_get_version(char *sw, char *hw);
cc_err_t gs_device_set_version(char *sw, char *hw);

cc_err_t gs_device_get_license(void);

cc_err_t gs_device_init(void);

#ifdef __cplusplus
}
#endif

#endif
