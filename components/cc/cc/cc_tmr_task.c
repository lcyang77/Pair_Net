/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#include "cc_tmr_task.h"

#include "cc_log.h"
#include "cc_list.h"
#include "cc_hal_os.h"
#include "cc_hal_sys.h"

#include <string.h>

static char *TAG = "cc_tmr_task";

typedef struct{
	uint8_t start;
	uint64_t interval_tick;
	cc_tmr_task_t task;
	void *arg;
	uint64_t calc_tick;
}_task_ctx_t;

static cc_list_node *g_task_list = NULL;
static cc_os_semphr_handle_t g_semphr_handle = NULL;
static cc_tmr_task_t g_curr_task = NULL;

int __find_by_task(cc_list_node* node, void* arg){

    _task_ctx_t *ctx = node->data;
    cc_tmr_task_t task = (cc_tmr_task_t)arg;
    
    if(NULL == ctx){
        return 0; 
    }

    if(ctx->task == task){
        return 1; 
    }

    return 0;
}


cc_err_t cc_tmr_task_init(void){
    g_semphr_handle = cc_hal_os_semphr_create_mutex();
    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "semphr create error");
        return CC_FAIL;
    }
    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

void cc_tmr_task_run(uint16_t interval){
    cc_list_node *node = NULL;
    cc_list_node *last_node = NULL;

    if(NULL == g_task_list){
        return;
    }

    node = g_task_list;
    while(node){
        cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);
        _task_ctx_t *task_ctx = node->data;
        if(NULL != task_ctx){
            task_ctx->calc_tick += interval;
            if (task_ctx->start && task_ctx->calc_tick >= task_ctx->interval_tick) {
                cc_hal_os_semphr_give(g_semphr_handle);
                g_curr_task = task_ctx->task;
                if(task_ctx->task){
				    task_ctx->task(task_ctx->interval_tick, task_ctx->arg);
                }
                g_curr_task = NULL;

                cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);
                if(cc_list_find_node(g_task_list, node)){
                    task_ctx->calc_tick = 0;
                    last_node = node;
                    node = node->next;
                }else{
                    // is delete
                    // CC_LOGW(TAG, "curr task is delete");
                    if(last_node){
                        node = last_node->next;
                    }else{
                        node = NULL;
                    }
                }
            }else{
                last_node = node;
                node = node->next;
            }
        }else{
            last_node = node;
            node = node->next;
        }
        cc_hal_os_semphr_give(g_semphr_handle);
    }
}

cc_err_t cc_tmr_task_create(cc_tmr_task_t task, uint32_t interval, void *arg){
    _task_ctx_t *task_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    task_ctx = (_task_ctx_t *)cc_hal_sys_malloc(sizeof(_task_ctx_t));
    if (!task_ctx) {
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NO_MEM;
    }

    memset(task_ctx, 0, sizeof(_task_ctx_t));

    task_ctx->start = 1;
    task_ctx->task = task;
    task_ctx->interval_tick = interval;
    task_ctx->arg = arg;
    task_ctx->calc_tick = 0;

    if(NULL != g_task_list){
        if(NULL == cc_list_insert_end(g_task_list, task_ctx)){
            cc_hal_sys_free(task_ctx);
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NO_MEM;
        }
    }else{
        g_task_list = cc_list_create(task_ctx);
        if(NULL == g_task_list){
            cc_hal_sys_free(task_ctx);
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NO_MEM;
        }
    }
   

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_tmr_task_delete(cc_tmr_task_t task){
    cc_list_node *node = NULL;
    
    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(task == NULL){
        task = g_curr_task;
    }

    if(NULL != g_task_list){
        node = cc_list_find(g_task_list, __find_by_task, (void *)task);
        if(NULL == node){
            // CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            cc_hal_sys_free(node->data);
        }

        cc_list_remove(&g_task_list, node);
    }else{
        // CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_tmr_task_set_interval(cc_tmr_task_t task, uint32_t interval){
    cc_list_node *node = NULL;
    _task_ctx_t *task_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(NULL != g_task_list){
        node = cc_list_find(g_task_list, __find_by_task, (void *)task);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            task_ctx = (_task_ctx_t *)node->data;
            task_ctx->interval_tick = interval;
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_tmr_task_stop(cc_tmr_task_t task){
    cc_list_node *node = NULL;
    _task_ctx_t *task_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(NULL != g_task_list){
        node = cc_list_find(g_task_list, __find_by_task, (void *)task);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            task_ctx = (_task_ctx_t *)node->data;
            task_ctx->start = 0;
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_tmr_task_continue(cc_tmr_task_t task){
    cc_list_node *node = NULL;
    _task_ctx_t *task_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(NULL != g_task_list){
        node = cc_list_find(g_task_list, __find_by_task, (void *)task);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            task_ctx = (_task_ctx_t *)node->data;
            task_ctx->start = 1;
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}