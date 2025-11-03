
#include "cc_hal_network.h"

#include <stdio.h>
#include <stdlib.h>

#include "cc_log.h"
#include "cc_hal_os.h"
#include "cc_hal_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "http_client.h"

#include "lwip/init.h"
#include "lwip/udp.h"
#include <lwip/def.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

#include "mqtt_client.h"

#include "cc_list.h"

#include "rbuffer.h"


#define CONFIG_HTTP_STACK_SIZE          3072
#define CONFIG_HTTP_STACK_PRIORITY      4

#define CONFIG_TCPC_STACK_SIZE          3072+512
#define CONFIG_TCPC_STACK_PRIORITY      4

#define CONFIG_TCPS_STACK_SIZE          3072
#define CONFIG_TCPS_STACK_PRIORITY      4
#define CONFIG_TCPS_MAX_SOCKETS         64

#define CONFIG_UDP_STACK_SIZE           4096
#define CONFIG_UDP_STACK_PRIORITY       4

#define CONFIG_MQTT_STACK_SIZE           4096
#define CONFIG_MQTT_STACK_PRIORITY       4
#define CONFIG_MQTT_MSG_LEN_MAX          (512)

static char *TAG = "hal_network";

typedef struct {
    cc_http_t *http;
    TaskHandle_t task;
}_http_ctx_t;

static cc_list_node* g_http_list = NULL;

static int __find_by_http(cc_list_node* node, void* arg){
    _http_ctx_t *http_ctx = node->data;
    cc_http_t *http = (cc_http_t *)arg;
    if(http_ctx && http_ctx->http == http){
        return 1;
    }

    return 0;
}


static void http_event_cb(http_client_t *client, HTTP_EVENT event, void *data){
    cc_http_t *http = (cc_http_t *)client->user_arg;

    if(http->event_cb != NULL){
        switch (event)
        {
        case HTTP_EVENT_ERR:
            http->event_cb(http->arg, CC_HTTP_EVENT_ERROR, data);
            break;
        case HTTP_EVENT_DNS_GET_IP:
            http->event_cb(http->arg, CC_HTTP_EVENT_DNS_GET_IP, data);
            break;
        case HTTP_EVENT_CONNECTED:
            http->event_cb(http->arg, CC_HTTP_EVENT_CONNECTED, NULL);
            break;
        case HTTP_EVENT_HEADER_SENTED:
            http->event_cb(http->arg, CC_HTTP_EVENT_HEADER_SENTED, NULL);
            break;
        case CC_HTTP_EVENT_RECV_HEADER:
            http->event_cb(http->arg, CC_HTTP_EVENT_RECV_HEADER, NULL);
            break;
        case HTTP_EVENT_RECV_DATA:
            http->event_cb(http->arg, CC_HTTP_EVENT_RECV_DATA, NULL);
            break;
        case HTTP_EVENT_FINISH:
            http->event_cb(http->arg, CC_HTTP_EVENT_FINISH, data);
            break;
        default:
            break;
        }
    }
}


static HTTPC_RESULT http_recv_cb(http_client_t *client, http_client_data_t *client_data){

    cc_http_t *http = (cc_http_t *)client->user_arg;

    cc_err_t ret = CC_FAIL;

    if(http->recv_cb){
        ret = http->recv_cb(http->arg, (uint8_t *)client_data->response_buf, client_data->response_len);
        if(ret != CC_OK){
            return HTTP_ERECV;
        }
    }

    return HTTP_SUCCESS;
}

static void http_connect_cb(http_client_t *client){

    cc_http_t *http = (cc_http_t *)client->user_arg;
    
    if(http->connect_cb){
        http->connect_cb(http->arg);
    }
}

static void http_close_cb(http_client_t *client){

}


