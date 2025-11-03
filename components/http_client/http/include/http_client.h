/**
 * @file http_client.h
 * http API header file.
 *
 * @version   V1.0
 * @date      2019-12-24
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define CONFIG_HTTP_FILE_OPERATE            1

#if CONFIG_HTTP_FILE_OPERATE
#include <stdio.h>
#define HTTP_FILE       FILE
#define http_fopen(name, modes)             fopen(name, modes)
#define http_fread(buf, size, n, file)      fread(buf, size, n, file)
#define http_fseek(file, off, whence)       fseek(file, off, whence)
#define http_ftell(file)                    ftell(file)
#define http_feof(file)                     feof(file)
#define http_fclose(file)                   fclose(file)
#endif

#include <stdlib.h>
#define http_malloc(size)                   malloc(size)
#define http_free(ptr)                      free(ptr)

/** @defgroup aos_http_client_api http
  * @{
  */

/** @brief   http requst type */
typedef enum {
    HTTP_DELETE,
    HTTP_GET,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_PUT
} HTTP_REQUEST_TYPE;

/** @brief   http error code */
typedef enum {
    HTTP_EAGAIN   =  1,  /**< more data to retrieved */
    HTTP_SUCCESS  =  0,  /**< operation success      */
    HTTP_ENOBUFS  = -1,  /**< buffer error           */
    HTTP_EARG     = -2,  /**< illegal argument       */
    HTTP_ENOTSUPP = -3,  /**< not support            */
    HTTP_EDNS     = -4,  /**< DNS fail               */
    HTTP_ECONN    = -5,  /**< connect fail           */
    HTTP_ESEND    = -6,  /**< send packet fail       */
    HTTP_ECLSD    = -7,  /**< connect closed         */
    HTTP_ERECV    = -8,  /**< recv packet fail       */
    HTTP_EPARSE   = -9,  /**< url parse error        */
    HTTP_EPROTO   = -10, /**< protocol error         */
    HTTP_EUNKOWN  = -11, /**< unknown error          */
    HTTP_ETIMEOUT = -12, /**< timeout                */
} HTTPC_RESULT;

typedef enum {
    HTTP_EVENT_ERR,
    HTTP_EVENT_DNS_GET_IP,
    HTTP_EVENT_CONNECTED,
    HTTP_EVENT_HEADER_SENTED,
    HTTP_EVENT_RECV_HEADER,
    HTTP_EVENT_RECV_DATA,
    HTTP_EVENT_FINISH,
    HTTP_EVENT_CLOSE,
} HTTP_EVENT;

/** @brief   This structure defines the http_client_t structure   */
typedef struct {
    int socket;                     /**< socket ID                 */
    int remote_port;                /**< hTTP or HTTPS port        */
    int response_code;              /**< response code             */
    char *header;                   /**< request custom header     */
    char *auth_user;                /**< username for basic authentication         */
    char *auth_password;            /**< password for basic authentication         */
    bool is_http;                   /**< http connection? if 1, http; if 0, https  */
    void *client_data;
    void *client_cb;
    void *user_arg;
#if CONFIG_HTTP_SECURE
    const char *server_cert;        /**< server certification      */
    const char *client_cert;        /**< client certification      */
    const char *client_pk;          /**< client private key        */
    int server_cert_len;            /**< server certification lenght, server_cert buffer size  */
    int client_cert_len;            /**< client certification lenght, client_cert buffer size  */
    int client_pk_len;              /**< client private key lenght, client_pk buffer size      */
    void *ssl;                      /**< ssl content               */
#endif
} http_client_t;

/** @brief   This structure defines the HTTP data structure.  */
typedef struct {
    bool is_more;                /**< indicates if more data needs to be retrieved. */
    bool is_chunked;             /**< response data is encoded in portions/chunks.*/
    int retrieve_len;            /**< content length to be retrieved. */
    int response_content_len;    /**< response content length. */
    int content_block_len;       /**< the content length of one block. */
    int response_len;
    int post_buf_len;            /**< post data length. */
    int response_buf_len;        /**< response body buffer length. */
    int header_buf_len;          /**< response head buffer lehgth. */
    char *post_content_type;     /**< content type of the post data. */
    char *post_buf;              /**< user data to be posted. */
    char *response_buf;          /**< buffer to store the response body data. */
    char *header_buf;            /**< buffer to store the response head data. */
    bool  is_redirected;         /**< redirected URL? if 1, has redirect url; if 0, no redirect url */
    char* redirect_url;          /**< redirect url when got http 3** response code. */
    HTTPC_RESULT (*post_write_cb)(http_client_t *, char* post_buf, int post_buf_len, int *write_len); 
} http_client_data_t;

typedef struct{
    void (*event_cb)(http_client_t* client, HTTP_EVENT event, void *data);
    void (*connect_cb)(http_client_t*);
    HTTPC_RESULT (*recv_cb)(http_client_t *, http_client_data_t *); 
    void (*close_cb)(http_client_t *); 
}http_client_cb_t;
/**
 * This function executes a GET request on a given URL. It blocks until completion.
 * @param[in] client             client is a pointer to the #http_client_t.
 * @param[in] url                url is the URL to run the request.
 * @param[in, out] client_data   client_data is a pointer to the #http_client_data_t instance to collect the data returned by the request.
 * @return           Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_get(http_client_t *client, const char *url, http_client_data_t *client_data);

HTTPC_RESULT http_client_get_request(http_client_t *client, const char *url, http_client_data_t *client_data, http_client_cb_t *client_cb);
HTTPC_RESULT http_client_post_request(http_client_t *client, const char *url, http_client_data_t *client_data, http_client_cb_t *client_cb);
/**
 * This function executes a HEAD request on a given URL. It blocks until completion.
 * @param[in] client             client is a pointer to the #http_client_t.
 * @param[in] url                url is the URL to run the request.
 * @param[in, out] client_data   client_data is a pointer to the #http_client_data_t instance to collect the data returned by the request.
 * @return           Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_head(http_client_t *client, const char *url, http_client_data_t *client_data);

/**
 * This function executes a POST request on a given URL. It blocks until completion.
 * @param[in] client              client is a pointer to the #http_client_t.
 * @param[in] url                 url is the URL to run the request.
 * @param[in, out] client_data    client_data is a pointer to the #http_client_data_t instance to collect the data returned by the request. It also contains the data to be posted.
 * @return           Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_post(http_client_t *client, const char *url, http_client_data_t *client_data);

/**
 * This function executes a PUT request on a given URL. It blocks until completion.
 * @param[in] client              client is a pointer to the #http_client_t.
 * @param[in] url                 url is the URL to run the request.
 * @param[in, out] client_data    client_data is a pointer to the #http_client_data_t instance to collect the data returned by the request. It also contains the data to be put.
 * @return           Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_put(http_client_t *client, const char *url, http_client_data_t *client_data);

/**
 * This function executes a DELETE request on a given URL. It blocks until completion.
 * @param[in] client               client is a pointer to the #http_client_t.
 * @param[in] url                  url is the URL to run the request.
 * @param[in, out] client_data client_data is a pointer to the #http_client_data_t instance to collect the data returned by the request.
 * @return           Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_delete(http_client_t *client, const char *url, http_client_data_t *client_data);

/**
 * This function allocates buffer for http header/response
 * @param[in] client_data      pointer to the #http_client_data_t.
 * @param[in] header_size      header buffer size
 * @param[in] resp_size        response buffer size
 * @return  HTTP_SUCCESS       success
 * @return  HTTP_EUNKOWN       fail
 */
HTTPC_RESULT http_client_prepare(http_client_data_t *client_data, int header_size, int resp_size);

/**
 * This function deallocates buffer for http header/response.
 * @param[in] client_data      pointer to the #http_client_data_t.
 * @return  HTTP_SUCCESS       success
 * @return  HTTP_EUNKOWN       fail
 */
HTTPC_RESULT http_client_unprepare(http_client_data_t *client_data);

/**
 * This function reset buffer for  http header/response.
 * @param[in] client_data      pointer to the #http_client_data_t.
 * @return           None.
 */
void http_client_reset(http_client_data_t *client_data);

/**
 * This function establish http/https connection.
 * @param[in] client            pointer to the #http_client_t.
 * @param[in] url               remote URL
 * @return           Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_conn(http_client_t *client, const char *url);

/**
 * This function sends HTTP request.
 * @param[in] client            a pointer to the #http_client_t.
 * @param[in] url               remote URL
 * @param[in] method            http request method
 * @param[in] client_data       a pointer to #http_client_data_t.
 * @return    Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_send(http_client_t *client, const char *url, int method, http_client_data_t *client_data);

/**
 * This function receives response from remote
 * @param[in]  client               a pointer to #http_client_t.
 * @param[out] client_data          a pointer to #http_client_data_t.
 * @return     Please refer to #HTTPC_RESULT.
 */
HTTPC_RESULT http_client_recv(http_client_t *client, http_client_data_t *client_data);

/**
 * This function close http connection.
 * @param[in] client               client is a pointer to the #http_client_t.
 * @return           None.
 */
void http_client_clse(http_client_t *client);

/**
 * This function sets a custom header.
 * @param[in] client               client is a pointer to the #http_client_t.
 * @param[in] header               header is a custom header string.
 * @return           None.
 */
void http_client_set_custom_header(http_client_t *client, char *header);

/**
 * This function gets the HTTP response code assigned to the last request.
 * @param[in] client               client is a pointer to the #http_client_t.
 * @return           The HTTP response code of the last request.
 */
int http_client_get_response_code(http_client_t *client);

/**
 * This function get specified response header value.
 * @param[in] header_buf header_buf is the response header buffer.
 * @param[in] name                 name is the specified http response header name.
 * @param[in, out] val_pos         val_pos is the position of header value in header_buf.
 * @param[in, out] val_len         val_len is header value length.
 * @return           0, if value is got. Others, if errors occurred.
 */
int http_client_get_response_header_value(char *header_buf, char *name, int *val_pos, int *val_len);

/**
 * This function add text formdata information.
 * @param[in] client_data          client_data is a pointer to the #http_client_data_t.
 * @param[in] content_disposition  content_disposition is a pointer to the content disposition string.
 * @param[in] content_type         content_type is a pointer to the content type string.
 * @param[in] name                 name is a pointer to the name string.
 * @param[in] data                 data is a pointer to the data.
 * @param[in] data_len             data_len is the data length.
 * @return           The HTTP response code of the last request.
 */
int http_client_formdata_addtext(http_client_data_t* client_data, char* content_disposition, char* content_type, char* name, char* data, int data_len);

/**
 * This function add file formdata information.
 * @param[in] client_data          client_data is a pointer to the #http_client_data_t.
 * @param[in] content_disposition  content_disposition is a pointer to the content disposition string.
 * @param[in] content_type         content_type is a pointer to the content type string.
 * @param[in] file_path            file_path is a pointer to the file path.
 * @return           The HTTP response code of the last request.
 */
int http_client_formdata_addfile(http_client_data_t* client_data, char* content_disposition, char* name, char* content_type, char* file_path);

int http_client_formdata_addfile_for_data(http_client_data_t* client_data, char* content_disposition, char* name, char* content_type, char* file_name, char  *data, int data_len);

#ifdef __cplusplus
}
#endif

#endif /* http_client_H */
