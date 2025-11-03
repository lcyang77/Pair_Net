/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "cc_log.h"

#include "cc_hal_sys.h"
#include "cc_hal_kvs.h"
#include "cc_hal_wifi.h"

#include "cc_event.h"
#include "cc_timer.h"
#include "cc_tmr_task.h"

#include "cc_http.h"

#include "gs_main.h"

#include "product.h"

#define LOOP_INTERVAL   10

static char *TAG = "main";

void app_main(void)
{
    static uint64_t last = 0;   

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    CC_LOGI(TAG, "cc_init");

    cc_hal_sys_init();
    cc_hal_kvs_init();

    cc_hal_wifi_init();

    cc_event_init();
    cc_timer_init();
    cc_tmr_task_init();

    last = cc_hal_sys_get_ms();

    gs_init("8.0.0.0", "1.0.0");

    product_init();

    while (1){
        uint64_t now = cc_hal_sys_get_ms();
        uint16_t ms = now - last;
        // if(ms > LOOP_INTERVAL){
            last = cc_hal_sys_get_ms();

            cc_event_run();
            cc_timer_run(CC_TIMMER_MS(ms));
            cc_http_run(ms);
            cc_tmr_task_run(ms);
        // }
        vTaskDelay(pdMS_TO_TICKS(LOOP_INTERVAL));
    }
}
