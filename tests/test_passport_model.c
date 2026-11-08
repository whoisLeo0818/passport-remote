#include "passport_model.h"
#include <assert.h>
#include <stdio.h>
static uint32_t at;
static passport_result_t tap(passport_model_t *m,passport_key_t key)
{
    at+=20;passport_button(m,key,true,at);at+=100;
    return passport_button(m,key,false,at);
}
static void hold(passport_model_t *m,passport_key_t key)
{
    at+=20;passport_button(m,key,true,at);at+=500;passport_tick(m,at);
    at+=20;passport_button(m,key,false,at);
}
static void navigation(void)
{
    passport_model_t m;passport_init(&m,(passport_settings_t){15,true},6,0);at=0;
    assert(m.page==PASSPORT_CARD && m.settings.timeout_s==15);
    tap(&m,KEY_OK);assert(m.card_panel==0);
    tap(&m,KEY_UP);assert(m.card_panel==1);
    tap(&m,KEY_DOWN);assert(m.card_panel==0);
    hold(&m,KEY_OK);assert(m.page==PASSPORT_MENU);
    tap(&m,KEY_UP);assert(m.menu==6);tap(&m,KEY_DOWN);assert(m.menu==0);
    for(unsigned i=0;i<7;i++) {
        m.menu=i;tap(&m,KEY_OK);assert(m.page==(passport_page_t)i);
        hold(&m,KEY_OK);assert(m.page==PASSPORT_MENU);
    }
    hold(&m,KEY_OK);assert(m.page==PASSPORT_CARD);
    m.page=PASSPORT_WEATHER;assert(tap(&m,KEY_OK).effects==EFFECT_WEATHER);
    m.page=PASSPORT_PHOTOS;tap(&m,KEY_UP);assert(m.photo==5);
    tap(&m,KEY_OK);assert(m.fullscreen);tap(&m,KEY_DOWN);assert(m.photo==0 && m.fullscreen);
    tap(&m,KEY_OK);assert(!m.fullscreen);
    m.photo_count=0;tap(&m,KEY_OK);tap(&m,KEY_UP);assert(!m.fullscreen && m.photo==0);
    m.photo_count=1;tap(&m,KEY_UP);assert(m.photo==0);
}
static void sleep_and_volume(void)
{
    passport_model_t m;passport_init(&m,(passport_settings_t){15,true},6,0);
    passport_tick(&m,14999);assert(!m.asleep);passport_tick(&m,15000);assert(m.asleep);
    passport_button(&m,KEY_UP,true,16000);assert(!m.asleep);
    passport_tick(&m,20000);assert(m.card_panel==0);
    passport_button(&m,KEY_UP,false,21000);assert(m.card_panel==0);
    passport_button(&m,KEY_UP,true,22000);passport_button(&m,KEY_UP,false,22100);assert(m.card_panel==1);
    m.page=PASSPORT_REMOTE;passport_connection(&m,true);
    passport_button(&m,KEY_UP,true,23000);
    assert(passport_tick(&m,23499).media==MEDIA_NONE);
    assert(passport_tick(&m,23500).media==MEDIA_VOLUME_UP);
    assert(passport_tick(&m,26499).media==MEDIA_NONE);
    assert(passport_tick(&m,26500).media==MEDIA_VOLUME_UP);
    assert(passport_button(&m,KEY_UP,false,29500).media==MEDIA_NONE);
    assert(passport_tick(&m,33000).media==MEDIA_NONE);
    at=33000;assert(tap(&m,KEY_DOWN).media==MEDIA_NEXT);
    assert(tap(&m,KEY_UP).media==MEDIA_PREVIOUS);assert(tap(&m,KEY_OK).media==MEDIA_TOGGLE);
    hold(&m,KEY_OK);assert(m.page==PASSPORT_MENU);
    m.page=PASSPORT_REMOTE;passport_connection(&m,false);
    assert(tap(&m,KEY_OK).effects&EFFECT_UNAVAILABLE);assert(m.page==PASSPORT_REMOTE);
    passport_button(&m,KEY_DOWN,true,40000);assert(passport_tick(&m,40500).media==MEDIA_NONE);
    passport_connection(&m,true);assert(passport_tick(&m,43500).media==MEDIA_NONE);
    passport_button(&m,KEY_DOWN,false,43600);
    passport_button(&m,KEY_DOWN,true,44000);assert(passport_tick(&m,44500).media==MEDIA_VOLUME_DOWN);
    passport_cancel_input(&m);assert(passport_tick(&m,48000).media==MEDIA_NONE);
    passport_button(&m,KEY_OK,true,49000);assert(m.held_key==-1);
    passport_input_released(&m,50000);assert(!m.consume);
    m.page=PASSPORT_METRO;passport_tick(&m,100000);assert(!m.asleep);
    at=100000;tap(&m,KEY_DOWN);assert(m.page==PASSPORT_BUS);
    hold(&m,KEY_OK);passport_tick(&m,at+15000);assert(m.asleep);
    passport_init(&m,(passport_settings_t){0,true},6,0);passport_tick(&m,1000000);assert(!m.asleep);
    passport_init(&m,(passport_settings_t){15,true},6,UINT32_MAX-10000);
    passport_tick(&m,4998);assert(!m.asleep);passport_tick(&m,4999);assert(m.asleep);
}
static void settings_and_fit(void)
{
    passport_model_t m;passport_init(&m,(passport_settings_t){15,true},6,0);at=0;m.page=PASSPORT_SETTINGS;
    tap(&m,KEY_OK);assert(m.settings_view==SETTINGS_TIMEOUT);
    tap(&m,KEY_DOWN);hold(&m,KEY_OK);assert(m.settings.timeout_s==15 && m.settings_view==SETTINGS_LIST);
    tap(&m,KEY_OK);tap(&m,KEY_DOWN);assert(tap(&m,KEY_OK).effects&EFFECT_SAVE);assert(m.settings.timeout_s==30);
    passport_settings_t restored=passport_settings_decode(passport_settings_encode(m.settings));
    assert(restored.timeout_s==30 && restored.bluetooth);
    passport_init(&m,restored,6,at);m.page=PASSPORT_SETTINGS;
    tap(&m,KEY_DOWN);assert(tap(&m,KEY_OK).effects==EFFECT_HOTSPOT && m.hotspot && !m.wifi);
    tap(&m,KEY_DOWN);tap(&m,KEY_OK);assert(m.settings_view==SETTINGS_HOTSPOT_INFO);
    tap(&m,KEY_DOWN);assert(m.settings_view==SETTINGS_HOTSPOT_INFO);
    tap(&m,KEY_OK);assert(m.settings_view==SETTINGS_LIST);
    tap(&m,KEY_DOWN);assert(tap(&m,KEY_OK).effects==EFFECT_WIFI&&m.wifi&&m.hotspot);
    tap(&m,KEY_DOWN);tap(&m,KEY_OK);assert(m.settings_view==SETTINGS_WIFI_INFO);
    tap(&m,KEY_DOWN);assert(m.settings_view==SETTINGS_WIFI_INFO);tap(&m,KEY_OK);
    tap(&m,KEY_DOWN);assert(tap(&m,KEY_OK).effects==(EFFECT_SAVE|EFFECT_BLUETOOTH));assert(!m.settings.bluetooth);
    tap(&m,KEY_DOWN);assert(tap(&m,KEY_OK).effects==EFFECT_NEED_BT);
    tap(&m,KEY_UP);tap(&m,KEY_OK);tap(&m,KEY_DOWN);
    assert(tap(&m,KEY_OK).effects==EFFECT_PAIR && m.settings_view==SETTINGS_PAIR);
    passport_connection(&m,true);assert(m.settings_view==SETTINGS_LIST);
    tap(&m,KEY_DOWN);tap(&m,KEY_OK);assert(m.confirm==0);
    assert(tap(&m,KEY_OK).effects==0);tap(&m,KEY_OK);tap(&m,KEY_DOWN);
    assert(tap(&m,KEY_OK).effects==EFFECT_CLEAR);
    assert(m.photo_count==6 && m.settings.timeout_s==30 && m.settings.bluetooth);
    m.selection=0;tap(&m,KEY_UP);assert(m.selection==7);tap(&m,KEY_DOWN);assert(m.selection==0);
    tap(&m,KEY_DOWN);tap(&m,KEY_OK);assert(!m.hotspot&&m.wifi);
    const unsigned values[]={0,15,30,60};
    for(unsigned i=0;i<4;i++)for(unsigned b=0;b<2;b++){
        passport_settings_t s=passport_settings_decode(passport_settings_encode((passport_settings_t){values[i],b}));
        assert(s.timeout_s==values[i] && s.bluetooth==(bool)b);
    }
    assert(passport_settings_decode(0xffffffff).timeout_s==15);
    assert(passport_settings_decode(0x50410011).timeout_s==15);
    unsigned w,h;passport_fit(320,240,240,320,&w,&h);assert(w==240 && h==180);
    passport_fit(240,320,208,208,&w,&h);assert(w==156 && h==208);
    passport_fit(300,300,240,320,&w,&h);assert(w==240 && h==240);
    passport_fit(0,100,240,320,&w,&h);assert(w==0 && h==0);
}
int main(void)
{
    navigation();sleep_and_volume();settings_and_fit();
    media_pulse_t pulse = {0};
    assert(!media_release_due(&pulse, 100));
    media_press_sent(&pulse, MEDIA_NEXT, 100);
    assert(!media_release_due(&pulse, 159));
    assert(media_release_due(&pulse, 160));
    assert(media_release_due(&pulse, 180));
    media_release_sent(&pulse);
    assert(!media_release_due(&pulse, 180));
    media_press_sent(&pulse, MEDIA_PREVIOUS, UINT32_MAX - 20);
    assert(!media_release_due(&pulse, 30));
    assert(media_release_due(&pulse, 39));
    puts("Passport model tests: PASS");
}
