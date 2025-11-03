#include "cc_hal_os.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

cc_os_semphr_handle_t cc_hal_os_semphr_create_binary(void){
    return xSemaphoreCreateBinary();
}

cc_os_semphr_handle_t cc_hal_os_semphr_create_mutex(void){
    return xSemaphoreCreateMutex();
}

cc_err_t cc_hal_os_semphr_delete(cc_os_semphr_handle_t handle){
    if(handle == NULL){
        return CC_FAIL;
    }
    vSemaphoreDelete(handle);
    return CC_OK;
}
cc_err_t cc_hal_os_semphr_take(cc_os_semphr_handle_t handle, cc_os_tick_t tick){
    if(handle == NULL){
        return CC_FAIL;
    }
    BaseType_t xStatus = xSemaphoreTake(handle, tick);
    if(xStatus != pdPASS){
        return CC_FAIL;
    }
    return CC_OK;
}
cc_err_t cc_hal_os_semphr_give(cc_os_semphr_handle_t handle){
    if(handle == NULL){
        return CC_FAIL;
    }
    BaseType_t xStatus = xSemaphoreGive(handle);
    if(xStatus != pdPASS){
        return CC_FAIL;
    }
    return CC_OK;
}

void cc_hal_os_task_delay(cc_os_tick_t tick){
    vTaskDelay(tick);
}

void cc_hal_os_task_delete(cc_os_task_handle_t handle){
    vTaskDelete(handle);
}

cc_err_t cc_hal_os_task_create(cc_os_task_t task, const char *name, uint32_t stack_size, void *arg, uint8_t priority, cc_os_task_handle_t *handle){
    BaseType_t xStatus = xTaskCreate((TaskFunction_t)task, name, stack_size, arg, priority, (TaskHandle_t *)handle);
    if(xStatus != pdPASS){
        return CC_FAIL;
    }
    return CC_OK;
}