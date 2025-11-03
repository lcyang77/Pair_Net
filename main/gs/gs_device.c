#include "gs_device.h"

#include <string.h>

#include "cc_hal_sys.h"
#include "cc_hal_kvs.h"

#include "cc_log.h"
#include "cc_event.h"
#include "cc_http.h"
#include "cc_tmr_task.h"

#include "cJSON.h"

#define DEV_KVS_KEY    "gs_dev"

static char *TAG = "gs_device";

CC_EVENT_DEFINE_BASE(GS_DEVICE_EVENT);

typedef struct 
{
    char product_key[GS_PRODUCT_KEY_BUF_MAX_LEN];
    char device_name[GS_DEVICE_NAME_BUF_MAX_LEN];
    char device_secret[GS_DEVICE_SECRET_BUF_MAX_LEN];
}_dev_triple_t;

_dev_triple_t g_dev_triple = {.product_key = "", .device_name = "", .device_secret = ""};

static char g_token[GS_TOKEN_BUF_MAX_LEN] = "";

static char g_sw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
static char g_hw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";

static void __get_time_task(uint32_t interval, void *arg);

static void __get_license_http_cb(void *arg, int resp_code, uint8_t *buf, uint16_t len){
    CC_LOGD(TAG, "__get_license_http_cb: %s", buf);

    cJSON *root_obj = NULL, *status_obj = NULL, *data_obj = NULL, *product_key_obj = NULL, *device_name_obj = NULL, *device_secret_obj = NULL;

    if(resp_code != 200){
        CC_LOGE(TAG, "resp_code: %d", resp_code);
        if(resp_code == -1){
            CC_LOGE(TAG, "license_http error");
            goto fail;
        }
        return;
    }

    buf[len] = '\0';
    root_obj = cJSON_Parse((char *)buf);
    if(root_obj == NULL){
        CC_LOGE(TAG, "cJSON_Parse error");
        goto fail;
    }

    status_obj = cJSON_GetObjectItem(root_obj, "status");
    if(status_obj == NULL || status_obj->type != cJSON_Number){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        goto fail;
    }

    if(status_obj->valueint != 0){
        CC_LOGE_CODE(TAG, CC_FAIL);
        goto fail;
    }

    data_obj = cJSON_GetObjectItem(root_obj, "data");
    if(data_obj == NULL || data_obj->type != cJSON_Object){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        goto fail;
    }

    product_key_obj = cJSON_GetObjectItem(data_obj, "product_key");
    device_name_obj = cJSON_GetObjectItem(data_obj, "device_name");
    device_secret_obj = cJSON_GetObjectItem(data_obj, "device_secret");
    if(product_key_obj == NULL || product_key_obj->type != cJSON_String ||
            device_name_obj == NULL || device_name_obj->type != cJSON_String ||
            device_secret_obj == NULL || device_secret_obj->type != cJSON_String){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        goto fail;
    }

    CC_LOGI(TAG, "product_key: %s", product_key_obj->valuestring);
    CC_LOGI(TAG, "device_name: %s", device_name_obj->valuestring);
    CC_LOGI(TAG, "device_secret: %s", device_secret_obj->valuestring);

    strcpy(g_dev_triple.product_key, product_key_obj->valuestring);
    strcpy(g_dev_triple.device_name, device_name_obj->valuestring);
    strcpy(g_dev_triple.device_secret, device_secret_obj->valuestring);
    len = GS_PRODUCT_KEY_BUF_MAX_LEN;
    cc_hal_kvs_set("gs_product_key", g_dev_triple.product_key, len);
    len = GS_DEVICE_NAME_BUF_MAX_LEN;
    cc_hal_kvs_set("gs_device_name", g_dev_triple.device_name, len);
    len = GS_DEVICE_SECRET_BUF_MAX_LEN;
    cc_hal_kvs_set("gs_device_secret", g_dev_triple.device_secret, len);

    cc_event_post(GS_DEVICE_EVENT, GS_DEVICE_EVENT_GET_LICENSE_SUCCESS, NULL, 0);
    
    cJSON_Delete(root_obj);
    return;

fail:
    cc_event_post(GS_DEVICE_EVENT, GS_DEVICE_EVENT_GET_LICENSE_FAIL, NULL, 0);

    if(root_obj){
        cJSON_Delete(root_obj);
    }
}

static void __get_license_task(uint32_t interval, void *arg){
    char url_buf[256] = "";

    char data[32] = "";
    uint8_t hash[20] = "";
    char sign[41] = "";
    char *uname = CONFIG_GAOSI_LICENSE_UNAME;
    char *key = CONFIG_GAOSI_LICENSE_UKEY;

    uint32_t timestamp = cc_hal_sys_get_time();

    sprintf(data, "time=%ld&uname=%s", timestamp, uname);
    cc_err_t err =  cc_hal_sys_hmac("SHA1", (uint8_t *)data, strlen(data), (uint8_t *)key, strlen(key), hash);
    if(err == CC_OK){
        for (int i = 0; i < 20; i++) {
            sprintf(sign + 2 * i, "%02x", hash[i]);
        }
        sign[40] = '\0';

        sprintf(url_buf, "http://xxx.xxx.xx/app/api/?c=license&a=getLicenseInfo&uname=%s&time=%ld&sign=%s", uname, timestamp, sign);
        // sprintf(url_buf, "http://192.168.x.xxx:8080/app/api/?c=license&a=getLicenseInfo&uname=%s&time=%d&sign=%s", uname, timestamp, sign);
        CC_LOGD(TAG, "get license: %s", url_buf);
        cc_http_simple_get(url_buf, __get_license_http_cb, NULL);
    }else{
        CC_LOGE_CODE(TAG, err);
    }
    cc_tmr_task_delete(__get_license_task);
}

static void __get_time_http_cb(void *arg, int resp_code, uint8_t *buf, uint16_t len){
    CC_LOGD(TAG, "_get_list_http_cb recv len: %d  data: %.*s", len, len, buf);

    cJSON *root_obj = NULL, *time_obj = NULL;

    if(resp_code != 200){
        CC_LOGE(TAG, "resp_code: %d", resp_code);
        return;
    }

    buf[len] = '\0';
    root_obj = cJSON_Parse((char *)buf);
    if(root_obj == NULL){
        CC_LOGE(TAG, "cJSON_Parse error");
        return;
    }

    time_obj = cJSON_GetObjectItem(root_obj, "time");
    if(time_obj == NULL || time_obj->type != cJSON_Number){
        cJSON_Delete(root_obj);
        return;
    }

    cc_hal_sys_set_time(time_obj->valueint);

    CC_LOGI(TAG, "get time: %ld", time_obj->valueint);

    // cc_tmr_task_set_interval(__get_time_task, 60*60*1000);
    if(strlen(g_dev_triple.product_key) == 0){
        cc_tmr_task_create(__get_license_task, 100, NULL);
    }

    cJSON_Delete(root_obj);
}


void __get_time_task(uint32_t interval, void *arg){
    char url_buf[128] = "";

    CC_LOGI(TAG, "start get time");
    sprintf(url_buf, "http://xxx.xxx.xx/mqtt/getTime.php?device_name=%s", g_dev_triple.device_name);
    cc_http_simple_get(url_buf, __get_time_http_cb, NULL);
    cc_tmr_task_delete(__get_time_task);
}

cc_err_t gs_device_get_license(void){
    if(strlen(g_dev_triple.product_key) == 0){
        cc_tmr_task_create(__get_time_task, 100, NULL);
    }
    return CC_OK;
}

cc_err_t gs_device_save_token(char *token){
    if(strnlen(token, GS_TOKEN_BUF_MAX_LEN) == GS_TOKEN_BUF_MAX_LEN){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return CC_ERR_INVALID_ARG;
    }

    CC_LOGI(TAG, "gs_device_save_token: %s", token);

    strcpy(g_token, token);
    return CC_OK;
}

cc_err_t gs_device_get_token(char *token){
    if(strlen(g_token) == 0){
        CC_LOGE(TAG, "token is null");
        return CC_FAIL;
    }

    strcpy(token, g_token);
    return CC_OK;
}

cc_err_t gs_device_get_ble_mac(uint8_t *mac){

    return CC_ERR_NOT_SUPPORTED;
}

cc_err_t gs_device_get_wifi_mac(uint8_t *mac){

    return CC_ERR_NOT_SUPPORTED;
}

cc_err_t gs_device_get_product_key(char *product_key){
    if(strlen(g_dev_triple.product_key) == 0){
        CC_LOGE(TAG, "product_key is null");
        return CC_FAIL;
    }

    strcpy(product_key, g_dev_triple.product_key);
    return CC_OK;
}

cc_err_t gs_device_get_device_name(char *device_name){
    if(strlen(g_dev_triple.device_name) == 0){
        CC_LOGE(TAG, "device_name is null");
        return CC_FAIL;
    }

    strcpy(device_name, g_dev_triple.device_name);
    return CC_OK;
}

cc_err_t gs_device_get_device_secret(char *device_secret){
    if(strlen(g_dev_triple.device_secret) == 0){
        CC_LOGE(TAG, "device_secret is null");
        return CC_FAIL;
    }

    strcpy(device_secret, g_dev_triple.device_secret);
    return CC_OK;
}

cc_err_t gs_device_get_version(char *sw, char *hw){
    if(strlen(g_sw_version) == 0
            || strlen(g_hw_version) == 0){
        CC_LOGE(TAG, "version is null");
        return CC_FAIL;
    }

    strcpy(sw, g_sw_version);
    strcpy(hw, g_hw_version);
    return CC_OK;
}

cc_err_t gs_device_set_version(char *sw, char *hw){
    if(strnlen(sw, GS_DEVICE_VERSION_BUF_MAX_LEN) == GS_DEVICE_VERSION_BUF_MAX_LEN
            || strnlen(hw, GS_DEVICE_VERSION_BUF_MAX_LEN) == GS_DEVICE_VERSION_BUF_MAX_LEN){
        CC_LOGE_CODE(TAG, CC_ERR_INVALID_ARG);
        return CC_ERR_INVALID_ARG;
    }

    CC_LOGD(TAG, "gs_device_set_version sw: %s hw: %s", sw, hw);

    strcpy(g_sw_version, sw);
    strcpy(g_hw_version, hw);
    return CC_OK;
}

cc_err_t gs_device_init(void){

    size_t len = 0;

    memset(&g_dev_triple, 0, sizeof(_dev_triple_t));

    // cc_hal_kvs_get(DEV_KVS_KEY, &g_dev_triple, &len);
    len = GS_PRODUCT_KEY_BUF_MAX_LEN;
    cc_hal_kvs_get("gs_product_key", g_dev_triple.product_key, &len);
    len = GS_DEVICE_NAME_BUF_MAX_LEN;
    cc_hal_kvs_get("gs_device_name", g_dev_triple.device_name, &len);
    len = GS_DEVICE_SECRET_BUF_MAX_LEN;
    cc_hal_kvs_get("gs_device_secret", g_dev_triple.device_secret, &len);

    if(strlen(g_dev_triple.product_key) && strlen(g_dev_triple.device_name) && strlen(g_dev_triple.device_secret)){
        CC_LOGI(TAG, "read dev triple product_key: %s, device_name: %s, device_secret: %s", 
            g_dev_triple.product_key, g_dev_triple.device_name, g_dev_triple.device_secret);

        cc_log_write("gs_product_key: %s\n", g_dev_triple.product_key);
        cc_log_write("gs_device_name: %s\n", g_dev_triple.device_name);
        cc_log_write("gs_device_secret: %s\n", g_dev_triple.device_secret);

    }else{
        CC_LOGI(TAG, "dev triple not set");

        strcpy(g_dev_triple.product_key, "");
        strcpy(g_dev_triple.device_name, "");
        strcpy(g_dev_triple.device_secret, "");
    }

    return CC_OK;
}
