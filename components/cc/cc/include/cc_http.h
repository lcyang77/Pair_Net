

/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:41
 * @Last Modified time: 2022-03-20 18:41:41
 */

#ifndef __CC_HTTP_H__
#define __CC_HTTP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#define CC_HTTP_HEADER_CONNECT_TXT    "content-type: text/plain\r\n"
#define CC_HTTP_HEADER_CONNECT_JSON   "content-type: application/json\r\n"

typedef void (*cc_http_event_cb_t)(void *arg, uint8_t event, void *data);
typedef void (*cc_http_req_cb_t)(void *arg, int resp_code, uint8_t *buf, uint16_t len);

typedef struct{
    char *url;
    uint8_t method;
    char *header;
    uint8_t *post_buf;
    uint16_t post_buf_len;
    uint8_t *resp_buf;
    uint16_t resp_buf_len;
    cc_http_req_cb_t req_cb;
    cc_http_event_cb_t event_cb;
    uint8_t auto_free;
    uint8_t retry_cnt;
    uint16_t retry_wait_time;
    void* arg;
}cc_http_request_t;

#define CC_HTTP_REQUEST_DEFAULT {\
    .header = NULL,             \
    .post_buf = NULL,           \
    .post_buf_len = 0,          \
    .resp_buf = NULL,           \
    .resp_buf_len = 0,          \
    .req_cb = NULL,             \
    .event_cb = NULL,           \
    .auto_free = 1,             \
    .retry_cnt = 3,             \
    .retry_wait_time = 1000,    \
}

cc_err_t cc_http_simple_get(char *url, cc_http_req_cb_t req_cb, void *arg);
cc_err_t cc_http_simple_post_json_str(char *url, char *post_buf, cc_http_req_cb_t req_cb, void *arg);
cc_err_t cc_http_request(cc_http_request_t request);

void cc_http_run(uint16_t interval);

#ifdef __cplusplus
}
#endif

#endif  //__CC_EVENT_H__