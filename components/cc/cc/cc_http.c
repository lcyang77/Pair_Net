/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#include "cc_http.h"

#include <string.h>

#include "cc_list.h"
#include "cc_log.h"

#include "cc_hal_sys.h"
#include "cc_hal_network.h"

static char* TAG = "cc_http";

#define CC_HTTP_STA_IDEL            0
#define CC_HTTP_STA_BEGIN           1
#define CC_HTTP_STA_RESTART         2
#define CC_HTTP_STA_WAIT            3
#define CC_HTTP_STA_WAIT_NETCONN    4
#define CC_HTTP_STA_START   	    5
#define CC_HTTP_STA_WAIT_CB   	    6
#define CC_HTTP_STA_STOP   	        7

typedef struct{
    uint8_t sta;
    uint8_t retry_cnt;
    uint8_t max_retry_cnt;
    uint16_t wait_tick;
    uint16_t retry_wait_time;
    cc_http_t *client;
    cc_http_request_t *request;
}_http_ctx_t;

cc_list_node *g_http_list = NULL;

int __list_find_by_client_fun(cc_list_node *list, void *arg){

    cc_http_t *client = (cc_http_t *)arg;

    _http_ctx_t *http_ctx = list->data;
    if(!http_ctx){
        return 0; 
    }
    if(http_ctx->client == client){
        return 1;
    }else{
        return 0;
    }
}

static cc_err_t __request_finish_cb(void *arg, uint16_t resp_code){

    cc_http_t *client = (cc_http_t *)arg;

    CC_LOGD(TAG, "http ResponseCode: %d   resp_len: %d", resp_code, client->resp_len);

    cc_list_node *node = cc_list_find(g_http_list, __list_find_by_client_fun, client);
    if(node == NULL){
        return CC_FAIL;
    }

    _http_ctx_t *http_ctx = node->data;

    if(client->resp_len){
        if(http_ctx->request->req_cb){
            http_ctx->request->req_cb(http_ctx->request->arg, resp_code, (uint8_t *)client->resp_buf, client->resp_len);
        }
        if(resp_code != 200){
            http_ctx->sta = CC_HTTP_STA_RESTART;
        }else{
            http_ctx->sta = CC_HTTP_STA_STOP;
        }
    }else{
        http_ctx->sta = CC_HTTP_STA_RESTART;
    }

    return CC_OK;
}

