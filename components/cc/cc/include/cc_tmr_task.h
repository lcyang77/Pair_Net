/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#ifndef __CC_TMR_TASK_H__
#define __CC_TMR_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

typedef void (*cc_tmr_task_t)(uint32_t interval, void *arg);

cc_err_t cc_tmr_task_init(void);

void cc_tmr_task_run(uint16_t interval);

cc_err_t cc_tmr_task_create(cc_tmr_task_t task, uint32_t interval, void *arg);
cc_err_t cc_tmr_task_delete(cc_tmr_task_t task);

cc_err_t cc_tmr_task_set_interval(cc_tmr_task_t task, uint32_t interval);

cc_err_t cc_tmr_task_stop(cc_tmr_task_t task);
cc_err_t cc_tmr_task_continue(cc_tmr_task_t task);

#ifdef __cplusplus
}
#endif

#endif