#include "passport_model.h"
#include <string.h>

static const unsigned timeouts[] = {15, 30, 60, 0};
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now-at) >= 0; }
bool passport_timeout_valid(unsigned s) { return s==0 || s==15 || s==30 || s==60; }
uint32_t passport_settings_encode(passport_settings_t s)
{
    return 0x50410000u | ((uint32_t)s.bluetooth<<8) | s.timeout_s;
}
passport_settings_t passport_settings_decode(uint32_t value)
{
    passport_settings_t s = {15, true};
    if ((value & 0xfffffe00u)==0x50410000u && passport_timeout_valid(value & 255)) {
        s.timeout_s=value & 255; s.bluetooth=(value & 256)!=0;
    }
    return s;
}
void passport_init(passport_model_t *m, passport_settings_t s, unsigned photos, uint32_t now)
{
    memset(m, 0, sizeof(*m));
    m->settings=passport_settings_decode(passport_settings_encode(s));
    m->held_key=-1; m->photo_count=photos>6?6:photos; m->activity_at=now;
}
bool passport_always_on(const passport_model_t *m)
{
    return m->page==PASSPORT_METRO || m->page==PASSPORT_BUS || !m->settings.timeout_s;
}
static unsigned move(unsigned value, unsigned count, passport_key_t key)
{
    return count ? (value+count+(key==KEY_UP?-1:1))%count : 0;
}
static passport_result_t action(passport_model_t *m, passport_key_t key, bool hold)
{
    passport_result_t r={0};
    if (hold && key==KEY_OK) {
        if (m->page==PASSPORT_MENU) m->page=PASSPORT_CARD;
        else if (m->page==PASSPORT_SETTINGS && m->settings_view!=SETTINGS_LIST)
            m->settings_view=SETTINGS_LIST;
        else m->page=PASSPORT_MENU;
        m->fullscreen=false;
        return r;
    }
    if (m->page==PASSPORT_REMOTE) {
        if (!m->ready || !m->settings.bluetooth) {r.effects=EFFECT_UNAVAILABLE;return r;}
        r.media=hold ? (key==KEY_UP?MEDIA_VOLUME_UP:MEDIA_VOLUME_DOWN) :
            key==KEY_UP?MEDIA_PREVIOUS:key==KEY_DOWN?MEDIA_NEXT:MEDIA_TOGGLE;
        return r;
    }
    if (hold) return r;
    switch (m->page) {
    case PASSPORT_REMOTE: break; /* Handled above. */
    case PASSPORT_CARD:
        if (key!=KEY_OK) m->card_panel^=1;
        break;
    case PASSPORT_MENU:
        if (key==KEY_OK) {m->page=(passport_page_t)m->menu;m->settings_view=SETTINGS_LIST;}
        else m->menu=move(m->menu,7,key);
        break;
    case PASSPORT_WEATHER:
        if(key==KEY_OK)r.effects=EFFECT_WEATHER;
        break;
    case PASSPORT_PHOTOS:
        if (m->photo_count) {
            if (key==KEY_OK) m->fullscreen=!m->fullscreen;
            else m->photo=move(m->photo,m->photo_count,key);
        }
        break;
    case PASSPORT_METRO: case PASSPORT_BUS:
        if (key!=KEY_OK) m->page=m->page==PASSPORT_METRO?PASSPORT_BUS:PASSPORT_METRO;
        break;
    case PASSPORT_SETTINGS:
        if (m->settings_view==SETTINGS_TIMEOUT) {
            if (key!=KEY_OK) m->timeout_choice=move(m->timeout_choice,4,key);
            else {m->settings.timeout_s=timeouts[m->timeout_choice];r.effects=EFFECT_SAVE;m->settings_view=SETTINGS_LIST;}
        } else if (m->settings_view==SETTINGS_CLEAR) {
            if (key!=KEY_OK) m->confirm^=1;
            else {if(m->confirm)r.effects=EFFECT_CLEAR;m->settings_view=SETTINGS_LIST;}
        } else if (m->settings_view==SETTINGS_WIFI_INFO||m->settings_view==SETTINGS_HOTSPOT_INFO) {
            if(key==KEY_OK)m->settings_view=SETTINGS_LIST;
        } else if (m->settings_view==SETTINGS_LIST) {
            if (key!=KEY_OK) m->selection=move(m->selection,8,key);
            else if (m->selection==0) {
                for(unsigned i=0;i<4;i++)if(timeouts[i]==m->settings.timeout_s)m->timeout_choice=i;
                m->settings_view=SETTINGS_TIMEOUT;
            } else if (m->selection==1) {
                m->hotspot=!m->hotspot;r.effects=EFFECT_HOTSPOT;
            } else if (m->selection==2) {
                m->settings_view=SETTINGS_HOTSPOT_INFO;
            } else if (m->selection==3) {
                m->wifi=!m->wifi;r.effects=EFFECT_WIFI;
            } else if (m->selection==4) {
                m->settings_view=SETTINGS_WIFI_INFO;
            } else if (m->selection==5) {
                m->settings.bluetooth=!m->settings.bluetooth;
                r.effects=EFFECT_SAVE|EFFECT_BLUETOOTH;
            } else if (m->selection==6) {
                if (m->settings.bluetooth) {m->settings_view=SETTINGS_PAIR;r.effects=EFFECT_PAIR;}
                else r.effects=EFFECT_NEED_BT;
            } else {m->confirm=0;m->settings_view=SETTINGS_CLEAR;}
        }
        break;
    }
    return r;
}
passport_result_t passport_tick(passport_model_t *m, uint32_t now)
{
    passport_result_t r={0};
    if (m->held_key>=0) {
        m->activity_at=now;
        if (!m->consume && !m->held_long && due(now,m->press_at+500)) {
            m->held_long=true;
            r=action(m,(passport_key_t)m->held_key,true);
            m->repeat=r.media==MEDIA_VOLUME_UP || r.media==MEDIA_VOLUME_DOWN;
            m->repeat_at=now+3000;
        } else if (m->repeat && !m->consume && due(now,m->repeat_at)) {
            r=action(m,(passport_key_t)m->held_key,true);
            m->repeat_at=now+3000;
        }
    } else if (!m->consume && !m->asleep && !passport_always_on(m) &&
               due(now,m->activity_at+(uint32_t)m->settings.timeout_s*1000)) m->asleep=true;
    return r;
}
passport_result_t passport_button(passport_model_t *m, passport_key_t key, bool down, uint32_t now)
{
    passport_result_t r={0};
    if (key>KEY_OK) return r;
    if (down) {
        if(m->held_key>=0 || m->consume)return r;
        m->held_key=key; m->press_at=now; m->held_long=false;m->repeat=false;
        if(m->asleep){m->asleep=false;m->consume=true;}
    } else if (m->held_key==(int)key) {
        m->repeat=false; /* Release never emits a scheduled repeat. */
        r=passport_tick(m,now);
        if(!m->consume && !m->held_long)r=action(m,key,false);
        m->held_key=-1;m->consume=false;m->repeat=false;
    }
    m->activity_at=now;
    return r;
}
void passport_cancel_input(passport_model_t *m) {m->held_key=-1;m->repeat=false;m->consume=true;}
void passport_input_released(passport_model_t *m,uint32_t now)
{
    if(m->consume && m->held_key<0){m->consume=false;m->activity_at=now;}
}
void passport_connection(passport_model_t *m,bool ready)
{
    if(!ready)m->repeat=false;
    m->ready=ready;
    if(ready && m->page==PASSPORT_SETTINGS && m->settings_view==SETTINGS_PAIR)m->settings_view=SETTINGS_LIST;
}
void passport_fit(unsigned iw,unsigned ih,unsigned bw,unsigned bh,unsigned *ow,unsigned *oh)
{
    *ow=*oh=0;
    if(!iw||!ih||!bw||!bh)return;
    if((uint64_t)bw*ih<=(uint64_t)bh*iw){*ow=bw;*oh=(unsigned)((uint64_t)ih*bw/iw);}
    else {*oh=bh;*ow=(unsigned)((uint64_t)iw*bh/ih);}
}

bool media_release_due(const media_pulse_t *pulse, uint32_t now)
{
    return pulse->held && (int32_t)(now - pulse->release_at) >= 0;
}
void media_press_sent(media_pulse_t *pulse, media_key_t key, uint32_t now)
{
    pulse->held = (uint8_t)key;
    pulse->release_at = now + 60;
}
void media_release_sent(media_pulse_t *pulse) { pulse->held = 0; }