static void __request_event_cb(void *arg, uint8_t event, void *data){
    cc_http_t *client = (cc_http_t *)arg;

    cc_list_node *node = cc_list_find(g_http_list, __list_find_by_client_fun, client);
    if(node == NULL){
        return;
    }

    _http_ctx_t *http_ctx = node->data;
    switch (event)
    {
    case CC_HTTP_EVENT_ERROR:{
        uint8_t *err = (uint8_t *)data;
        CC_LOGD(TAG, "CC_HTTP_EVENT_ERROR: %d", *err);
        }break;
    case CC_HTTP_EVENT_DNS_GET_IP:{
        uint8_t *ip = (uint8_t *)data;
        CC_LOGD(TAG, "CC_HTTP_EVENT_DNS_GET_IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        }break;
    case CC_HTTP_EVENT_CONNECTED:
        CC_LOGD(TAG, "CC_HTTP_EVENT_CONNECTED");
        break;
    case CC_HTTP_EVENT_FINISH:{
        int *response_code = (int *)data;
        CC_LOGD(TAG, "CC_HTTP_EVENT_FINISH: %d", *response_code);
        }break;
    default:
        break;
    }

    if(http_ctx->request->event_cb){
        http_ctx->request->event_cb(http_ctx->request->arg, event, data);
    }
}

cc_http_t *__create_client(cc_http_request_t *request){
    cc_http_t *client = NULL;

    client = (cc_http_t *)(cc_hal_sys_malloc(sizeof(cc_http_t)));
    if(!client){
        return NULL;
    }
    memset(client, 0, sizeof(cc_http_t));

    client->arg = client;

    client->url = request->url;

    client->method = request->method;
    client->header = request->header;

    client->recv_cb = NULL;
    client->close_cb = __request_finish_cb;
    client->event_cb = __request_event_cb;

    client->post_buf = request->post_buf;
    client->post_buf_len = request->post_buf_len;

    client->resp_buf = (char *)request->resp_buf;
    client->resp_buf_len = request->resp_buf_len;
    return client;
}

cc_err_t __start_request(cc_http_request_t* request){

    _http_ctx_t *http_ctx = (_http_ctx_t *)cc_hal_sys_malloc(sizeof(_http_ctx_t));
    if(!http_ctx){
        return CC_ERR_NO_MEM;
    }

    cc_http_t *client = __create_client(request);
    if(!client){
        cc_hal_sys_free(http_ctx);
        return CC_ERR_NO_MEM;
    }

    http_ctx->client = client;
    http_ctx->request = request;
    http_ctx->sta = CC_HTTP_STA_BEGIN;
    http_ctx->max_retry_cnt = request->retry_cnt;
    http_ctx->retry_wait_time = request->retry_wait_time;

    http_ctx->retry_cnt = 0;
    http_ctx->wait_tick = 0;

    if(g_http_list == NULL){
        g_http_list = cc_list_create(http_ctx);
    }else{
        cc_list_insert_end(g_http_list, http_ctx);
    }

    return CC_OK;
}

cc_err_t cc_http_request(cc_http_request_t request){
    cc_err_t err = CC_OK;
    cc_http_request_t *request_cp = (cc_http_request_t *)cc_hal_sys_malloc(sizeof(cc_http_request_t));
    if(!request_cp){
        return CC_ERR_NO_MEM;
    }

    memcpy(request_cp, &request, sizeof(cc_http_request_t));
    err = __start_request(request_cp);
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
        cc_hal_sys_free(request_cp);
        return CC_ERR_NO_MEM;
    }

    return CC_OK;
}

cc_err_t cc_http_simple_post_json_str(char *url, char *post_buf, cc_http_req_cb_t req_cb, void *arg){
    cc_err_t err =  CC_FAIL;
    uint8_t *resp_buf = NULL;
    uint16_t resp_buf_len = 2048;
    uint8_t *post_cp_buf = NULL;
    uint16_t post_buf_len = 0;

    resp_buf = cc_hal_sys_malloc(resp_buf_len);    
    if(!resp_buf){
        return CC_ERR_NO_MEM;
    }

    post_buf_len = strlen(post_buf);
    post_cp_buf = cc_hal_sys_malloc(post_buf_len);    
    if(!post_cp_buf){
        cc_hal_sys_free(resp_buf);
        return CC_ERR_NO_MEM;
    }
    memcpy(post_cp_buf, post_buf, post_buf_len);

    CC_LOGD(TAG, "url: %s, post_buf: %s", url, post_buf);

    char *request_url = (char *)(cc_hal_sys_malloc(strlen(url)+1));
    if(!request_url){
        cc_hal_sys_free(resp_buf);
        cc_hal_sys_free(post_cp_buf);
        return CC_ERR_NO_MEM;
    }
    strcpy(request_url, url);

    cc_http_request_t *request = (cc_http_request_t *)(cc_hal_sys_malloc(sizeof(cc_http_request_t)));
    if(!request){
        cc_hal_sys_free(resp_buf);
        cc_hal_sys_free(post_cp_buf);
        cc_hal_sys_free(request_url);
        return CC_ERR_NO_MEM;
    }

    request->url = request_url;
    request->method = CC_HTTP_METHOD_POST;
    request->header = CC_HTTP_HEADER_CONNECT_JSON;
    request->post_buf = post_cp_buf;
    request->post_buf_len = post_buf_len;
    request->resp_buf = resp_buf;
    request->resp_buf_len = resp_buf_len;
    request->req_cb = req_cb;
    request->event_cb = NULL;
    request->auto_free = 1;
    request->retry_cnt = 3;
    request->retry_wait_time = 100;
    request->arg = arg;

    err = __start_request(request);
    if(err != CC_OK){
        cc_hal_sys_free(resp_buf);
        cc_hal_sys_free(request_url);
        return err;
    }

    return CC_OK;
}


cc_err_t cc_http_simple_get(char *url, cc_http_req_cb_t req_cb, void *arg){
    cc_err_t err =  CC_FAIL;
    uint16_t resp_buf_len = 2048;
    uint8_t *resp_buf = cc_hal_sys_malloc(resp_buf_len);    
    if(!resp_buf){
        return CC_FAIL;
    }

    char *request_url = (char *)(cc_hal_sys_malloc(strlen(url)+1));
    if(!request_url){
        return CC_ERR_NO_MEM;
    }
    strcpy(request_url, url);

    cc_http_request_t *request = (cc_http_request_t *)(cc_hal_sys_malloc(sizeof(cc_http_request_t)));
    if(!request){
        cc_hal_sys_free(resp_buf);
        cc_hal_sys_free(request_url);
        return CC_ERR_NO_MEM;
    }

    request->url = request_url;
    request->method = CC_HTTP_METHOD_GET;
    request->header = NULL;
    request->post_buf = NULL;
    request->post_buf_len = 0;
    request->resp_buf = resp_buf;
    request->resp_buf_len = resp_buf_len;
    request->req_cb = req_cb;
    request->event_cb = NULL;
    request->auto_free = 1;
    request->retry_cnt = 3;
    request->retry_wait_time = 100;
    request->arg = arg;

    err = __start_request(request);
    if(err != CC_OK){
        cc_hal_sys_free(resp_buf);
        cc_hal_sys_free(request_url);
        return err;
    }

    return CC_OK;
}

void cc_http_run(uint16_t interval){
    cc_err_t ret = CC_OK;

    cc_list_node *del_node = NULL;

    cc_list_node *node = g_http_list;
    while(node) {
        _http_ctx_t *http_ctx = node->data;
        switch (http_ctx->sta) {
            case CC_HTTP_STA_IDEL:
                break;
            case CC_HTTP_STA_BEGIN:
                CC_LOGD(TAG, "http_begin url: %s", http_ctx->client->url);
                http_ctx->retry_cnt = 0;
                http_ctx->wait_tick = 0;
                http_ctx->sta = CC_HTTP_STA_WAIT_NETCONN;
                break;
            case CC_HTTP_STA_RESTART:
                CC_LOGD(TAG, "http_restart url: %s", http_ctx->client->url);
                http_ctx->wait_tick = 0;
                http_ctx->sta = CC_HTTP_STA_WAIT;
            case CC_HTTP_STA_WAIT:
                http_ctx->wait_tick = http_ctx->wait_tick + interval;
                if(http_ctx->wait_tick++ >= http_ctx->retry_wait_time){
                    http_ctx->sta = CC_HTTP_STA_WAIT_NETCONN;
                }
                break;
            case CC_HTTP_STA_WAIT_NETCONN:
                // if(cc_wifi_get_connect_status()){
                    http_ctx->sta = CC_HTTP_STA_START;
                // }
                break;
            case CC_HTTP_STA_START:
                http_ctx->retry_cnt++;
                CC_LOGD(TAG, "http_start cnt: %d url: %s", http_ctx->retry_cnt, http_ctx->client->url);
                if(http_ctx->retry_cnt < http_ctx->max_retry_cnt){
                    http_ctx->sta = CC_HTTP_STA_WAIT_CB;
                    ret = cc_hal_http_connect(http_ctx->client);
                    if(ret != CC_OK){
                        cc_http_request_t *request = http_ctx->request;
                        if(request->req_cb){
                            request->req_cb(request->arg, -1, (uint8_t *)http_ctx->client->resp_buf, 0);
                        }
                        http_ctx->sta = CC_HTTP_STA_STOP;
                    }
                }else{
                    cc_http_request_t *request = http_ctx->request;
                    if(request->req_cb){
                        request->req_cb(request->arg, -1, (uint8_t *)http_ctx->client->resp_buf, 0);
                    }
                    http_ctx->sta = CC_HTTP_STA_STOP;
                }
                break;
            case CC_HTTP_STA_WAIT_CB:
                
                break;
            case CC_HTTP_STA_STOP:
                CC_LOGD(TAG, "http_stop url: %s", http_ctx->client->url);
                cc_hal_sys_free(http_ctx->client);
                if(http_ctx->request->auto_free){
                    cc_hal_sys_free(http_ctx->request->url);
                    if(http_ctx->request->header && (http_ctx->request->header != (char *)CC_HTTP_HEADER_CONNECT_TXT && http_ctx->request->header != (char *)CC_HTTP_HEADER_CONNECT_JSON)){
                        cc_hal_sys_free(http_ctx->request->header);
                    }
                    if(http_ctx->request->post_buf){
                        cc_hal_sys_free(http_ctx->request->post_buf);
                    }
                    if(http_ctx->request->resp_buf){
                        cc_hal_sys_free(http_ctx->request->resp_buf);
                    }
                }
                cc_hal_sys_free(http_ctx->request);
                cc_hal_sys_free(http_ctx);
                del_node = node;
                node = node->next;
                cc_list_remove(&g_http_list, del_node);
                continue;
            default:
                break;
        }

        node = node->next;
    }
}   