#pragma once
#include <stdint.h>
#define LV_IMAGE_HEADER_MAGIC 0x19
#define LV_COLOR_FORMAT_RGB565 1
typedef struct { struct {unsigned magic,cf,w,h,stride;} header; unsigned data_size; const uint8_t *data; } lv_image_dsc_t;
