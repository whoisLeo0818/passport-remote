#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { unsigned city,minutes; } weather_config_t;
typedef struct {
    uint32_t version,city;
    int64_t observed_at,received_at;
    int16_t temperature,feels_like,wind; /* tenths of Celsius and m/s */
    uint16_t humidity,code;
} weather_sample_t;
bool weather_interval_valid(unsigned minutes);
bool weather_epoch_valid(int64_t epoch);
bool weather_sample_valid(const weather_sample_t *sample);
uint32_t weather_config_encode(weather_config_t config);
weather_config_t weather_config_decode(uint32_t word);
const char *weather_city(unsigned city);
const char *weather_condition(unsigned code);
/* Bounded ASCII approximation for the small device font. The webpage always
 * retains the full UTF-8 SSID; '?' explicitly marks unsupported code points. */
void weather_ssid_label(const char *ssid,char output[33]);
bool weather_due(uint32_t now,uint32_t deadline);
uint32_t weather_retry_ms(unsigned failures);