static void http_task(void *arg){
     
    _http_ctx_t *http_ctx = (_http_ctx_t *)arg;
    cc_http_t *http = http_ctx->http;

    http_client_t http_client = {0};
    http_client_data_t client_data = {0};

    http_client_cb_t client_cb = {0};

    http_client.user_arg = http;
    
    client_data.post_buf = (char *)http->post_buf;
    client_data.post_buf_len = http->post_buf_len;
    client_data.response_buf = http->resp_buf;
    client_data.response_buf_len = http->resp_buf_len;
    http_client_set_custom_header(&http_client, http->header);

    client_cb.event_cb = http_event_cb;
    client_cb.connect_cb = http_connect_cb;
    client_cb.recv_cb = http_recv_cb;
    client_cb.close_cb = http_close_cb;

    if(http->method == CC_HTTP_METHOD_GET){
        http_client_get_request(&http_client, http->url, &client_data, &client_cb);
    }else if(http->method == CC_HTTP_METHOD_POST){
        http_client_post_request(&http_client, http->url, &client_data, &client_cb);
    }

    http->resp_len = client_data.response_content_len - client_data.retrieve_len;

    if(http->close_cb){
        http->close_cb(http->arg, http_client.response_code);
    }

    http_ctx->task = NULL;

    cc_list_remove_by_data(&g_http_list, http_ctx);
    cc_hal_sys_free(http_ctx);

    cc_hal_os_task_delete(NULL);
}


cc_err_t cc_hal_http_connect(cc_http_t *http){

    if(http == NULL){
        return CC_FAIL;
    }

    if(NULL != g_http_list){
        if(NULL != cc_list_find(g_http_list, __find_by_http, (void *)http)){
            return CC_FAIL;
        }
    }

    _http_ctx_t *http_ctx = cc_hal_sys_malloc(sizeof(_http_ctx_t));
    if(NULL == http_ctx){
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        return CC_ERR_NO_MEM;
    }

    http_ctx->http = http;

    uint8_t cnt = 0;
    cc_list_node *node = g_http_list;
    while(node){
        cnt++;
        if(NULL != node){
            node = node->next;
        }else{
            break;
        }
    }

   char task_name[] = "hal_http0";
    task_name[sizeof("hal_http0") - 2] = cnt + '0'; 
    cc_hal_os_task_create(http_task, task_name, CONFIG_HTTP_STACK_SIZE, http_ctx, CONFIG_HTTP_STACK_PRIORITY, &http_ctx->task);
    return CC_OK;
}

#define TCPS_SELECT_TIMEOUT_MS 10

typedef struct {
    int sockfd;
    cc_os_semphr_handle_t semphr;
}_tcps_client;

typedef struct {
    cc_tcps_t *tcps;
    int sockfd;
    _tcps_client *client_lsit;
    uint16_t port;
    uint8_t max_client;
    cc_os_task_handle_t task;
    uint8_t wait_delete;
    void *arg;
}_tcps_ctx_t;

static cc_list_node* g_tcps_list = NULL;

static int __find_by_tcps(cc_list_node* node, void* arg){
    _tcps_ctx_t *tcps_ctx = node->data;
    cc_tcps_t *tcps = (cc_tcps_t *)arg;
    if(tcps_ctx && tcps_ctx->tcps == tcps){
        return 1;
    }

    return 0;
}

