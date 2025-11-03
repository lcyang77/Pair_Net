/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:41
 * @Last Modified time: 2022-03-20 18:41:41
 */

#ifndef __CC_EVENT_H__
#define __CC_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#include "esp_event.h"

#define cc_event_base_t             esp_event_base_t
#define cc_event_handler_t          esp_event_handler_t

#define CC_EVENT_DECLARE_BASE(base) extern cc_event_base_t base
#define CC_EVENT_DEFINE_BASE(base) cc_event_base_t base = #base

cc_err_t cc_event_register_handler(cc_event_base_t base_event, cc_event_handler_t handler);
cc_err_t cc_event_unregister_handler(cc_event_base_t base_event, cc_event_handler_t handler);

cc_err_t cc_event_post(cc_event_base_t base_event, int32_t event_id, void* event_data, size_t event_data_len);
cc_err_t cc_event_real_post(cc_event_base_t base_event, int32_t event_id, void* event_data, size_t event_data_len);

cc_err_t cc_event_init(void);
void cc_event_run(void);

#ifdef __cplusplus
}
#endif

#endif  //__CC_EVENT_H__