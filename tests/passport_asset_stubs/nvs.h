#pragma once
#include "esp_err.h"
#include <stdint.h>
#define NVS_READWRITE 1
typedef unsigned nvs_handle_t;
esp_err_t nvs_open(const char *,int,nvs_handle_t *);
esp_err_t nvs_get_u32(nvs_handle_t,const char *,uint32_t *);
esp_err_t nvs_set_u32(nvs_handle_t,const char *,uint32_t);
esp_err_t nvs_commit(nvs_handle_t);
