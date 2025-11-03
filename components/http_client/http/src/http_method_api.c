/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_client.h"
#include "http_wrappers.h"
#include "http_form_data.h"
#include "http_opts.h"

static HTTPC_RESULT http_client_common(http_client_t *client, const char *url, int method, http_client_data_t *client_data)
{
    HTTPC_RESULT ret = HTTP_ECONN;

    /* reset http_client redirect flag */
    client_data->is_redirected = 0;

    ret = http_client_conn(client, url);

    if (!ret) {
        ret = http_client_send(client, url, method, client_data);

        if (!ret) {
            ret = http_client_recv(client, client_data);
        }
    }
    /* Don't reset form data when got a redirected response */
    if(client_data->is_redirected == 0) {
        http_client_clear_form_data(client_data);
    }

    http_client_clse(client);

    return ret;
}

HTTPC_RESULT http_client_get(http_client_t *client, const char *url, http_client_data_t *client_data)
{
    int ret = http_client_common(client, url, HTTP_GET, client_data);

    while((0 == ret) && (1 == client_data->is_redirected)) {
        ret = http_client_common(client, client_data->redirect_url, HTTP_GET, client_data);
    }

    if(client_data->redirect_url != NULL) {
        http_free(client_data->redirect_url);
        client_data->redirect_url = NULL;
	}

    return ret;
}

HTTPC_RESULT http_client_head(http_client_t *client, const char *url, http_client_data_t *client_data)
{
    int ret = http_client_common(client, url, HTTP_HEAD, client_data);

    while((0 == ret) && (1 == client_data->is_redirected)) {
        ret = http_client_common(client, client_data->redirect_url, HTTP_HEAD, client_data);
    }

    if(client_data->redirect_url != NULL) {
        http_free(client_data->redirect_url);
        client_data->redirect_url = NULL;
	}

    return ret;
}

HTTPC_RESULT http_client_post(http_client_t *client, const char *url, http_client_data_t *client_data)
{
    int ret = http_client_common(client, url, HTTP_POST, client_data);

    while((0 == ret) && (1 == client_data->is_redirected)) {
        ret = http_client_common(client, client_data->redirect_url, HTTP_POST, client_data);
    }

    if(client_data->redirect_url != NULL) {
        http_free(client_data->redirect_url);
        client_data->redirect_url = NULL;
    }

    return ret;
}

HTTPC_RESULT http_client_put(http_client_t *client, const char *url, http_client_data_t *client_data)
{
    int ret = http_client_common(client, url, HTTP_PUT, client_data);

    while((0 == ret) && (1 == client_data->is_redirected)) {
        ret = http_client_common(client, client_data->redirect_url, HTTP_PUT, client_data);
    }

    if(client_data->redirect_url != NULL) {
        http_free(client_data->redirect_url);
        client_data->redirect_url = NULL;
    }

    return ret;
}

HTTPC_RESULT http_client_delete(http_client_t *client, const char *url, http_client_data_t *client_data)
{
    return http_client_common(client, url, HTTP_DELETE, client_data);
}

static HTTPC_RESULT http_client_common_request(http_client_t *client, const char *url, int method, http_client_data_t *client_data, http_client_cb_t *client_cb)
{
    HTTPC_RESULT ret = HTTP_ECONN;
    HTTPC_RESULT recv_ret = -1;
    uint8_t http_free_header_buf = 0;

    if(client_data->header_buf_len == 0){
        char *header_buf = http_malloc(HTTP_CLIENT_CHUNK_SIZE);    
        if(!header_buf){
            goto request_exit;
        }
        http_free_header_buf = 1;
        memset(header_buf, 0, HTTP_CLIENT_CHUNK_SIZE);
        client_data->header_buf = header_buf;
        client_data->header_buf_len = HTTP_CLIENT_CHUNK_SIZE;
    }


    client->client_data = client_data;
    client->client_cb = client_cb;

    /* reset http_client redirect flag */
    client_data->is_redirected = 0;
    ret = http_client_conn(client, url);

    if (!ret) {

        if(client_cb->event_cb){
            client_cb->event_cb(client, HTTP_EVENT_CONNECTED, NULL);
        }

        if(client_cb->connect_cb){
            client_cb->connect_cb(client);
        }

        ret = http_client_send(client, url, method, client_data);

        if (!ret) {
            ret = http_client_recv(client, client_data);
            while(ret == HTTP_EAGAIN){
                if(client_cb->recv_cb){
                    recv_ret = client_cb->recv_cb(client, client_data);
                    if(recv_ret != HTTP_SUCCESS){
                        goto request_exit;
                    }
                }
                ret = http_client_recv(client, client_data);
            }
            if(!ret){
                if(client_cb->recv_cb){
                    recv_ret = client_cb->recv_cb(client, client_data); 
                    if(recv_ret != HTTP_SUCCESS){
                        goto request_exit;
                    }  
                }
            }
        }

    }
    /* Don't reset form data when got a redirected response */
    if(client_data->is_redirected == 0) {
        http_client_clear_form_data(client_data);
    }

request_exit: 
    http_client_clse(client);

    if(http_free_header_buf){
        http_free(client_data->header_buf);
    }

    if(!ret){
        if(recv_ret != HTTP_SUCCESS){
            if(client_cb->event_cb){
                client_cb->event_cb(client, HTTP_EVENT_ERR, (void *)&recv_ret);
            }
        }else{
            if(client_cb->event_cb){
                client_cb->event_cb(client, HTTP_EVENT_FINISH, (void *)&client->response_code);
            }
        }
    }else{
        if(client_cb->event_cb){
            client_cb->event_cb(client, HTTP_EVENT_ERR, (void *)&ret);
        }
    }

    if(client_cb->close_cb){
        client_cb->close_cb(client);
    }

    return ret;
}

HTTPC_RESULT http_client_get_request(http_client_t *client, const char *url, http_client_data_t *client_data, http_client_cb_t *client_cb)
{
    int ret = http_client_common_request(client, url, HTTP_GET, client_data, client_cb);

    while((0 == ret) && (1 == client_data->is_redirected)) {
        ret = http_client_common_request(client, client_data->redirect_url, HTTP_GET, client_data, client_cb);
    }

    if(client_data->redirect_url != NULL) {
        http_free(client_data->redirect_url);
        client_data->redirect_url = NULL;
	}

    return ret;
}

HTTPC_RESULT http_client_post_request(http_client_t *client, const char *url, http_client_data_t *client_data, http_client_cb_t *client_cb)
{
    int ret = http_client_common_request(client, url, HTTP_POST, client_data, client_cb);

    while((0 == ret) && (1 == client_data->is_redirected)) {
        ret = http_client_common_request(client, client_data->redirect_url, HTTP_POST, client_data, client_cb);
    }

    if(client_data->redirect_url != NULL) {
        http_free(client_data->redirect_url);
        client_data->redirect_url = NULL;
    }

    return ret;
}