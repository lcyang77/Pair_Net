#ifndef __CC_HAL_SYS_H__
#define __CC_HAL_SYS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "cc_err.h"

typedef enum{
    CC_RST_POWER_OFF = 0,            //电源重启 
    CC_RST_HARDWARE,                 //硬件复位 
    CC_RST_SOFTWARE,                 //软件复位 
    CC_RST_HARDWARE_WATCHDOG,        //硬件看门狗复位 
    CC_RST_SOFTWARE_WATCHDOG,        //软件看门狗重启 
    CC_RST_FATAL_EXCEPTION,          //异代码常 
    CC_RST_DEEPSLEEP,                //深度睡眠 
    CC_RST_OTHER,                    //其它原因 
    CC_RST_UNSUPPORT,                //不支持 
}cc_rst_reason_t;

void *cc_hal_sys_malloc(size_t size);
void cc_hal_sys_free(void *ptr);

uint32_t cc_hal_sys_get_rand(uint32_t range);

uint64_t cc_hal_sys_get_ms(void);

void cc_hal_sys_set_time(uint32_t time);
uint32_t cc_hal_sys_get_time(void);

cc_err_t cc_hal_sys_md5(uint8_t *input, uint8_t len, uint8_t output[16]);

cc_err_t cc_hal_sys_hmac(const char* algorithm, uint8_t *data, uint16_t data_len, uint8_t *key, uint16_t key_len, uint8_t *result);

cc_err_t cc_hal_sys_reboot(void);

cc_rst_reason_t cc_hal_sys_get_rst_reason(void);

cc_err_t cc_hal_sys_init(void);

#ifdef __cplusplus
}
#endif

#endif