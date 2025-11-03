#ifndef __CC_LOG_H__
#define __CC_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

#include "esp_log.h"

#define CC_LOGD         ESP_LOGI 
#define CC_LOGI         ESP_LOGI 
#define CC_LOGW         ESP_LOGW 
#define CC_LOGE         ESP_LOGE

#define CC_LOGD_CODE(tag, code)         ESP_LOGI(tag, "err: %d", (int)code)
#define CC_LOGI_CODE(tag, code)         ESP_LOGI(tag, "err: %d", (int)code)
#define CC_LOGW_CODE(tag, code)         ESP_LOGW(tag, "err: %d", (int)code)
#define CC_LOGE_CODE(tag, code)         ESP_LOGE(tag, "err: %d", (int)code)

#define cc_log_write                    printf

#define CC_LOGD_HEXDUMP(tag, buf, len)                 ESP_LOG_BUFFER_HEXDUMP(tag, buf, len, ESP_LOG_INFO)
#define CC_LOGI_HEXDUMP(tag, buf, len)                 ESP_LOG_BUFFER_HEXDUMP(tag, buf, len, ESP_LOG_INFO)
#define CC_LOGW_HEXDUMP(tag, buf, len)                 ESP_LOG_BUFFER_HEXDUMP(tag, buf, len, ESP_LOG_WARN)
#define CC_LOGE_HEXDUMP(tag, buf, len)                 ESP_LOG_BUFFER_HEXDUMP(tag, buf, len, ESP_LOG_ERROR)

#ifdef __cplusplus
}
#endif

#endif
