/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#ifndef __CC_ERR_H__
#define __CC_ERR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>
#include <assert.h>


typedef int32_t cc_err_t;

/* Definitions for error constants. */
#define CC_OK          0       /*!< esp_err_t value indicating success (no error) */
#define CC_FAIL        -1      /*!< Generic esp_err_t code indicating failure */

#define CC_ERR_NO_MEM              0x101   /*!< Out of memory */
#define CC_ERR_INVALID_ARG         0x102   /*!< Invalid argument */
#define CC_ERR_INVALID_STATE       0x103   /*!< Invalid state */
#define CC_ERR_INVALID_SIZE        0x104   /*!< Invalid size */
#define CC_ERR_NOT_FOUND           0x105   /*!< Requested resource not found */
#define CC_ERR_NOT_SUPPORTED       0x106   /*!< Operation or feature not supported */
#define CC_ERR_TIMEOUT             0x107   /*!< Operation timed out */
#define CC_ERR_INVALID_RESPONSE    0x108   /*!< Received response was invalid */
#define CC_ERR_INVALID_CRC         0x109   /*!< CRC or checksum was invalid */
#define CC_ERR_INVALID_VERSION     0x10A   /*!< Version was invalid */
#define CC_ERR_NOT_RESOURCES       0x10B   

#ifdef __cplusplus
}
#endif

#endif
