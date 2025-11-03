
#include "captive_portal.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <lwip/def.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

// #define _PRINTF  os_printf
#define _PRINTF(...)
#define LOG(TAG, format, ...) _PRINTF("\033[0;32m");_PRINTF(format, ##__VA_ARGS__);_PRINTF("\n\033[0m")
#define ESP_LOGI LOG
#define ESP_LOGE LOG

enum{
    HUAWEI = 0,
    HUAWEI_1,
    COMMON
};

typedef struct
{
    char *str;
    char *resp;
}resp_ctx_t;

resp_ctx_t g_resp_ctx[] = {
    [HUAWEI] = {.str = "huawei", .resp = "HTTP/1.1 204 No Content\r\nDate: Fri, 20 Oct 2023 10:40:31 GMT\r\nConnection: keep-alive\r\nX-Hwcloud-ReqId: 1c1b92582ed841cbb98db15c7b9f592c\r\nServer: elb\r\n\r\n"},
    [HUAWEI_1] = {.str = "hicloud", .resp = "HTTP/1.1 204 No Content\r\nDate: Fri, 20 Oct 2023 10:40:31 GMT\r\nConnection: keep-alive\r\nX-Hwcloud-ReqId: 1c1b92582ed841cbb98db15c7b9f592c\r\nServer: elb\r\n\r\n"},
    [COMMON] = {.str = NULL, .resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n"}
};

static uint8_t g_curr_type = COMMON;

static const char *HTTP_400 = "HTTP/1.0 400 BadRequest\r\n"
                              "Content-Length: 0\r\n"
                              "Connection: Close\r\n"
                              "Server: lwIP/1.4.0\r\n\n";

static const char *TAG = "CAPTIVE_PORTAL";

static uint8_t g_start = 0;

static int create_udp_socket(int port)
{
    struct sockaddr_in saddr = {0};
    int sock = -1;
    int err = 0;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    // Bind the socket to any address
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }

    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}

static int create_tcp_socket(int port)
{
    struct sockaddr_in saddr = {0};
    int sock = -1;
    int err = 0;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Failed to create socket. Error %d", errno);
        return -1;
    }

    memset(&saddr, 0, sizeof(saddr));

    // Bind the socket to any address
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);

    err = bind(sock, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Failed to bind socket. Error %d", errno);
        goto err;
    }

    // All set, socket is configured for sending and receiving
    return sock;

err:
    close(sock);
    return -1;
}