void __tcps_task(void *arg){
    _tcps_ctx_t *tcps_ctx = (_tcps_ctx_t *)arg;

    int ret = 0;
    uint8_t buf[1024] = {0};

    while (1){
        if ((tcps_ctx->sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            CC_LOGE(TAG, "tcps create socket error");
            cc_hal_os_task_delay(5000 / CC_OS_TICK_PERIOD_MS);
            continue;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(tcps_ctx->port);
        server_addr.sin_addr.s_addr = INADDR_ANY;


        if (bind(tcps_ctx->sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
            CC_LOGE(TAG, "tsps bind error");
            close(tcps_ctx->sockfd);
            cc_hal_os_task_delay(5000 / CC_OS_TICK_PERIOD_MS);
            continue;
        }

        if (listen(tcps_ctx->sockfd, tcps_ctx->max_client) != 0) {
            CC_LOGE(TAG, "tsps listen error");
            close(tcps_ctx->sockfd);
            cc_hal_os_task_delay(5000 / CC_OS_TICK_PERIOD_MS);
            continue;
        }

        struct timeval timeout;
        timeout.tv_sec = TCPS_SELECT_TIMEOUT_MS / 1000;
        timeout.tv_usec = (TCPS_SELECT_TIMEOUT_MS % 1000) * 1000;

        int max_sockfd = tcps_ctx->sockfd;

        while (1) {
            fd_set set;
            FD_ZERO(&set);
            FD_SET(tcps_ctx->sockfd, &set);

            for (int i = 0; i < tcps_ctx->max_client; i ++){
                if (tcps_ctx->client_lsit[i].sockfd != -1){
                    FD_SET(tcps_ctx->client_lsit[i].sockfd, &set);
                }
            }

            ret = select(max_sockfd + 1, &set, NULL, NULL, &timeout);
            if (ret > 0){
                if (FD_ISSET(tcps_ctx->sockfd, &set)){
                    // 有数据可接收
                    struct sockaddr_in client_addr;
                    socklen_t client_addr_size = sizeof(client_addr);
                    int fd = accept(tcps_ctx->sockfd, (struct sockaddr *) &client_addr, &client_addr_size);
                    if (fd > 0) {
                        int i = 0;
                        for (i = 0; i < tcps_ctx->max_client; i++){
                            if(tcps_ctx->client_lsit[i].sockfd == -1){
                                tcps_ctx->client_lsit[i].sockfd = fd;
                                CC_LOGD(TAG, "tcps connected client_id: %d", i);

                                const struct timeval recv_timeout = { 1, 0 }; /* 1 second timeout */
                                setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

                                const struct timeval send_timeout = { 3, 0 }; /* 3 second timeout */
                                setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));

                                if (tcps_ctx->tcps && tcps_ctx->tcps->connect_cb){
                                    tcps_ctx->tcps->connect_cb(tcps_ctx->tcps->arg, i);
                                }

                                if (fd > max_sockfd){
                                    max_sockfd = fd;
                                }
                                break;
                            }
                        }
                        if(i == tcps_ctx->max_client){
                            close(fd);
                        }
                    }else {
                        CC_LOGE(TAG, "tcps accept error");
                    }
                }else{
                    for (int i = 0; i < tcps_ctx->max_client; i ++){
                        _tcps_client *tcps_client = &tcps_ctx->client_lsit[i];
                        if(tcps_client->sockfd != -1 && FD_ISSET(tcps_client->sockfd, &set)){
                            int len = recv(tcps_client->sockfd, buf, sizeof(buf), 0);
                            if (len > 0){
                                // 调用接收回调函数处理接收到的数据
                                if (tcps_ctx->tcps && tcps_ctx->tcps->recv_cb){
                                    tcps_ctx->tcps->recv_cb(tcps_ctx->tcps->arg, i, buf, len);
                                }
                            }else{
                                uint8_t need_close = 0;
                                if (len == 0){
                                    // CC_LOGE(TAG, "tcps client: %d recv len = 0", i);
                                    need_close = 1;
                                }else{
                                    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                                        
                                    } else {
                                        CC_LOGE(TAG, "tcps client: %d recv len < 0 & errno:%d", i, errno);
                                        need_close = 1;
                                    }
                                }
                                if(need_close){
                                    close(tcps_client->sockfd);
                                    tcps_client->sockfd = -1;
                                    if (tcps_ctx->tcps && tcps_ctx->tcps->disconnect_cb){
                                        tcps_ctx->tcps->disconnect_cb(tcps_ctx->tcps->arg, i);
                                    }
                                    for (int i = 0; i < tcps_ctx->max_client; i ++){
                                        if (tcps_ctx->client_lsit[i].sockfd != -1){
                                            if (tcps_ctx->client_lsit[i].sockfd > max_sockfd){
                                                max_sockfd = tcps_ctx->client_lsit[i].sockfd;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                }
            }
            else if (ret == 0){
                
            }else{
                break;
            }

            if(tcps_ctx->wait_delete){
                break;
            }
            cc_hal_os_task_delay(10 / CC_OS_TICK_PERIOD_MS);
        }
        for (int i = 0; i < tcps_ctx->max_client; i ++){
            _tcps_client *tcps_client = &tcps_ctx->client_lsit[i];
            close(tcps_client->sockfd);
            // if(tcps_client->semphr){
            //     cc_hal_os_semphr_delete(tcps_client->semphr);
            // }
        }
        close(tcps_ctx->sockfd);
        
        if(tcps_ctx->wait_delete){
            break;
        }
    }

    cc_hal_sys_free(tcps_ctx->client_lsit); 
    cc_hal_sys_free(tcps_ctx);
    
    vTaskDelete(NULL);
}


cc_err_t cc_hal_tcps_send(cc_tcps_t *tcps, uint8_t client_id, char *data, uint16_t len){

    cc_err_t err = CC_OK;
    cc_list_node *node = NULL;

    if(tcps == NULL || g_tcps_list == NULL){
        return CC_FAIL;
    }

    node = cc_list_find(g_tcps_list, __find_by_tcps, (void *)tcps);
    if(NULL == node){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        return CC_ERR_NOT_FOUND;
    }
    
    _tcps_ctx_t *tcps_ctx = node->data;

    if(tcps_ctx){
        _tcps_client *tcps_client = &tcps_ctx->client_lsit[client_id];        
        if(tcps_client->sockfd != -1){

            //TODO 当发送缓冲区已满时，send 函数将被阻塞
            int send_len = send(tcps_client->sockfd, data, len, 0);
            if(send_len != len){
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR ) {
                
                } else {
                    CC_LOGE(TAG, "tcps client: %d send error & errno:%d", errno, client_id);
                    err = CC_FAIL;
                }
            }
        }else{
            err = CC_FAIL;
        }
    }
    
    return err;
}


cc_err_t cc_hal_tcps_create(cc_tcps_t *tcps){
    if(tcps == NULL){
        return CC_FAIL;
    }

    if(NULL != g_tcps_list){
        if(NULL != cc_list_find(g_tcps_list, __find_by_tcps, (void *)tcps)){
            return CC_FAIL;
        }
    }

    _tcps_ctx_t *tcps_ctx = cc_hal_sys_malloc(sizeof(_tcps_ctx_t));
    if(NULL == tcps_ctx){
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        return CC_ERR_NO_MEM;
    }

    uint8_t max_client = 0;

    max_client = (tcps->max_client > 5)?5:tcps->max_client;

    _tcps_client *client_list = cc_hal_sys_malloc(sizeof(_tcps_client) * max_client);
    if(NULL == client_list){
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        cc_hal_sys_free(tcps_ctx);
        return CC_ERR_NO_MEM;
    }

    for (size_t i = 0; i < max_client; i++){
        client_list[i].sockfd = -1;
        // cc_os_semphr_handle_t semphr = cc_hal_os_semphr_create_binary();
        // if(NULL == semphr){
        //     for (size_t i = 0; i < max_client; i++){
        //         if(client_list[i].semphr){
        //             cc_hal_os_semphr_delete(semphr);
        //         }
        //     }
        //     CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        //     cc_hal_sys_free(client_list);
        //     cc_hal_sys_free(tcps_ctx);
        //     return CC_ERR_NO_MEM;
        // }
        // cc_hal_os_semphr_give(semphr);

        // client_list[i].semphr = semphr;
    }
    
    tcps_ctx->tcps = tcps;
    tcps_ctx->sockfd = -1;
    tcps_ctx->max_client = max_client;
    tcps_ctx->port = tcps->port;
    tcps_ctx->client_lsit = client_list;
    tcps_ctx->wait_delete = 0;

    if(NULL == g_tcps_list){
        g_tcps_list = cc_list_create(tcps_ctx);
    }else{
        if(NULL == cc_list_insert_end(g_tcps_list, tcps_ctx)){
            // for (size_t i = 0; i < max_client; i++){
            //     if(client_list[i].semphr){
            //         cc_hal_os_semphr_delete(client_list[i].semphr);
            //     }
            // }
            cc_hal_sys_free(client_list);
            cc_hal_sys_free(tcps_ctx);
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            return CC_ERR_NO_MEM;
        }
    }

    uint8_t cnt = 0;
    cc_list_node *node = g_tcps_list;
    while(node){
        cnt++;
        if(NULL != node){
            node = node->next;
        }else{
            break;
        }
    }
    
    char task_name[] = "hal_tcps0";
    task_name[sizeof("hal_tcps0") - 2] = cnt + '0'; 
    cc_err_t err = CC_FAIL;
    if(tcps_ctx->port == 8266){
        err = cc_hal_os_task_create(__tcps_task, task_name, 4096+1024, tcps_ctx, CONFIG_TCPS_STACK_PRIORITY, &tcps_ctx->task);
    }else{
        err = cc_hal_os_task_create(__tcps_task, task_name, CONFIG_TCPS_STACK_SIZE, tcps_ctx, CONFIG_TCPS_STACK_PRIORITY, &tcps_ctx->task);
    }
    if(err != CC_OK){
        CC_LOGE_CODE(TAG, err);
        cc_list_remove_by_data(&g_tcps_list, tcps_ctx);
        // for (size_t i = 0; i < max_client; i++){
        //     if(client_list[i].semphr){
        //         cc_hal_os_semphr_delete(client_list[i].semphr);
        //     }
        // }
        cc_hal_sys_free(client_list);
        cc_hal_sys_free(tcps_ctx);
        return err;
    }
    
    return CC_OK;
}

cc_err_t cc_hal_tcps_delete(cc_tcps_t *tcps){
    cc_list_node *node = NULL;

    if(tcps == NULL || g_tcps_list == NULL){
        return CC_FAIL;
    }

    node = cc_list_find(g_tcps_list, __find_by_tcps, (void *)tcps);
    if(NULL == node){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        return CC_ERR_NOT_FOUND;
    }

    _tcps_ctx_t *tcps_ctx = node->data;
    cc_list_remove(&g_tcps_list, node);

    if(tcps_ctx){
        tcps_ctx->wait_delete = 1;
        tcps_ctx->tcps = NULL;
    }else{
        return CC_FAIL;
    }

    return CC_OK;
}


typedef struct {
    cc_mqtt_t *mqtt;
    void *handle;
    char *host;
    uint16_t port;
    char *client_id;
    char *username;
    char *password;
    uint8_t is_connect;
    cc_os_task_handle_t task;
    cc_os_semphr_handle_t semphr;
    uint8_t wait_delete;
    uint8_t wait_reconnect;
    void *arg;
}_mqtt_ctx_t;

static cc_list_node* g_mqtt_list = NULL;


static int __find_by_mqtt(cc_list_node* node, void* arg){
    _mqtt_ctx_t *mqtt_ctx = node->data;
    cc_mqtt_t *mqtt = (cc_mqtt_t *)arg;
    if(mqtt_ctx && mqtt_ctx->mqtt == mqtt){
        return 1;
    }
    
    return 0;
}

static void __mqtt_event_handler(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    CC_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;

    _mqtt_ctx_t *mqtt_ctx = (_mqtt_ctx_t *)arg;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        if(mqtt_ctx->mqtt->connect_cb){
            mqtt_ctx->mqtt->connect_cb(mqtt_ctx->mqtt->arg);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        if(mqtt_ctx->mqtt->disconnect_cb){
            mqtt_ctx->mqtt->disconnect_cb(mqtt_ctx->mqtt->arg);
        }
        break;
    case MQTT_EVENT_DATA:
        if(mqtt_ctx->mqtt->msg_cb){
            char topic[256] = "";
            snprintf(topic, sizeof(topic), "%.*s", event->topic_len, event->topic);
            mqtt_ctx->mqtt->msg_cb(mqtt_ctx->mqtt->arg, topic, event->data, event->data_len, event->qos, event->retain);
        }
        break;
    default:
        CC_LOGD(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

cc_err_t cc_hal_mqtt_create(cc_mqtt_t *mqtt){

    esp_mqtt_client_config_t mqtt_cfg;

    if(mqtt == NULL){
        return CC_FAIL;
    }

    if(NULL != g_mqtt_list){
        if(NULL != cc_list_find(g_mqtt_list, __find_by_mqtt, (void *)mqtt)){
            return CC_FAIL;
        }
    }

    _mqtt_ctx_t *mqtt_ctx = cc_hal_sys_malloc(sizeof(_mqtt_ctx_t));
    if(NULL == mqtt_ctx){
        CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
        return CC_ERR_NO_MEM;
    }

    mqtt_ctx->mqtt = mqtt;

    if(mqtt->host){
        mqtt_ctx->host = cc_hal_sys_malloc(strlen(mqtt->host) + 1);
        if(NULL == mqtt_ctx->host){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            goto mem_err;
        }
        strcpy(mqtt_ctx->host, mqtt->host);
    }else{
        cc_hal_sys_free(mqtt_ctx);
        return CC_ERR_INVALID_ARG;
    }

    mqtt_ctx->port = mqtt->port;

    if(mqtt->client_id){
        mqtt_ctx->client_id = cc_hal_sys_malloc(strlen(mqtt->client_id) + 1);
        if(NULL == mqtt_ctx->client_id){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            goto mem_err;
        }
        strcpy(mqtt_ctx->client_id, mqtt->client_id);
    }

    if(mqtt->username){
        mqtt_ctx->username = cc_hal_sys_malloc(strlen(mqtt->username) + 1);
        if(NULL == mqtt_ctx->username){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            goto mem_err;
        }
        strcpy(mqtt_ctx->username, mqtt->username);
    }

    if(mqtt->password){
        mqtt_ctx->password = cc_hal_sys_malloc(strlen(mqtt->password) + 1);
        if(NULL == mqtt_ctx->password){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            goto mem_err;
        }
        strcpy(mqtt_ctx->password, mqtt->password);
    }

    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
    mqtt_cfg.broker.address.hostname = mqtt_ctx->host;
    mqtt_cfg.broker.address.port = mqtt_ctx->port;
    mqtt_cfg.credentials.client_id = mqtt_ctx->client_id;
    mqtt_cfg.credentials.username = mqtt_ctx->username;
    mqtt_cfg.credentials.authentication.password = mqtt_ctx->password;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if(NULL == client){
        goto mem_err;
    }

    mqtt_ctx->handle = (void *)client;

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, __mqtt_event_handler, mqtt_ctx);
    esp_mqtt_client_start(client);

    if(NULL == g_mqtt_list){
        g_mqtt_list = cc_list_create(mqtt_ctx);
    }else{
        if(NULL == cc_list_insert_end(g_mqtt_list, mqtt_ctx)){
            CC_LOGE_CODE(TAG, CC_ERR_NO_MEM);
            goto mem_err;
        }
    }

    return CC_OK;

mem_err:
    if(mqtt_ctx->host){
        cc_hal_sys_free(mqtt_ctx->host);
    }
    if(mqtt_ctx->client_id){
        cc_hal_sys_free(mqtt_ctx->client_id);
    }
    if(mqtt_ctx->username){
        cc_hal_sys_free(mqtt_ctx->username);
    }
    if(mqtt_ctx->password){
        cc_hal_sys_free(mqtt_ctx->password);
    }
    if(mqtt_ctx){
        cc_hal_sys_free(mqtt_ctx);
    }
    return CC_ERR_NO_MEM;
}

cc_err_t cc_hal_mqtt_delete(cc_mqtt_t *mqtt){
    cc_list_node *node = NULL;

    if(mqtt == NULL || g_mqtt_list == NULL){
        return CC_FAIL;
    }

    node = cc_list_find(g_mqtt_list, __find_by_mqtt, (void *)mqtt);
    if(NULL == node){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        return CC_ERR_NOT_FOUND;
    }

    _mqtt_ctx_t *mqtt_ctx = node->data;
    cc_list_remove(&g_mqtt_list, node);

    if(mqtt_ctx){
        mqtt_ctx->wait_delete = 1;
        mqtt_ctx->mqtt = NULL;
    }else{
        return CC_FAIL;
    }

    return CC_OK;
}
cc_err_t cc_hal_mqtt_subscribe(cc_mqtt_t *mqtt, char *topic, cc_mqtt_qos_t qos){
    cc_list_node *node = NULL;

    if(mqtt == NULL || g_mqtt_list == NULL){
        return CC_FAIL;
    }

    node = cc_list_find(g_mqtt_list, __find_by_mqtt, (void *)mqtt);
    if(NULL == node){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        return CC_ERR_NOT_FOUND;
    }

    _mqtt_ctx_t *mqtt_ctx = node->data;
    esp_mqtt_client_handle_t client = mqtt_ctx->handle;

    esp_mqtt_client_subscribe(client, topic, qos);

    return CC_OK;
}

cc_err_t cc_hal_mqtt_publish(cc_mqtt_t *mqtt, char *topic, char *msg, uint16_t len, cc_mqtt_qos_t qos, uint8_t retain){
    cc_list_node *node = NULL;

    if(mqtt == NULL || g_mqtt_list == NULL){
        return CC_FAIL;
    }

    node = cc_list_find(g_mqtt_list, __find_by_mqtt, (void *)mqtt);
    if(NULL == node){
        CC_LOGE_CODE(TAG, CC_ERR_NOT_FOUND);
        return CC_ERR_NOT_FOUND;
    }

    _mqtt_ctx_t *mqtt_ctx = node->data;
    esp_mqtt_client_handle_t client = mqtt_ctx->handle;

    esp_mqtt_client_publish(client, topic, msg, len, qos, retain);

    return CC_OK;
}
