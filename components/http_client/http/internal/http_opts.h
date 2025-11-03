/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#ifndef HTTP_OPTS_H
#define HTTP_OPTS_H

#ifndef HTTP_CLIENT_AUTHB_SIZE
#define HTTP_CLIENT_AUTHB_SIZE     128
#endif

#ifndef HTTP_CLIENT_CHUNK_SIZE
#define HTTP_CLIENT_CHUNK_SIZE     (2048) // 1024
#endif

#ifndef HTTP_CLIENT_SEND_BUF_SIZE
#define HTTP_CLIENT_SEND_BUF_SIZE  2048 //512
#endif

#ifndef HTTP_CLIENT_MAX_HOST_LEN
#define HTTP_CLIENT_MAX_HOST_LEN   128
#endif

#ifndef HTTP_CLIENT_MAX_URL_LEN
#define HTTP_CLIENT_MAX_URL_LEN    1024 // 512
#endif

#ifndef HTTP_CLIENT_MAX_RECV_WAIT_MS
#define HTTP_CLIENT_MAX_RECV_WAIT_MS 5000
#endif

#ifndef HTTP_PORT
#define HTTP_PORT   80
#endif

#ifndef HTTPS_PORT
#define HTTPS_PORT  443
#endif

#endif /* HTTP_OPTS_H */
