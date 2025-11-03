/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "http_client.h"
#include "http_form_data.h"
#include "http_wrappers.h"

static formdata_info_t formdata_info[CLIENT_FORM_DATA_NUM] = {0};

static void form_data_clear(formdata_node_t* form_data) {
    if(form_data != NULL) {
        form_data_clear(form_data->next);
        form_data->next = NULL;
        if(form_data->data != NULL) {
            http_free(form_data->data);
        }
        http_free(form_data);
    }
}

formdata_info_t* found_formdata_info(http_client_data_t * client_data) {
    int i;

    for(i = 0; i < CLIENT_FORM_DATA_NUM; i++) {
        if((formdata_info[i].client_data == client_data)
           && (formdata_info[i].is_used == 1)) {
            break;
        }
    }

    if(i == CLIENT_FORM_DATA_NUM) {
        return NULL;
    }

    return &formdata_info[i];
}

static formdata_info_t* found_empty_formdata_info() {
    int i;

    for(i = 0; i < CLIENT_FORM_DATA_NUM; i++) {
        if(formdata_info[i].is_used == 0) {
            break;
        }
    }

    if(i == CLIENT_FORM_DATA_NUM) {
        return NULL;
    }

    return &formdata_info[i];
}

// #define TEXT_FORMAT              "\r\nContent-Disposition: %s; name=\"%s\"\r\n\r\n%s\r\n"
// #define TEXT_CONTENT_TYPE_FORMAT "\r\nContent-Disposition :%s; name=\"%s\"\r\nContent-Type:%s\r\n\r\n%s\r\n"
#define TEXT_FORMAT              "\r\nContent-Disposition: %s; name=\"%s\"\r\n\r\n%s"
#define TEXT_CONTENT_TYPE_FORMAT "\r\nContent-Disposition :%s; name=\"%s\"\r\nContent-Type:%s\r\n\r\n%s"
int http_client_formdata_addtext(http_client_data_t* client_data, char* content_disposition, char* content_type, char* name, char* data, int data_len)
{
    int buf_len;
    formdata_info_t* data_info;
    formdata_node_t* previous;
    formdata_node_t* current;

    if((content_disposition == NULL) || (name == NULL) || (data == NULL) || (data_len == 0)) {
        http_err("%s:%d invalid params", __func__, __LINE__);
        return -1;
    }

    if(strlen(data) > data_len) {
        http_err("%s:%d invalid data_len %d strlen data %d", __func__, __LINE__, data_len, strlen(data));
        return -1;
    }

    if((data_info = found_formdata_info(client_data)) == NULL) {
        if((data_info = found_empty_formdata_info()) == NULL) {
            http_err("%s:%d found no client_data info", __func__, __LINE__);
            return -1;
        }
    }

    if(data_info->is_used == 0) {
        data_info->is_used = 1;
        data_info->client_data = client_data;
        data_info->form_data = (formdata_node_t *)http_malloc(sizeof(formdata_node_t));
        if(data_info->form_data == NULL) {
            data_info->is_used = 0;
            http_err("%s:%d form data http_malloc failed", __func__, __LINE__);
            return -1;
        }
        previous = data_info->form_data;
        current = data_info->form_data;
    }
    else {
        current = data_info->form_data;

        while(current->next != NULL) {
            current = current->next;
        }

        current->next = (formdata_node_t *)http_malloc(sizeof(formdata_node_t));
        if(current->next == NULL) {
            http_err("%s:%d form data http_malloc failed", __func__, __LINE__);
            return -1;
        }
        previous = current;
        current = current->next;
    }

    memset(current, 0, sizeof(formdata_node_t));
    if(content_type == NULL) {
        buf_len = strlen(TEXT_FORMAT) - 6 + strlen(content_disposition) + strlen(name) + strlen(data) + 1;
    }
    else {
        buf_len = strlen(TEXT_CONTENT_TYPE_FORMAT) - 8 + strlen(content_disposition) + strlen(name) + strlen(data) + strlen(content_type) + 1;
    }
    current->data = (char*)http_malloc(buf_len+1);
    if( current->data == NULL) {
        if(previous == current ) {
            http_free(current);
            memset(data_info, 0, sizeof(formdata_info_t));
        }
        else {
            http_free(current);
            previous->next = NULL;
        }
        http_err("%s:%d data http_malloc failed", __func__, __LINE__);
        return -1;
    }
    memset(current->data, 0, sizeof(buf_len));
    if(content_type == NULL) {
        snprintf(current->data, buf_len, TEXT_FORMAT, content_disposition, name, data);
        current->data_len = strlen(current->data);
    }
    else {
        snprintf(current->data, buf_len, TEXT_CONTENT_TYPE_FORMAT, content_disposition, name, content_type, data);
        current->data_len = strlen(current->data);
    }
    return 0;
}

static int get_url_file_name(char* url)
{
    char * ptr = url;
    int offset = 0;
    int i = 0;

    while(*ptr != '\0')
    {
        i++;
        if(*ptr == '/') {
            offset = i;
        }
        ptr++;
    }
    return offset;
}

void http_client_clear_form_data(http_client_data_t * client_data)
{
    formdata_info_t * data_info;
    formdata_node_t * current;

    data_info = found_formdata_info(client_data);

    if(data_info == NULL) {
        http_debug("No form data info found");
        return;
    }

    current = data_info->form_data;
    if(current != NULL) {
        form_data_clear(current);
    }
    else {
        http_err("No form data in form data info");
    }

    memset(data_info, 0, sizeof(formdata_info_t));
}

#define FILE_FORMAT_START                "\r\nContent-Disposition: %s; name=\"%s\"; filename=\"%s\"\r\n"
#define FILE_FORMAT_END                  "\r\nContent-Disposition: %s; name=\"%s\"; filename=\"\"\r\n"
#define FILE_FORMAT_CONTENT_TYPE_START   "\r\nContent-Disposition: %s; name=\"%s\"; filename=\"%s\"\r\nContent-Type: %s\r\n\r\n"
int http_client_formdata_addfile(http_client_data_t* client_data, char* content_disposition, char* name, char* content_type, char* file_path)
{
    int buf_len;
    formdata_info_t* data_info;
    formdata_node_t* previous;
    formdata_node_t* current;

    if((content_disposition == NULL) || (name == NULL) || (file_path == NULL)) {
        http_err("%s:%d invalid params", __func__, __LINE__);
        return -1;
    }

    if((data_info = found_formdata_info(client_data)) == NULL) {
        if((data_info = found_empty_formdata_info()) == NULL) {
            http_err("%s:%d found no client_data info", __func__, __LINE__);
            return -1;
        }
    }

    if(data_info->is_used == 0) {
        data_info->is_used = 1;
        data_info->client_data = client_data;
        data_info->form_data = (formdata_node_t *)http_malloc(sizeof(formdata_node_t));
        if(data_info->form_data == NULL) {
            data_info->is_used = 0;
            http_err("%s:%d data http_malloc failed", __func__, __LINE__);
            return -1;
        }

        previous = data_info->form_data;
        current = data_info->form_data;
    }
    else {
        current = data_info->form_data;

        while(current->next != NULL) {
            current = current->next;
        }

        current->next = (formdata_node_t *)http_malloc(sizeof(formdata_node_t));
        if(current->next == NULL) {
            http_err("%s:%d data http_malloc failed", __func__, __LINE__);
            return -1;
        }
        previous = current;
        current = current->next;
    }

    memset(current, 0, sizeof(formdata_node_t));
    if(content_type == NULL) {
        buf_len = strlen(FILE_FORMAT_START) - 6 + strlen(content_disposition) + strlen(name) + strlen(file_path) - get_url_file_name(file_path) + 1;
    }
    else {
        buf_len = strlen(FILE_FORMAT_CONTENT_TYPE_START) - 8 + strlen(content_disposition) + strlen(name) + strlen(file_path) - get_url_file_name(file_path) + strlen(content_type) + 1;
    }
    current->data = (char*)http_malloc(buf_len + 1);
    if( current->data == NULL) {
        if(previous == current ) {
            http_free(current);
        } else {
            http_free(current);
            previous->next = NULL;
        }
        http_err("%s:%d data http_malloc failed", __func__, __LINE__);
        return -1;
    }
    memset(current->data, 0, sizeof(buf_len));

    current->is_file = 1;

    memcpy(current->file_path, file_path, strlen(file_path));
    if(content_type == NULL) {
        snprintf(current->data, buf_len, FILE_FORMAT_START, content_disposition, name, file_path + get_url_file_name(file_path));
    }
    else {
        snprintf(current->data, buf_len, FILE_FORMAT_CONTENT_TYPE_START, content_disposition, name, file_path + get_url_file_name(file_path), content_type);
    }
    current->data_len = strlen(current->data);
    return 0;
}
int http_client_formdata_addfile_for_data(http_client_data_t* client_data, char* content_disposition, char* name, char* content_type, char* file_name, char  *data, int data_len)
{
    int buf_len;
    formdata_info_t* data_info;
    formdata_node_t* previous;
    formdata_node_t* current;

    if((content_disposition == NULL) || (name == NULL)) {
        http_err("%s:%d invalid params", __func__, __LINE__);
        return -1;
    }

    if((data_info = found_formdata_info(client_data)) == NULL) {
        if((data_info = found_empty_formdata_info()) == NULL) {
            http_err("%s:%d found no client_data info", __func__, __LINE__);
            return -1;
        }
    }

    if(data_info->is_used == 0) {
        data_info->is_used = 1;
        data_info->client_data = client_data;
        data_info->form_data = (formdata_node_t *)http_malloc(sizeof(formdata_node_t));
        if(data_info->form_data == NULL) {
            data_info->is_used = 0;
            http_err("%s:%d data http_malloc failed", __func__, __LINE__);
            return -1;
        }

        previous = data_info->form_data;
        current = data_info->form_data;
    }
    else {
        current = data_info->form_data;

        while(current->next != NULL) {
            current = current->next;
        }

        current->next = (formdata_node_t *)http_malloc(sizeof(formdata_node_t));
        if(current->next == NULL) {
            http_err("%s:%d data http_malloc failed", __func__, __LINE__);
            return -1;
        }
        previous = current;
        current = current->next;
    }

    memset(current, 0, sizeof(formdata_node_t));
    if(content_type == NULL) {
        buf_len = strlen(FILE_FORMAT_START) - 6 + strlen(content_disposition) + strlen(name) + strlen(file_name) + 1;
    }
    else {
        buf_len = strlen(FILE_FORMAT_CONTENT_TYPE_START) - 8 + strlen(content_disposition) + strlen(name) + strlen(file_name) + strlen(content_type) + 1;
    }
    current->data = (char*)http_malloc(buf_len + 1);
    if( current->data == NULL) {
        if(previous == current ) {
            http_free(current);
        }
        else {
            http_free(current);
            previous->next = NULL;
        }
        http_err("%s:%d data http_malloc failed", __func__, __LINE__);
        return -1;
    }
    memset(current->data, 0, sizeof(buf_len));

    current->is_file = 1;

    current->file_path[0] = '\0';
    
    if(content_type == NULL) {
        snprintf(current->data, buf_len, FILE_FORMAT_START, content_disposition, name, file_name);
    }
    else {
        snprintf(current->data, buf_len, FILE_FORMAT_CONTENT_TYPE_START, content_disposition, name, file_name, content_type);
    }
    current->data_len = strlen(current->data);

    current->file_data = data;
    current->file_data_len = data_len;
    return 0;
}

