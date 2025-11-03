#ifndef __CC_HAL_BLE_H__
#define __CC_HAL_BLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#include "cc_event.h"

CC_EVENT_DECLARE_BASE(CC_HAL_BLE_EVENT);

typedef enum{
    CC_HAL_BLE_EVENT_ENABLED = 0,
    CC_HAL_BLE_EVENT_CONNECTED,
    CC_HAL_BLE_EVENT_DISCONNECTED,
    CC_HAL_BLE_EVENT_ADV_START,
    CC_HAL_BLE_EVENT_ADV_STOP,
}cc_hal_ble_event_t;

typedef void (*cc_hal_ble_recv_cb_t)(uint8_t *data, uint16_t len);

cc_err_t cc_hal_ble_start_advzertising(uint8_t *adv_data, uint8_t adv_len, uint8_t *scan_rsp_data, uint8_t scan_rsp_len);
cc_err_t cc_hal_ble_stop_advzertising(void);

cc_err_t cc_hal_ble_reset_advzertising(uint8_t *adv_data, uint8_t adv_len, uint8_t *scan_rsp_data, uint8_t scan_rsp_len);

uint16_t cc_hal_ble_send(uint8_t *data, uint16_t len);

cc_err_t cc_hal_ble_get_mac(uint8_t *mac);

cc_err_t cc_hal_ble_disconnect(void);

cc_err_t cc_hal_ble_init(cc_hal_ble_recv_cb_t recv_cb);
cc_err_t cc_hal_ble_deinit(void);

#ifdef __cplusplus
}
#endif

#endif