#pragma once
#include <stdbool.h>
#include "esp_err.h"
#define PASSPORT_WIFI_SSID "Passport WIFI"
#define PASSPORT_WIFI_ADDRESS "192.168.4.1"
typedef struct {
    bool enabled,busy,failed,hotspot_requested,wifi_enabled,linked,connecting,internet;
    bool scan_busy,scan_valid,store_failed;
    unsigned clients,revision;
    char ssid[33],ip[16];
} passport_wifi_status_t;
esp_err_t passport_wifi_init(void);
bool passport_wifi_enable(bool enable);
bool passport_hotspot_enable(bool enable);
void passport_wifi_internet_result(bool success);
bool passport_network_io_lock(unsigned wait_ms);
void passport_network_io_unlock(void);
passport_wifi_status_t passport_wifi_status(void);
/* Build-local credential, never logged or returned by the web API. */
const char *passport_wifi_password(void);
