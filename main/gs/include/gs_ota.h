#ifndef __GS_OTA_H__
#define __GS_OTA_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#include "cc_event.h"

CC_EVENT_DECLARE_BASE(GS_OTA_EVENT);

typedef enum{
    GS_OTA_EVENT_HTTP_START = 0,
    GS_OTA_EVENT_HTTP_DNS_GET_IP,
    GS_OTA_EVENT_HTTP_CONNECTED,
    GS_OTA_EVENT_HTTP_GET_DTAT_START,
    GS_OTA_EVENT_HTTP_GET_DTAT_PROGRESS,
    GS_OTA_EVENT_HTTP_GET_DTAT_FINISH,
    GS_OTA_EVENT_SUCCESS,
    GS_OTA_EVENT_FAIL,
}gs_ota_event_t;

cc_err_t gs_ota_for_http(char *url);

cc_err_t gs_ota_init(void);

#ifdef __cplusplus
}
#endif

#endif