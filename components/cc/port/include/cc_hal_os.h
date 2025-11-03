#ifndef __CC_HAL_OS_H__
#define __CC_HAL_OS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifndef portTICK_RATE_MS
#define portTICK_RATE_MS    portTICK_PERIOD_MS
#endif

typedef TaskHandle_t cc_os_task_handle_t;
typedef QueueHandle_t cc_os_semphr_handle_t;

typedef TaskFunction_t cc_os_task_t;

typedef TickType_t cc_os_tick_t;
#define CC_OS_TICK_PERIOD_MS                 portTICK_PERIOD_MS
#define CC_OS_MAX_DELAY                 portMAX_DELAY
#define CC_OS_MS_TO_TICK                pdMS_TO_TICKS

cc_os_semphr_handle_t cc_hal_os_semphr_create_binary(void);
cc_os_semphr_handle_t cc_hal_os_semphr_create_mutex(void);
cc_err_t cc_hal_os_semphr_delete(cc_os_semphr_handle_t handle);
cc_err_t cc_hal_os_semphr_take(cc_os_semphr_handle_t handle, cc_os_tick_t tick);
cc_err_t cc_hal_os_semphr_give(cc_os_semphr_handle_t handle);

void cc_hal_os_task_delay(cc_os_tick_t tick);
void cc_hal_os_task_delete(cc_os_task_handle_t handle);
cc_err_t cc_hal_os_task_create(cc_os_task_t task, const char *name, uint32_t stack_size, void *arg, uint8_t priority, cc_os_task_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif