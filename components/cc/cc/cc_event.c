/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#include "cc_event.h"

#include "cc_list.h"
#include "cc_log.h"

#include "cc_hal_sys.h"
#include "cc_hal_os.h"

static char *TAG = "cc_event";

cc_err_t cc_event_register_handler(cc_event_base_t base_event, cc_event_handler_t handler){
    
    esp_err_t err = esp_event_handler_register(base_event, ESP_EVENT_ANY_ID, handler, NULL);
    return (err == ESP_OK)?CC_OK:CC_FAIL;
}

cc_err_t cc_event_unregister_handler(cc_event_base_t base_event, cc_event_handler_t handler){
    
    esp_err_t err = esp_event_handler_unregister(base_event, ESP_EVENT_ANY_ID, handler);
    return (err == ESP_OK)?CC_OK:CC_FAIL;
}


cc_err_t cc_event_post(cc_event_base_t base_event, int32_t event_id, void* event_data, size_t event_data_len){

    esp_err_t err = esp_event_post(base_event, event_id, event_data, event_data_len, portMAX_DELAY);
    return (err == ESP_OK)?CC_OK:CC_FAIL;
}

cc_err_t cc_event_real_post(cc_event_base_t base_event, int32_t event_id, void* event_data, size_t event_data_len){
    
    esp_err_t err = esp_event_post(base_event, event_id, event_data, event_data_len, portMAX_DELAY);
    return (err == ESP_OK)?CC_OK:CC_FAIL;
}

void cc_event_run(void){


}

cc_err_t cc_event_init(void){
    
    esp_err_t err = esp_event_loop_create_default();
    return (err == ESP_OK)?CC_OK:CC_FAIL;
}