static const char *boundary = "----WebKitFormBoundarypNjgoVtFRlzPquKE";
int http_client_formdata_len(http_client_data_t *client_data)
{
	int total_len = 0;
	formdata_info_t* data_info = NULL;
	formdata_node_t * current;

    data_info = found_formdata_info(client_data);
    if ((NULL == data_info) || (0 == data_info->is_used)) {
    	return 0;
    }

    current = data_info->form_data;
    /* calculate content-length*/
    do {
        if(current->is_file == 1){
            if(strlen(current->file_path) == 0) {
                total_len += current->file_data_len;
            }
#if CONFIG_HTTP_FILE_OPERATE
            else{
                HTTP_FILE* fd;
                long size;

                fd = http_fopen(current->file_path, "rb");
                if(fd == NULL) {
                    http_err("%s: open file(%s) failed errno=%d", __func__, current->file_path, errno);
                    return -1;
                }

                http_fseek(fd,0,SEEK_END);
                size=http_ftell(fd);
                http_fseek(fd,0,SEEK_SET);
                http_fclose(fd);
                total_len += size;
            }
#endif
        }

        total_len += current->data_len;
        total_len += strlen(boundary) + 4;
        current = current->next;
    } while(current != NULL);

     
    return total_len;
}

int http_client_send_formdata(http_client_t *client, http_client_data_t *client_data)
{
	int ret;
    formdata_info_t* data_info = NULL;
	formdata_node_t * current;
	char data[HTTP_DATA_SIZE] = {0};

    data_info = found_formdata_info(client_data);
    if ((NULL == data_info) || (0 == data_info->is_used)) {
    	return 0;
    }

    current = data_info->form_data;

    while(current != NULL) {
        /* set boundary */
        memset(data, 0, sizeof(data));
        snprintf(data, sizeof(data), "\r\n--%s", boundary);
        ret = http_tcp_send_wrapper(client, data, strlen(data));
        if (ret <= 0) {
            return -1;
        }
       
        ret = http_tcp_send_wrapper(client, current->data, current->data_len);
        if (ret <= 0) {
            return -1;
        }
        
        if(current->is_file == 1 ) {
            break;
        }
        current = current->next;
    }

    if(current == NULL) {
        memset(data, 0, sizeof(data));
        snprintf(data, sizeof(data), "\r\n--%s--\r\n", boundary);
        ret = http_tcp_send_wrapper(client, data, strlen(data));
        if (ret <= 0) {
            return -1;
        }
        return 0;
    }

    if(strlen(current->file_path) == 0){
        long int left_len = 0;
        left_len = current->file_data_len;
        http_debug("send file len %d bytes", current->file_data_len);
        while(left_len) {
            long int idx = current->file_data_len - left_len;
            long int send_len = (left_len > HTTP_DATA_SIZE)?HTTP_DATA_SIZE:left_len;
            ret = http_tcp_send_wrapper(client, current->file_data + idx, send_len);
            if (ret > 0) {
                http_debug("Written %d bytes", ret);
            } else if ( ret == 0 ) {
                http_err("ret == 0,Connection was closed by server");
                return HTTP_ECLSD; /* Connection was closed by server */
            } else {
                http_err("Connection error (send returned %d) errno=%d", ret, errno);
                return HTTP_ECONN;
            }
            left_len -= send_len;
        }
    }
#if CONFIG_HTTP_FILE_OPERATE
    else{
	    HTTP_FILE* fd = http_fopen(current->file_path, "rb");
	    if(fd == NULL) {
	        http_err("%s: open file(%s) failed errno=%d", __func__, current->file_path, errno);
	        return -1;
	    }

	    while(!http_feof(fd)) {
	        ret = http_fread(data, 1, sizeof(data), fd);
	        if(ret <= 0) {
	            http_err("http_fread failed returned %d errno=%d", ret, errno);
	            return -1;
	        }

	        ret = http_tcp_send_wrapper(client, data, ret);
	        if (ret > 0) {
	            http_debug("Written %d bytes", ret);
	        } else if ( ret == 0 ) {
	            http_err("ret == 0,Connection was closed by server");
	            return HTTP_ECLSD; /* Connection was closed by server */
	        } else {
	            http_err("Connection error (send returned %d) errno=%d", ret, errno);
	            return HTTP_ECONN;
	        }

	        memset(data, 0, sizeof(data));
	    }

	    http_fclose(fd);
    }
#endif

    current = current->next;
    while(current != NULL) {
        memset(data, 0, sizeof(data));
        snprintf(data, sizeof(data), "\r\n--%s", boundary);
        ret = http_tcp_send_wrapper(client, data, strlen(data));
        if (ret <= 0) {
            return -1;
        }

        ret = http_tcp_send_wrapper(client, current->data, current->data_len);
        if (ret <= 0) {
            return -1;
        }
        current = current->next;
    }

    memset(data, 0, sizeof(data));
    snprintf(data, sizeof(data), "\r\n--%s--\r\n", boundary);
    ret = http_tcp_send_wrapper(client, data, strlen(data));
    if (ret <= 0) {
        return -1;
    }

    return 0;
}
