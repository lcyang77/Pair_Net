#include "product.h"

#include "cc_log.h"
#include "cc_hal_sys.h"

#include "gs_bind.h"
#include "gs_wifi.h"
#include "gs_mqtt.h"
#include "gs_device.h"

#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#include "frame_parser.h"

#include "cJSON.h"

#include "flexible_button.h"

#include "driver/gpio.h"

#include "cc_tmr_task.h"

static char *TAG = "product";

#define MAX_LEN 32
#define MAX_DATA_LEN (MAX_LEN - sizeof(frame_parser_head_t) - sizeof(frame_parser_last_t))

#define EX_UART_NUM UART_NUM_1
#define RX_BUF_SIZE 32*5

// #define TXD_PIN (GPIO_NUM_4)
// #define RXD_PIN (GPIO_NUM_5)

#define TXD_PIN (GPIO_NUM_20)
#define RXD_PIN (GPIO_NUM_19)

#define SUB_TOPIC_SERVER_PUB                     "/service/publish"
#define PUB_TOPIC_DEVICE_PUB                     "/event/notify"

void __reboot(uint32_t interval, void *arg){
    cc_tmr_task_delete(__reboot);

    cc_hal_sys_reboot();
}

static uint8_t __crc_high_first(uint8_t *ptr, uint16_t len)
{
    uint8_t i;
    uint8_t crc=0x00;/* 计算的初始crc值 */ 

    while(len--)
    {
        crc ^= *ptr++;/* 每次先与需要计算的数据异或,计算完指向下一数据 */  
        for (i=8;i>0;--i)   /* 下面这段计算过程与计算一个字节crc一样 */  
        { 
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x7;
            else
                crc = (crc << 1);
        }
    }
    return (crc);
}


uint8_t binaryToString(const unsigned char *buffer, uint8_t bufferSize, char *output, uint8_t outputSize) {

    if (bufferSize*2 > outputSize) {
        printf("Output buffer is too small\n");
        return false;
    }

    for (uint8_t i = 0; i < bufferSize; i++) {
        sprintf(output + (i * 2), "%02X", buffer[i]);
    }
    output[bufferSize * 2] = '\0';

    return 1;
}

unsigned char hexCharToByte(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    } else {
        printf("Invalid hex character: %c\n", c);
        return 0;
    }
}


uint8_t stringToBinary(const char *str, unsigned char *output, uint8_t outputSize, uint8_t *output_len) {
    uint8_t strLength = strlen(str);
    uint8_t bufferSize = strLength / 2;

    if (bufferSize > outputSize) {
        printf("Output buffer is too small\n");
        return 0;
    }

    for (uint8_t i = 0; i < bufferSize; i++) {
        unsigned char byte = (hexCharToByte(str[i * 2]) << 4) | hexCharToByte(str[i * 2 + 1]);
        output[i] = byte;
    }

    *output_len = bufferSize;
    return 1;
}

static void __frame_process(uint8_t *data, uint8_t len){

    cJSON *root_obj = NULL;
    char hex_str[MAX_DATA_LEN*2 + 1] = {0};

    CC_LOGI_HEXDUMP(TAG, data, len);
    frame_parser_add_buf(data, len);
    while (1){
        uint32_t get_len = 0;
        uint8_t get_buf[FRAME_MAX_LEN] = {0};
        if(frame_parser_get_frame(get_buf, &get_len)){
            CC_LOGD(TAG, "frame_parser_get_frame get_len: %d", get_len);
            if(get_len > MAX_LEN){
                CC_LOGE(TAG, "to long");
                continue;
            }
            frame_parser_head_t *head = (frame_parser_head_t *)get_buf;
            frame_parser_last_t *last = (frame_parser_last_t *)(get_buf + sizeof(frame_parser_head_t) + head->len);
            uint8_t crc =  __crc_high_first((uint8_t *)head, sizeof(frame_parser_head_t) + head->len);
            if(crc != last->crc){
                CC_LOGE(TAG, "crc error: %02x != %02x", crc, last->crc);
            }
            if(head->cmd != 0x01){
                CC_LOGE(TAG, "cmd error: %02x != %02x", 0x01, head->cmd);
                continue;
            }
            CC_LOGI_HEXDUMP(TAG, head->data, head->len);
            binaryToString(head->data, head->len, hex_str, sizeof(hex_str));
            root_obj = cJSON_CreateObject();
            if(root_obj){
                char sw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
                char hw_version[GS_DEVICE_VERSION_BUF_MAX_LEN] = "";
                gs_device_get_version(sw_version, hw_version);

                cJSON_AddItemToObject(root_obj, "ver", cJSON_CreateString(sw_version));
                cJSON_AddItemToObject(root_obj, "type", cJSON_CreateString("0001"));
                cJSON_AddItemToObject(root_obj, "data", cJSON_CreateString(hex_str));
                cJSON_AddItemToObject(root_obj, "seq_no", cJSON_CreateString(gs_mqtt_generate_seq()));
                char *msg = cJSON_PrintUnformatted(root_obj);
                if(msg){
                    CC_LOGI(TAG, "pub %s: %s'", PUB_TOPIC_DEVICE_PUB, msg);
                    gs_mqtt_publish(PUB_TOPIC_DEVICE_PUB, (uint8_t *)msg, strlen(msg), GS_MQTT_QOS0, 0);
                    cJSON_free(msg);
                }else{
                    CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
                }
                cJSON_Delete(root_obj);
            }else{
                CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            }
        }else{
            break;
        }
    }
}

