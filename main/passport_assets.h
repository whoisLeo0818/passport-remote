#pragma once
#include "lvgl.h"
#include "esp_http_server.h"
#include "passport_transfer.h"
bool passport_assets_init(void);
/* Getters/descriptors and revision require the BSP LVGL lock. Descriptors have
 * application lifetime. HTTP mutations publish only while holding that lock. */
unsigned passport_assets_revision(void);
unsigned passport_assets_count(void);
const lv_image_dsc_t *passport_assets_photo(unsigned index,bool full);
const lv_image_dsc_t *passport_assets_profile(unsigned section);
/* HTTP server task only; index 0..5 refers to stable slots, not display order. */
esp_err_t passport_assets_upload(httpd_req_t *req,bool profile,unsigned index);
bool passport_assets_delete(unsigned index);
esp_err_t passport_assets_state(httpd_req_t *req,const char *token);
esp_err_t passport_assets_download(httpd_req_t *req,bool profile,unsigned index);
