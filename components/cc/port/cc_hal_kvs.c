#define CONFIG_CC_FEIL_LEVEL    CC_LOG_ERROR

#include "cc_hal_kvs.h"

#include <string.h>
#include <stdbool.h>

#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "cc_log.h"

static char *TAG = "hal_kvs";

#define NVS_PARTITION_NAME  "nvs"
#define NVS_KV              "cc_iot_kv"

static bool s_kv_init_flag;

cc_err_t cc_hal_kvs_init(void){
	esp_err_t ret = ESP_OK;

    do {
        if (s_kv_init_flag == false) {
            ret = nvs_flash_init_partition(NVS_PARTITION_NAME);

            if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
                ESP_ERROR_CHECK(nvs_flash_erase_partition(NVS_PARTITION_NAME));
                ret = nvs_flash_init_partition(NVS_PARTITION_NAME);
            } else if (ret != ESP_OK) {
                CC_LOGE(TAG, "NVS Flash init %s failed!", NVS_PARTITION_NAME);
                break;
            }

            s_kv_init_flag = true;
        }
    } while (0);

    return (ret==ESP_OK)?CC_OK:CC_FAIL;
}

cc_err_t cc_hal_kvs_get(const char *key, void *value_buf, size_t *buf_len){
    nvs_handle handle;
    esp_err_t ret;

    char key_name[16] = {0};

    if (key == NULL || value_buf == NULL || buf_len == NULL) {
        CC_LOGE(TAG, "HAL_Kvs_Get Null params");
        return CC_FAIL;
    }

    if (cc_hal_kvs_init() != ESP_OK) {
        return CC_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READONLY, &handle);

    if (ret != ESP_OK) {
        CC_LOGW(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return CC_FAIL;
    }
    /*max key name is 15UL*/
    memcpy(key_name, key, sizeof(key_name) - 1);
    CC_LOGI(TAG, "Get %s blob", key_name);
    ret = nvs_get_blob(handle, key_name, value_buf, (size_t *) buf_len);

    if (ret != ESP_OK) {
        CC_LOGW(TAG, "nvs get blob %s failed with %x", key_name, ret);
    }

    nvs_close(handle);

    return (ret==ESP_OK)?CC_OK:CC_FAIL;
}

cc_err_t cc_hal_kvs_set(const char *key, const void *value_buf, size_t buf_len){
    nvs_handle handle;
    esp_err_t ret;

    char key_name[16] = {0};

    if (key == NULL || value_buf == NULL || buf_len <= 0) {
        CC_LOGE(TAG, "HAL_Kvs_Set NULL params");
        return CC_FAIL;
    }

    if (cc_hal_kvs_init() != ESP_OK) {
        return CC_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        CC_LOGE(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return CC_FAIL;
    }
    /*max key name is 15UL*/
    memcpy(key_name, key, sizeof(key_name) - 1);
    CC_LOGI(TAG, "Set %s blob", key_name);
    ret = nvs_set_blob(handle, key_name, value_buf, buf_len);

    if (ret != ESP_OK) {
        CC_LOGE(TAG, "nvs erase key %s failed with %x", key_name, ret);
    } else {
        nvs_commit(handle);
    }

    nvs_close(handle);

    return (ret==ESP_OK)?CC_OK:CC_FAIL;
}

cc_err_t cc_hal_kvs_del(const char *key){
    nvs_handle handle;
    esp_err_t ret;

    char key_name[16] = {0};

    if (key == NULL) {
        CC_LOGE(TAG, "HAL_Kvs_Del Null key");
        return CC_FAIL;
    }

    if (cc_hal_kvs_init() != ESP_OK) {
        return CC_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        CC_LOGE(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return CC_FAIL;
    }

    /*max key name is 15UL*/
    memcpy(key_name, key, sizeof(key_name) - 1);
    CC_LOGI(TAG, "Del %s blob", key_name);
    ret = nvs_erase_key(handle, key_name);

    if (ret != ESP_OK) {
        CC_LOGE(TAG, "nvs erase key %s failed with %x", key_name, ret);
    } else {
        nvs_commit(handle);
    }

    nvs_close(handle);

    return (ret==ESP_OK)?CC_OK:CC_FAIL;
}

cc_err_t cc_hal_kvs_del_all(void){
    nvs_handle handle;
    esp_err_t ret;

    if (cc_hal_kvs_init() != ESP_OK) {
        return CC_FAIL;
    }

    ret = nvs_open_from_partition(NVS_PARTITION_NAME, NVS_KV, NVS_READWRITE, &handle);

    if (ret != ESP_OK) {
        CC_LOGE(TAG, "nvs open %s failed with %x", NVS_KV, ret);
        return CC_FAIL;
    }

    ret = nvs_erase_all(handle);

    if (ret != ESP_OK) {
        CC_LOGE(TAG, "nvs erase all failed with %x", ret);
    } else {
        nvs_commit(handle);
    }

    nvs_close(handle);

    return (ret==ESP_OK)?CC_OK:CC_FAIL;
}