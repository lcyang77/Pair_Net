/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:41
 * @Last Modified time: 2022-03-20 18:41:41
 */

#ifndef __CC_TIME_H__
#define __CC_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#define CC_TIMMER_MS(ms)    (((uint64_t)ms)*1000)

typedef void *cc_timer_handle_t;

typedef enum {
    CC_TIMER_TYPE_SW,     //!< Callback is called from timer task
    CC_TIMER_TYPE_HW,      //!< Callback is called from timer ISR
} cc_timer_type_t;

typedef void (*cc_timer_cb_t)(void* arg);

typedef struct {
    cc_timer_type_t type;           //!< Call the callback from task or from ISR
    cc_timer_cb_t callback;         //!< Function to call when timer expires
    void* arg;                      //!< Argument to pass to the callback
} cc_timer_config_t;

cc_timer_handle_t cc_timer_create(const cc_timer_config_t* config);

cc_err_t cc_timer_start_once(cc_timer_handle_t timer, uint64_t us);

cc_err_t cc_timer_start_periodic(cc_timer_handle_t timer, uint64_t us);

cc_err_t cc_timer_stop(cc_timer_handle_t timer);

cc_err_t cc_timer_delete(cc_timer_handle_t *timer);

cc_err_t cc_timer_simple_one(cc_timer_type_t type, cc_timer_cb_t callback, uint64_t us, void* arg);

void cc_timer_run(uint64_t us);
cc_err_t cc_timer_init(void);

#ifdef __cplusplus
}
#endif

#endif  //__CC_EVENT_H__