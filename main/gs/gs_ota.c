#include "gs_ota.h"

#include <string.h>

#include "cc_log.h"
#include "cc_hal_sys.h"
#include "cc_hal_ota.h"
#include "cc_hal_os.h"
#include "cc_timer.h"

#include "http_client.h"

static char *TAG = "gs_ota";

CC_EVENT_DEFINE_BASE(GS_OTA_EVENT);

static char g_ota_ing = 0; 
static char g_ota_url[256] = ""; 
static int g_update_idx = 0;
static cc_err_t g_hal_ota_err = CC_FAIL;

static void __timer_cb_for_reboot(void *arg){
    cc_hal_sys_reboot();
}

static void __event_cb(http_client_t *client, HTTP_EVENT event, void *data){

    switch (event)
    {
    case HTTP_EVENT_ERR:

        break;
    case HTTP_EVENT_DNS_GET_IP:
        cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_HTTP_DNS_GET_IP, NULL, 0);
        break;
    case HTTP_EVENT_CONNECTED:
        cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_HTTP_CONNECTED, NULL, 0);
        break;
    default:
        break;
    }
}

static HTTPC_RESULT __recv_cb(http_client_t *client, http_client_data_t *client_data){

    static uint8_t last_progress = 101;

    if(g_hal_ota_err != CC_OK){
        return HTTP_EUNKOWN;
    }
    
    uint32_t progress = ((client_data->response_content_len - client_data->retrieve_len) *100)/client_data->response_content_len;
    if(last_progress != progress){
        if(g_update_idx == 0 && progress == 100){
            return HTTP_EUNKOWN;
        }

        CC_LOGI(TAG, "ota progress: %d", progress);
        last_progress = progress;
        if(progress == 0){
            cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_HTTP_GET_DTAT_START, NULL, 0);
        }
        cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_HTTP_GET_DTAT_PROGRESS, (void *)&progress, sizeof(progress));
        if(progress == 100){
            cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_HTTP_GET_DTAT_FINISH, NULL, 0); 
        }
    }

    g_hal_ota_err = cc_hal_ota_update(g_update_idx, (uint8_t *)client_data->response_buf, client_data->response_len);
    if(g_hal_ota_err != CC_OK){
        return HTTP_EUNKOWN;
    }

    g_update_idx += client_data->response_len;

    return HTTP_SUCCESS;
}

static void __connect_cb(http_client_t *client){
   
}

static void __close_cb(http_client_t *client){

}

void __ota_http_task(void *arg){
    http_client_t http_client = {0};
    http_client_data_t client_data = {0};
    http_client_cb_t client_cb = {0};

    CC_LOGI(TAG, "ota_begin");
    g_update_idx = 0;
    g_hal_ota_err = cc_hal_ota_begin();

    client_data.response_buf = cc_hal_sys_malloc(2048 + 1);
    if(client_data.response_buf == NULL){
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        return;
    }
    client_data.response_buf_len = 2048 + 1;

    client_cb.event_cb = __event_cb;
    client_cb.connect_cb = __connect_cb;
    client_cb.recv_cb = __recv_cb;
    client_cb.close_cb = __close_cb;

    cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_HTTP_START, NULL, 0);

    HTTPC_RESULT status = http_client_get_request(&http_client, g_ota_url, &client_data, &client_cb);
    if(status != HTTP_SUCCESS && status != HTTP_EAGAIN){
        CC_LOGE_CODE(TAG, CC_FAIL);
        status = http_client_get_request(&http_client, g_ota_url, &client_data, &client_cb);
    }

    CC_LOGI(TAG, "ota_end");
    g_hal_ota_err = cc_hal_ota_end();

    if(status == HTTP_SUCCESS && g_hal_ota_err == CC_OK){
        cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_SUCCESS, NULL, 0);
    }else{
        cc_event_post(GS_OTA_EVENT, GS_OTA_EVENT_FAIL, NULL, 0);
    }

    cc_timer_handle_t g_reboot_timer_handle = NULL;
    cc_timer_config_t timer_config = {
        .arg = NULL,
        .callback = __timer_cb_for_reboot,
        .type = CC_TIMER_TYPE_SW
    };

    g_reboot_timer_handle = cc_timer_create(&timer_config);
    if(g_reboot_timer_handle){
        cc_timer_start_once(g_reboot_timer_handle, CC_TIMMER_MS(1000));
    }

    g_ota_ing = 0;

    cc_hal_os_task_delete(NULL);
}


cc_err_t gs_ota_for_http(char *url){

    if(g_ota_ing == 1){
        return CC_FAIL;
    }

    g_ota_ing = 1;

    strcpy(g_ota_url, url);

    for (size_t i = 0; i < strlen(g_ota_url); i++){
        if(g_ota_url[i] == ' '){
            g_ota_url[i] = '&';
        } 
    }
    
    cc_hal_os_task_create(__ota_http_task, "__ota_task", 4096, NULL, 4, NULL);
    return CC_OK;
}

cc_err_t gs_ota_init(void){
    return CC_OK;
}