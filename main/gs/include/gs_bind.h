#ifndef __GS_BIND_H__
#define __GS_BIND_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"
#include "cc_event.h"

CC_EVENT_DECLARE_BASE(GS_BIND_EVENT);

#define GS_BIND_CFG_MODE_NULL      0
#define GS_BIND_CFG_MODE_AP        (0x01 << 0)
#define GS_BIND_CFG_MODE_BLE       (0x01 << 1)

typedef enum{
    GS_BIND_ERR_TIMEOUT,
    GS_BIND_ERR_WIFI_NOT_FOUNT,
    GS_BIND_ERR_WIFI_PASSWORD,
}gs_bind_err_t;

typedef enum{
    GS_BIND_EVENT_START = 0,
    GS_BIND_EVENT_SUCCESS,
    GS_BIND_EVENT_FAIL,
    GS_BIND_EVENT_STOP,
}gs_bind_event_t;

void gs_bind_init(void);

cc_err_t gs_bind_reset_bind_status(void);
uint8_t gs_bind_get_bind_status(void);
cc_err_t gs_bind_start_cfg_mode(uint8_t mode);
cc_err_t gs_bind_stop_cfg_mode(void);
uint8_t gs_bind_has_cfg_mode(void);

uint8_t gs_bind_get_boot_auto_start_cfg_mode(void);
cc_err_t gs_bind_set_boot_auto_start_cfg_mode(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif