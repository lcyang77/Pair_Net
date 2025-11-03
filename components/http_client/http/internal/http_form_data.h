/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#ifndef HTTP_FORM_DATA
#define HTTP_FORM_DATA

#define HTTP_DATA_SIZE   1472
#define FORM_DATA_MAXLEN 32
#define CLIENT_FORM_DATA_NUM  1

typedef struct formdata_node_t formdata_node_t;
struct formdata_node_t
{
    formdata_node_t *next;
    int   is_file;
    char  file_path[FORM_DATA_MAXLEN];
    char  *file_data;
    long int  file_data_len;
    char  *data;
    int   data_len;
};

typedef struct {
    int                is_used;
    formdata_node_t    *form_data;
    http_client_data_t  *client_data;
} formdata_info_t;

void http_client_clear_form_data(http_client_data_t * client_data);
int http_client_formdata_len(http_client_data_t *client_data);
int http_client_send_formdata(http_client_t *client, http_client_data_t *client_data);
int http_client_formdata_addfile_for_data(http_client_data_t* client_data, char* content_disposition, char* name, char* content_type, char* file_name, char  *data, int data_len);


#endif
