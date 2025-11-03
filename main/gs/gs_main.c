#include "gs_main.h"

#include <string.h>

#include "cc_log.h"

#include "gs_device.h"
#include "gs_wifi.h"
#include "gs_bind.h"
#include "gs_mqtt.h"
#include "gs_ota.h"

static char *TAG = "gs_main";


void gs_init(char *sw_ver, char *hw_ver){
    gs_device_init();
    gs_device_set_version(sw_ver, hw_ver);
    
    gs_wifi_init();

    gs_bind_init();

    gs_mqtt_init();

    gs_ota_init();
}