static void __rx_task(void *arg)
{
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int len = uart_read_bytes(EX_UART_NUM, data, RX_BUF_SIZE, (100 / portTICK_PERIOD_MS));
        if (len > 0) {
            data[len] = 0;
            __frame_process(data, len);
        }
    }
    free(data);
}

void __mqtt_msg_cb(const char *topic, uint8_t qos, uint8_t retain, char *data, uint32_t len){
    CC_LOGI(TAG, "msg %s: %.*s'", topic, len, data);

    static uint8_t buf[FRAME_MAX_LEN] = {0};
    cJSON *root_obj = NULL, *type_obj = NULL, *data_obj = NULL;

    root_obj = cJSON_Parse(data);

    if(root_obj){
        type_obj = cJSON_GetObjectItem(root_obj, "type");
        if(type_obj && type_obj->type == cJSON_String && strcmp(type_obj->valuestring, "0002") == 0){
            data_obj = cJSON_GetObjectItem(root_obj, "data");
            if(data_obj && data_obj->type == cJSON_String && strlen(data_obj->valuestring) <= MAX_DATA_LEN*2){
                frame_parser_head_t *head = (frame_parser_head_t *)buf;
                head->head = 0x23BB;
                head->cmd = 0x02;; 
                stringToBinary(data_obj->valuestring, head->data, RX_BUF_SIZE, &head->len);
                CC_LOGI_HEXDUMP(TAG, head->data, head->len);
                frame_parser_last_t *last = (frame_parser_last_t *)(buf + sizeof(frame_parser_head_t) + head->len);
                last->crc =  __crc_high_first((uint8_t *)head, sizeof(frame_parser_head_t) + head->len);
                uart_write_bytes(EX_UART_NUM, head, sizeof(frame_parser_head_t) + head->len + sizeof(frame_parser_last_t));
            }else{
                CC_LOGE(TAG, "data error");
            }
        }else{
            CC_LOGE(TAG, "type error");
        }
        cJSON_Delete(root_obj);
    }else{
        CC_LOGE(TAG, "json error");
    }
}

cc_err_t __uart_init(void){
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    //Install UART driver, and get the queue.
    uart_driver_install(EX_UART_NUM, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(EX_UART_NUM, &uart_config);

    uart_set_pin(EX_UART_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // uart_set_rx_full_threshold(EX_UART_NUM, 120);
    // uart_set_rx_timeout(EX_UART_NUM, 10);

    xTaskCreate(__rx_task, "__rx_task", 3072, NULL, 6, NULL);

    return CC_OK;
}

static void __event_handler(void* handler_args, cc_event_base_t base_event, int32_t id, void* event_data){

    CC_LOGD(TAG, "__event_handler: %s event: %d", base_event, id);
    if(base_event == GS_MQTT_EVENT){
        switch (id)
        {
        case GS_MQTT_EVENT_CONNECTED:
            if(gs_bind_get_bind_status()){
                gs_mqtt_subscribe(SUB_TOPIC_SERVER_PUB, GS_MQTT_QOS0);
            }
            break;
        default:
            break;
        }
    }
}

uint8_t __btn_read1(void *arg){
    flex_button_t *button = (flex_button_t *)arg;
    return gpio_get_level(button->id);
}

static void __btn_event_cb1(void *arg)
{
    flex_button_t *btn = (flex_button_t *)arg;

    CC_LOGI(TAG, "id: [%d]  event: [%d]  repeat: %d", btn->id, btn->event, btn->click_cnt);

    switch (btn->event)
    {
    case FLEX_BTN_PRESS_LONG_START:
        gs_bind_reset_bind_status();
        gs_bind_set_boot_auto_start_cfg_mode(GS_BIND_CFG_MODE_AP | GS_BIND_CFG_MODE_BLE);
        cc_tmr_task_create(__reboot, 100, NULL);
        break;
    default:
        break;
    }
}


void __button_init(void){
    static flex_button_t button = {0};

    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_INPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = (1ULL<<9);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 1;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    button.id = 9;
    button.usr_button_read = __btn_read1;
    button.cb = __btn_event_cb1;

    button.pressed_logic_level = 0;

    button.short_press_start_tick = FLEX_MS_TO_SCAN_CNT(500);
    button.long_press_start_tick = FLEX_MS_TO_SCAN_CNT(2000);
    button.long_hold_start_tick = FLEX_MS_TO_SCAN_CNT(2000+100);

    flex_button_register(&button);
}

static void __button_task(uint32_t interval, void *arg){

    flex_button_scan((uint8_t)interval);
}

cc_err_t product_init(void){

    uint8_t mode = gs_bind_get_boot_auto_start_cfg_mode();
    if(mode != GS_BIND_CFG_MODE_NULL){
        gs_bind_start_cfg_mode(mode);
        gs_bind_set_boot_auto_start_cfg_mode(GS_BIND_CFG_MODE_NULL);
    }else if(gs_bind_get_bind_status()){
        gs_wifi_sta_start_connect();
    }

    __uart_init();

    gs_mqtt_register_msg_cb(__mqtt_msg_cb);
    cc_event_register_handler(GS_MQTT_EVENT, __event_handler);


    frame_parser_init(32*5);

    __button_init();
    cc_tmr_task_create(__button_task, 20, NULL);

    return CC_OK;
}