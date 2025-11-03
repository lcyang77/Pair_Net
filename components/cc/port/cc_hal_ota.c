
#include "cc_hal_ota.h"
#include "cc_log.h"

#include "cc_hal_sys.h"

#include "esp_ota_ops.h"

static const char *TAG = "app_ota";

esp_ota_handle_t update_handle = 0;
const esp_partition_t *update_partition = NULL;

cc_err_t cc_hal_ota_update(uint32_t idx, uint8_t *buf, uint16_t len){

    esp_err_t err = ESP_OK;
    err = esp_ota_write(update_handle, (const void *)buf, len);
    if (err != ESP_OK) {
        CC_LOGE(TAG, "esp_ota_write error: %d", err);
        return CC_FAIL;
    }
    
    return CC_OK;
}

cc_err_t cc_hal_ota_end(void){
    esp_err_t ota_end_err = esp_ota_end(update_handle);
	if (ota_end_err != ESP_OK) {
		CC_LOGE(TAG, "esp_ota_end failed! err=0x%d. Image is invalid", ota_end_err);
		return CC_FAIL;
	}

	esp_err_t err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK) {
		CC_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%d", err);
		return CC_FAIL;
	}
	CC_LOGI(TAG, "esp_ota_set_boot_partition succeeded");
    return CC_OK;
}


cc_err_t cc_hal_ota_begin(void){

    update_partition = esp_ota_get_next_update_partition(NULL);
	if (update_partition == NULL) {
		CC_LOGE(TAG, "Passive OTA partition not found");
		return CC_FAIL;
	}

	CC_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
	         update_partition->subtype, update_partition->address);

	esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK) {
		CC_LOGE(TAG, "esp_ota_begin failed, error=%d", err);
		return CC_FAIL;
	}

	CC_LOGI(TAG, "esp_ota_begin succeeded");
	CC_LOGI(TAG, "Please Wait. This may take time");

    return CC_OK;
}

