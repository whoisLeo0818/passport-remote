#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { PASSPORT_CARD, PASSPORT_REMOTE, PASSPORT_WEATHER, PASSPORT_PHOTOS, PASSPORT_METRO,
    PASSPORT_BUS, PASSPORT_SETTINGS, PASSPORT_MENU } passport_page_t;
typedef enum { KEY_UP, KEY_DOWN, KEY_OK } passport_key_t;
typedef enum { SETTINGS_LIST, SETTINGS_TIMEOUT, SETTINGS_PAIR, SETTINGS_CLEAR, SETTINGS_WIFI_INFO, SETTINGS_HOTSPOT_INFO } settings_view_t;
typedef enum { MEDIA_NONE=0, MEDIA_PREVIOUS=1, MEDIA_NEXT=2, MEDIA_TOGGLE=4,
    MEDIA_VOLUME_UP=8, MEDIA_VOLUME_DOWN=16 } media_key_t;
enum { EFFECT_SAVE=1, EFFECT_BLUETOOTH=2, EFFECT_PAIR=4, EFFECT_CLEAR=8,
    EFFECT_NEED_BT=16, EFFECT_UNAVAILABLE=32, EFFECT_WIFI=64, EFFECT_HOTSPOT=128, EFFECT_WEATHER=256 };
typedef struct { media_key_t media; unsigned effects; } passport_result_t;
typedef struct { uint16_t timeout_s; bool bluetooth; } passport_settings_t;
typedef struct {
    passport_page_t page;
    settings_view_t settings_view;
    passport_settings_t settings;
    unsigned menu, card_panel, photo, photo_count, selection, timeout_choice, confirm;
    bool fullscreen, asleep, ready, held_long, consume, repeat, wifi, hotspot;
    int held_key;
    uint32_t press_at, repeat_at, activity_at;
} passport_model_t;
bool passport_timeout_valid(unsigned seconds);
void passport_init(passport_model_t *m, passport_settings_t settings, unsigned photos, uint32_t now);
passport_result_t passport_button(passport_model_t *m, passport_key_t key, bool down, uint32_t now);
passport_result_t passport_tick(passport_model_t *m, uint32_t now);
void passport_connection(passport_model_t *m, bool ready);
void passport_cancel_input(passport_model_t *m);
void passport_input_released(passport_model_t *m, uint32_t now);
bool passport_always_on(const passport_model_t *m);
void passport_fit(unsigned image_w, unsigned image_h, unsigned box_w, unsigned box_h,
                  unsigned *out_w, unsigned *out_h);
uint32_t passport_settings_encode(passport_settings_t settings);
passport_settings_t passport_settings_decode(uint32_t value);

/* A HID press must always be followed by a zero report. Failed releases are
 * retried before the next press; link changes discard old commands. */
typedef struct { uint8_t held; uint32_t release_at; } media_pulse_t;
bool media_release_due(const media_pulse_t *pulse, uint32_t now);
void media_press_sent(media_pulse_t *pulse, media_key_t key, uint32_t now);
void media_release_sent(media_pulse_t *pulse);
