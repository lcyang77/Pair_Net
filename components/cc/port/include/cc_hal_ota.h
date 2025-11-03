#ifndef __CC_HAL_OTA_H__
#define __CC_HAL_OTA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

cc_err_t cc_hal_ota_update(uint32_t idx, uint8_t *buf, uint16_t len);

cc_err_t cc_hal_ota_end(void);

cc_err_t cc_hal_ota_begin(void);

#ifdef __cplusplus
}
#endif

#endif