void captive_portal_tast(void *arg){

    int cli_sock[8] = {-1, -1, -1, -1, -1, -1, -1, -1};

    int dns_sock = create_udp_socket(53);
    int web_sock = create_tcp_socket(80);

    if(dns_sock == -1 || web_sock == -1){
        goto err;
    }

    if (listen(web_sock, sizeof(cli_sock)) != 0) {
        ESP_LOGE(TAG, "tsps listen error");
        close(web_sock);
        close(dns_sock);
        goto err;
    }

    struct timeval timeout;
    timeout.tv_sec = 500 / 1000;
    timeout.tv_usec = (500 % 1000) * 1000;

    while (1) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(dns_sock, &set);
        FD_SET(web_sock, &set);

        int max_sockfd = (web_sock > dns_sock) ? web_sock : dns_sock;
        for (int i = 0; i < sizeof(cli_sock)/sizeof(cli_sock[0]); i ++){
            if (cli_sock[i] != -1){
                FD_SET(cli_sock[i], &set);
                if(cli_sock[i] > max_sockfd){
                    max_sockfd = cli_sock[i];
                }
            }
        }

        struct sockaddr_in client;
        socklen_t client_len = sizeof(client);

        int ret = select(max_sockfd + 1, &set, NULL, NULL, &timeout);
        if(ret > 0){
            if (FD_ISSET(dns_sock, &set)){
                uint8_t data[256];
                int len = recvfrom(dns_sock, data, 100, 0, (struct sockaddr *)&client, &client_len); // 阻塞式
                if(len > 0 && len < 100){
                    _PRINTF("DNS request:");
                    for (int i = 0x4; i < len; i++)
                    {
                        if ((data[i] >= 'a' && data[i] <= 'z') || (data[i] >= 'A' && data[i] <= 'Z') || (data[i] >= '0' && data[i] <= '9'))
                            _PRINTF("%c", data[i]);
                        else
                            _PRINTF("_");
                    }
                    _PRINTF(" ");
   
                    if (strstr((const char *)data + 0xc, "conn") == NULL
                            && strstr((const char *)data + 0xc, "wifi") == NULL){
                        if(strstr((const char *)data + 0xc, "qq") && data[0xd+0] == 'v' && data[0xd+2] == 'q' && data[0xd+3] == 'q'){     //MIUI

                        }else{
                            _PRINTF("no match\r\n");
                            continue;
                        }
                    }

                    _PRINTF("\r\n");

                    for (size_t i = 0; i < sizeof(g_resp_ctx)/sizeof(g_resp_ctx[0]); i++)
                    {
                        if (g_resp_ctx[i].str != NULL){
                            if (strstr((const char *)data + 0xc, g_resp_ctx[i].str)){
                                g_curr_type = i;
                                break;
                            }
                        }
                        g_curr_type = i;
                    }
                    

                    data[2] |= 0x80;
                    data[3] |= 0x80;
                    data[7] = 1;

                    data[len++] = 0xc0;
                    data[len++] = 0x0c;

                    data[len++] = 0x00;
                    data[len++] = 0x01;
                    data[len++] = 0x00;
                    data[len++] = 0x01;

                    data[len++] = 0x00;
                    data[len++] = 0x00;
                    data[len++] = 0x00;
                    data[len++] = 0x0A;

                    data[len++] = 0x00;
                    data[len++] = 0x04;

                    data[len++] = 192;
                    data[len++] = 168;
                    data[len++] = 6;
                    data[len++] = 1;

                    sendto(dns_sock, data, len, 0, (struct sockaddr *)&client, client_len);
                }
            }else if(FD_ISSET(web_sock, &set)){
                int fd = accept(web_sock, (struct sockaddr *) &client, &client_len);
                if (fd > 0) {
                    int i = 0;
                    for (i = 0; i < sizeof(cli_sock)/sizeof(cli_sock[0]); i++){
                        if(cli_sock[i] == -1){
                            cli_sock[i] = fd;
                            ESP_LOGI(TAG, "tcps client connect %d", fd);
                            break;
                        }
                    }
                    if(i == sizeof(cli_sock)/sizeof(cli_sock[0])){
                        close(fd);
                    }
                }
            }else{
                char *uri = NULL;
                char buff[1024];
                for (int i = 0; i < sizeof(cli_sock)/sizeof(cli_sock[0]); i ++){
                    if (cli_sock[i] != -1 && FD_ISSET(cli_sock[i], &set)){
                        int len = recv(cli_sock[i], buff, 1024-1, 0);
                        buff[len] = 0;
                        if (len > 0){

                            char *host_p = strstr(buff, "Host");
                            char *host_end_p = strstr(host_p, "\r\n");
                            host_end_p[0] = 0;
                            char *host = host_p + strlen("Host: ");

                            // 解析请求类型及请求URI
                            uri = strstr(buff, "HTTP");
                            if (uri == NULL)
                            {
                                ESP_LOGE(TAG, "Parase requst header error!");
                                break;
                            }
                            uri[0] = 0;
                            uri = NULL;

                            uri = strstr(buff, " ");
                            if (uri == NULL)
                            {
                                ESP_LOGE(TAG, "Parase requst uri error!");
                                break;
                            }
                            uri[0] = 0;
                            uri++;

                            ESP_LOGI(TAG, "the reqqust type is %s, host: %s uri is: %s", buff, host, uri);

                            // 响应GET请求
                            if (strcmp(buff, "GET") == 0)
                            {
                                for (size_t i = 0; i < sizeof(g_resp_ctx)/sizeof(g_resp_ctx[0]); i++){
                                    if (g_resp_ctx[i].str != NULL){
                                        if (strstr((const char *)host, g_resp_ctx[i].str)){
                                            g_curr_type = i;
                                            break;
                                        }
                                    }
                                    g_curr_type = i;
                                }
                                
                                ESP_LOGI(TAG, "response %s", g_resp_ctx[g_curr_type].resp);
                                send(cli_sock[i], (void *)g_resp_ctx[g_curr_type].resp, strlen(g_resp_ctx[g_curr_type].resp), 0);
                            }
                            else
                            {
                                send(cli_sock[i], (void *)HTTP_400, strlen(HTTP_400), 0);
                            }
                        }else{
                            uint8_t need_close = 0;
                            if (len == 0){
                                // CC_LOGE(TAG, "tcps client: %d recv len = 0", i);
                                need_close = 1;
                            }else{
                                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                                    
                                } else {
                                    ESP_LOGE(TAG, "tcps client: %d recv len < 0 & errno:%d", i, errno);
                                    need_close = 1;
                                }
                            }
                            if(need_close){
                                cli_sock[i] = -1;
                            }
                        }
                    }
                }

            }
        }
        if(g_start == 0){
            goto err;
        }
        vTaskDelay(10);
    }

err:
    vTaskDelete(NULL);

}

void captive_portal_start(void){

    if(g_start == 0){
        g_start = 1;
        xTaskCreate(&captive_portal_tast, "captive_portal_task", 3072, NULL, 4, NULL);
    }
}

void captive_portal_stop(void){

    if(g_start == 1){
        g_start = 0;
    }
}