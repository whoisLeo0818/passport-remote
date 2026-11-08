#pragma once
#include "passport_weather_model.h"
#include "esp_http_server.h"
typedef enum { WEATHER_OK,WEATHER_OFFLINE,WEATHER_CLOCK,WEATHER_NETWORK,WEATHER_DATA,WEATHER_MEMORY,WEATHER_STORAGE } weather_error_t;
typedef struct {
    weather_config_t config;
    weather_sample_t sample;
    bool valid,updating,clock_synced,clock_cached,store_failed;
    weather_error_t error;
    unsigned revision;
} passport_weather_status_t;
/* Initialize before first rendering; tasks never access LVGL directly. */
bool passport_weather_init(void);
passport_weather_status_t passport_weather_status(void);
void passport_weather_refresh(void); /* Nonblocking; coalesced and rate limited. */
esp_err_t passport_weather_api(httpd_req_t *req);
