/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#include "cc_timer.h"

#include <string.h>

#include "cc_log.h"

#include "cc_hal_sys.h"
#include "cc_hal_os.h"

#include "cc_list.h"

static char *TAG = "cc_timer";

#define	CC_TIMER_RELOAD_PERIODIC	1
#define	CC_TIMER_RELOAD_ONCE	0

typedef struct{
	uint8_t start;
	uint64_t period_us;
	uint8_t repeat;
	cc_timer_cb_t cb;
	void *arg;
	uint64_t calc_us;
}_sw_timer_ctx_t;

static cc_list_node *g_timer_list = NULL;
static cc_os_semphr_handle_t g_semphr_handle = NULL;
static cc_timer_handle_t g_curr_task = NULL;

static int __find_by_handle(cc_list_node* node, void* arg){

    _sw_timer_ctx_t *ctx = node->data;
    _sw_timer_ctx_t *handle = (_sw_timer_ctx_t *)arg;
    
    if(NULL == ctx){
        return 0; 
    }

    if(ctx == handle){
        return 1; 
    }

    return 0;
}

cc_timer_handle_t cc_timer_create(const cc_timer_config_t* config){

    _sw_timer_ctx_t *sw_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return NULL;
    }

    if(NULL == config){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return NULL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(config->type == CC_TIMER_TYPE_SW){
        sw_ctx = (_sw_timer_ctx_t *)cc_hal_sys_malloc(sizeof(_sw_timer_ctx_t));
        if (!sw_ctx) {
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            cc_hal_os_semphr_give(g_semphr_handle);
            return NULL;
        }

        memset(sw_ctx, 0, sizeof(_sw_timer_ctx_t));

        sw_ctx->start = 0;
        sw_ctx->cb = config->callback;
        sw_ctx->arg = config->arg;
        sw_ctx->calc_us = 0;

        if(NULL != g_timer_list){
            if(NULL == cc_list_insert_end(g_timer_list, sw_ctx)){
                cc_hal_sys_free(sw_ctx);
                CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
                cc_hal_os_semphr_give(g_semphr_handle);
                return NULL;
            }
        }else{
            g_timer_list = cc_list_create(sw_ctx);
            if(NULL == g_timer_list){
                cc_hal_sys_free(sw_ctx);
                CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
                cc_hal_os_semphr_give(g_semphr_handle);
                return NULL;
            }
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_SUPPORTED);
        cc_hal_os_semphr_give(g_semphr_handle);
        return NULL;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return sw_ctx;
}

cc_err_t cc_timer_start_once(cc_timer_handle_t timer, uint64_t us){
    cc_list_node *node = NULL;
    _sw_timer_ctx_t *sw_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(NULL != g_timer_list){
        node = cc_list_find(g_timer_list, __find_by_handle, (void *)timer);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            sw_ctx = (_sw_timer_ctx_t *)node->data;
            sw_ctx->period_us = us;
            sw_ctx->repeat = CC_TIMER_RELOAD_ONCE;
            sw_ctx->calc_us = 0;
            sw_ctx->start = 1;
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_timer_start_periodic(cc_timer_handle_t timer, uint64_t us){
    cc_list_node *node = NULL;
    _sw_timer_ctx_t *sw_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);
    
    if(NULL != g_timer_list){
        node = cc_list_find(g_timer_list, __find_by_handle, (void *)timer);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            sw_ctx = (_sw_timer_ctx_t *)node->data;
            sw_ctx->period_us = us;
            sw_ctx->repeat = CC_TIMER_RELOAD_PERIODIC;
            sw_ctx->calc_us = 0;
            sw_ctx->start = 1;
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_timer_stop(cc_timer_handle_t timer){
    cc_list_node *node = NULL;
    _sw_timer_ctx_t *sw_ctx = NULL;

    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);
    
    if(NULL != g_timer_list){
        node = cc_list_find(g_timer_list, __find_by_handle, (void *)timer);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            sw_ctx = (_sw_timer_ctx_t *)node->data;
            sw_ctx->start = 0;
        }
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

cc_err_t cc_timer_delete(cc_timer_handle_t *timer){
    cc_list_node *node = NULL;
    
    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "cc timer not init");
        return CC_FAIL;
    }

    cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);

    if(timer == NULL && g_curr_task != NULL){
        timer = &g_curr_task;
    }

    if(NULL != g_timer_list){
        node = cc_list_find(g_timer_list, __find_by_handle, (void *)*timer);
        if(NULL == node){
            CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
            cc_hal_os_semphr_give(g_semphr_handle);
            return CC_ERR_NOT_FOUND;
        }

        if(node->data){
            cc_hal_sys_free(node->data);
        }

        *timer = NULL;

        cc_list_remove(&g_timer_list, node);
    }else{
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        cc_hal_os_semphr_give(g_semphr_handle);
        return CC_ERR_NOT_FOUND;
    }

    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
    
}

void cc_timer_run(uint64_t us){

    cc_list_node *node = NULL;
    cc_list_node *last_node = NULL;

    if(NULL == g_semphr_handle){
        return;
    }

    node = g_timer_list;
    while(node){
        cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);
        _sw_timer_ctx_t *sw_ctx = node->data;
        if(NULL != sw_ctx){
            sw_ctx->calc_us += us;
            if (sw_ctx->start && sw_ctx->calc_us >= sw_ctx->period_us) {
                g_curr_task = sw_ctx;
                cc_hal_os_semphr_give(g_semphr_handle);
                if(sw_ctx->cb){
				    sw_ctx->cb(sw_ctx->arg);
                }

                cc_hal_os_semphr_take(g_semphr_handle, CC_OS_MAX_DELAY);
                g_curr_task = NULL;
                if(cc_list_find_node(g_timer_list, node)){
                    // sw_ctx->calc_us -= sw_ctx->period_us;
                    sw_ctx->calc_us %= sw_ctx->period_us;
                    if(!sw_ctx->repeat){
                        sw_ctx->start = 0;
                    }
                    last_node = node;
                    node = node->next;
                }else{
                    // CC_LOGW(TAG, "curr timer is delete");
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

cc_err_t cc_timer_init(void){
    g_semphr_handle = cc_hal_os_semphr_create_mutex();
    if(NULL == g_semphr_handle){
        CC_LOGE(TAG, "semphr create error");
        return CC_FAIL;
    }
    cc_hal_os_semphr_give(g_semphr_handle);
    return CC_OK;
}

static void _simple_one_timer_cb(void *arg){
    cc_timer_config_t *config = (cc_timer_config_t *)arg;
    if(NULL != config->callback){
        config->callback(config->arg);
    }
    cc_timer_delete(NULL);
}

cc_err_t cc_timer_simple_one(cc_timer_type_t type, cc_timer_cb_t callback, uint64_t us, void* arg){

    cc_timer_config_t one_config = {0};

    cc_timer_config_t *cp_config =  cc_hal_sys_malloc(sizeof(cc_timer_config_t));
    if(NULL == cp_config ){
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        return CC_ERR_NO_MEM;
    }

    cp_config->type = type;
    cp_config->callback = callback;
    cp_config->arg = arg;

    one_config.type = cp_config->type;
    one_config.callback = _simple_one_timer_cb;
    one_config.arg = cp_config;

    cc_timer_handle_t handle = cc_timer_create(&one_config);
    if(NULL == handle){
        CC_LOGE_CODE(TAG, CC_FAIL);
        cc_hal_sys_free(cp_config);
        return CC_FAIL;
    }

    cc_timer_start_once(handle, us);

    return CC_OK;
}