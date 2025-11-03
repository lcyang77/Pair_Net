#include "cc_hal_sys.h"

#include <stdlib.h>

#include "cc_log.h"

#include "cc_list.h"
#include "cc_tmr_task.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"

#include "mbedtls/md.h"
#include "mbedtls/cipher.h"

#include "mbedtls/md5.h"
#include "mbedtls/sha1.h"

static char *TAG = "hal_sys";

void *cc_hal_sys_malloc(size_t size){
    return malloc(size);
}

void cc_hal_sys_free(void *ptr){
    free(ptr);
}

uint32_t cc_hal_sys_get_rand(uint32_t range){
    return (range != 0) ? (rand() % range) : 0;
}

uint64_t cc_hal_sys_get_ms(void){
    return xTaskGetTickCount()*portTICK_PERIOD_MS;
}

void cc_hal_sys_set_time(uint32_t time){
    
}

uint32_t cc_hal_sys_get_time(void){
    return 0;
}

cc_err_t cc_hal_sys_md5(uint8_t *input, uint8_t len, uint8_t output[16]){
    return CC_ERR_NOT_SUPPORTED;
}

cc_err_t cc_hal_sys_hmac(const char* algorithm, uint8_t *data, uint16_t data_len, uint8_t *key, uint16_t key_len, uint8_t *result){
    const mbedtls_md_info_t *mdInfo = NULL;

    mdInfo = mbedtls_md_info_from_string(algorithm);
    if( mdInfo == NULL ){
        return CC_FAIL;
    }
    int err = mbedtls_md_hmac(mdInfo, (unsigned char *)key, (size_t)key_len, (unsigned char *)data, (size_t)data_len, (unsigned char *)result);
    if (err != CC_OK){
        return CC_FAIL;
    }
    return CC_OK;
}

cc_err_t cc_hal_sys_reboot(void){
    esp_restart();
    return CC_OK;
}

cc_rst_reason_t cc_hal_sys_get_rst_reason(void){
    return CC_RST_UNSUPPORT;
}

cc_err_t cc_hal_sys_init(void){
    uint32_t time =  cc_hal_sys_get_time();
    if(time < 1672502400){
        cc_hal_sys_set_time(1672502400);
    }
    return CC_OK;
}