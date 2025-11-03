#ifndef __CC_HAL_KVS_H__
#define __CC_HAL_KVS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

cc_err_t cc_hal_kvs_init(void);
cc_err_t cc_hal_kvs_get(const char *key, void *value, size_t *len);
cc_err_t cc_hal_kvs_set(const char *key, const void *value, size_t len);
cc_err_t cc_hal_kvs_del(const char *key);
cc_err_t cc_hal_kvs_del_all(void);

#ifdef __cplusplus
}
#endif

#endif