/*
 * @Author: HoGC 
 * @Date: 2022-10-26 11:30:08 
 * @Last Modified by:   HoGC 
 * @Last Modified time: 2022-10-26 11:30:08 
 */
#ifndef _DOIT_PRODUCT_H_
#define _DOIT_PRODUCT_H_

#include <stdint.h>
#include <stdbool.h>

#define FRAME_PARSER_HEAD        0x22BB

typedef struct{
    uint16_t head;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[];
}__attribute__ ((packed))frame_parser_head_t;

typedef struct{
    uint8_t crc;
}__attribute__ ((packed))frame_parser_last_t;

#define FRAME_MIN_LEN          (sizeof(frame_parser_head_t) + 1 + sizeof(frame_parser_last_t))
#define FRAME_MAX_LEN          128
#define FRAME_DATA_LEN(h)      (h->len) 

bool frame_parser_init(uint32_t size);
void frame_parser_reset(void);
uint32_t frame_parser_add_buf(uint8_t *buf, uint32_t len);
bool frame_parser_get_frame(uint8_t *frame, uint32_t *len);

#endif