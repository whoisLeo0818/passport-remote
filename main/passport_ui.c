#include "passport_ui.h"
#include "passport_text.h"
#include "passport_photos.h"
#include "passport_font_inventory.h"
#include "passport_assets.h"
#include "passport_wifi.h"
#include "passport_weather.h"
#include "src/misc/cache/instance/lv_image_cache.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

LV_FONT_DECLARE(passport_font_12);
LV_FONT_DECLARE(passport_font_16);
LV_FONT_DECLARE(passport_font_20);
static lv_obj_t *s_screen;
static char s_signature[224];
enum { BG=0x10232c, PANEL=0x213c45, INK=0xe7f0e8, MUTED=0xa2bdc3, ACCENT=0x8ad6c2 };
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(p);lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,radius,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *text(lv_obj_t *p,int x,int y,int w,const char *value,uint32_t color,bool large)
{
    lv_obj_t *o=lv_label_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,large?&passport_font_20:&passport_font_16,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);lv_obj_set_style_text_line_space(o,3,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_label_set_text(o,value);return o;
}
static void center(int y,const char *value,bool large)
{
    lv_obj_t *o=text(s_screen,16,y,208,value,INK,large);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
}
static lv_obj_t *small_text(lv_obj_t *parent,int x,int y,int w,const char *value,uint32_t color)
{
    lv_obj_t *o=text(parent,x,y,w,value,color,false);
    lv_obj_set_style_text_font(o,&passport_font_12,0);
    return o;
}
static void small_center(int y,const char *value,uint32_t color)
{
    lv_obj_t *o=small_text(s_screen,16,y,208,value,color);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
}
static lv_obj_t *outline(lv_obj_t *parent,int x,int y,int w,int h,int radius,uint32_t color)
{
    lv_obj_t *o=box(parent,x,y,w,h,BG,radius);
    lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(o,1,0);
    lv_obj_set_style_border_color(o,lv_color_hex(color),0);
    lv_obj_set_style_border_opa(o,LV_OPA_COVER,0);
    return o;
}
static void music_battery(int battery,const char *percent)
{
    outline(s_screen,171,17,14,9,2,MUTED);
    box(s_screen,185,20,2,3,MUTED,0);
    if(battery>0){
        int fill=(battery>100?100:battery)*10/100;
        box(s_screen,173,19,fill?fill:1,5,MUTED,0);
    }
    lv_obj_t *o=small_text(s_screen,190,13,32,percent,MUTED);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_RIGHT,0);
}
static void skip_icon(int x,bool forward)
{
    /* LVGL retains the point arrays, so they have application lifetime. */
    static const lv_point_precise_t previous[]={{17,0},{3,10},{17,20},{17,0}};
    static const lv_point_precise_t next[]={{3,0},{17,10},{3,20},{3,0}};
    static const lv_point_precise_t left_bar[]={{1,0},{1,20}};
    static const lv_point_precise_t right_bar[]={{19,0},{19,20}};
    lv_obj_t *triangle=lv_line_create(s_screen);
    lv_line_set_points(triangle,forward?next:previous,4);
    lv_obj_set_pos(triangle,x,188);
    lv_obj_set_style_line_color(triangle,lv_color_hex(INK),0);
    lv_obj_set_style_line_width(triangle,1,0);
    lv_obj_t *bar=lv_line_create(s_screen);
    lv_line_set_points(bar,forward?right_bar:left_bar,2);
    lv_obj_set_pos(bar,x,188);
    lv_obj_set_style_line_color(bar,lv_color_hex(INK),0);
    lv_obj_set_style_line_width(bar,1,0);
}
static void music_page(const passport_model_t *m,remote_status_t remote,const char *feedback)
{
    const char *connection=!m->settings.bluetooth?TXT_REMOTE_BT_OFF:
        remote.state==REMOTE_READY?TXT_REMOTE_CONNECTED:
        remote.state==REMOTE_CONNECTED?TXT_REMOTE_CONNECTING:
        remote.state==REMOTE_FAILED?TXT_REMOTE_ERROR:TXT_REMOTE_DISCONNECTED;
    text(s_screen,16,52,208,TXT_PAIR_NAME,INK,false);
    lv_obj_t *card=box(s_screen,16,87,208,78,PANEL,12);
    text(card,12,12,184,TXT_NO_METADATA,INK,true);
    small_text(card,12,46,184,connection,MUTED);
    skip_icon(38,false);
    outline(s_screen,110,188,20,20,LV_RADIUS_CIRCLE,INK);
    skip_icon(182,true);
    const char *labels[]={TXT_PREVIOUS,TXT_TOGGLE,TXT_NEXT};
    for(unsigned i=0;i<3;i++){
        lv_obj_t *o=small_text(s_screen,16+i*72,214,64,labels[i],INK);
        lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    }
    small_center(244,TXT_REMOTE_USAGE_HINT,MUTED);
    small_center(270,feedback[0]?feedback:TXT_REMOTE_PAIR_HINT,ACCENT);
    small_center(294,TXT_REMOTE_MENU_HINT,MUTED);
}
static void row(int y,const char *name,const char *value,bool selected)
{
    lv_obj_t *p=box(s_screen,16,y,208,38,selected?PANEL:BG,10);
    text(p,10,7,170,name,selected?ACCENT:INK,false);
    if(value){lv_obj_t *o=text(p,142,7,56,value,MUTED,false);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_RIGHT,0);}
}
static const char *timeout(unsigned v,char *buffer,size_t n)
{
    if(!v)return TXT_ALWAYS_ON;
    snprintf(buffer,n,"%u %s",v,TXT_SECONDS);return buffer;
}
bool passport_ui_init(void)
{
    const lv_font_t *fonts[]={&passport_font_12,&passport_font_16,&passport_font_20};bool valid=true;
    for(unsigned f=0;f<sizeof(fonts)/sizeof(fonts[0]);f++) {
        for(unsigned i=0;i<sizeof(passport_codepoints)/sizeof(passport_codepoints[0]);i++) {
            lv_font_glyph_dsc_t d={0};
            if(!lv_font_get_glyph_dsc(fonts[f],&d,passport_codepoints[i],0)||d.is_placeholder){
                ESP_LOGE("passport_ui","Missing glyph U+%04lx",(unsigned long)passport_codepoints[i]);valid=false;
            }
        }
        lv_font_glyph_dsc_t d={0};
        if(lv_font_get_glyph_dsc(fonts[f],&d,0x9f98,0)&&!d.is_placeholder)valid=false;
    }
    s_screen=box(NULL,0,0,240,320,BG,0);lv_screen_load(s_screen);
    return valid;
}
void passport_ui_clear_assets(void)
{
    if(s_screen)lv_obj_clean(s_screen);
    lv_image_cache_drop(NULL);s_signature[0]=0;
}
static void image(lv_obj_t *parent,const lv_image_dsc_t *src,int x,int y)
{
    lv_obj_t *img=lv_image_create(parent);lv_image_set_src(img,src);lv_obj_set_pos(img,x,y);
}
static int rounded_temperature(int tenths){return tenths>=0?(tenths+5)/10:-((-tenths+5)/10);}
static void weather_line(const passport_weather_status_t *w,char *buf,size_t n)
{
    if(w->valid)snprintf(buf,n,"%s %s %d°C",weather_city(w->config.city),weather_condition(w->sample.code),rounded_temperature(w->sample.temperature));
    else snprintf(buf,n,"%s",TXT_W_EMPTY);
}
static void weather_page(const passport_weather_status_t *w,const passport_wifi_status_t *wifi)
{
    char buf[96];text(s_screen,18,53,105,weather_city(w->config.city),INK,true);
    const char *state=w->updating?TXT_W_UPDATING:wifi->internet?TXT_W_ONLINE:
        w->valid?TXT_W_CACHE:wifi->linked?TXT_READY:TXT_WAIT;
    lv_obj_t *status=small_text(s_screen,126,61,96,state,ACCENT);lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_RIGHT,0);
    if(!w->valid) {
        center(120,TXT_W_EMPTY,true);
        if(!wifi->linked)center(164,TXT_W_SETUP,false);
        small_center(250,w->error==WEATHER_CLOCK?TXT_W_CLOCK:w->error==WEATHER_MEMORY?TXT_W_MEMORY:
            w->error==WEATHER_NETWORK||w->error==WEATHER_DATA?TXT_W_REQUEST_FAILED:"",MUTED);
        return;
    }
    snprintf(buf,sizeof(buf),"%d",rounded_temperature(w->sample.temperature));
    lv_obj_t *temperature=text(s_screen,18,91,139,buf,INK,true);
    lv_obj_set_style_text_font(temperature,&lv_font_montserrat_48,0);
    text(s_screen,135,119,45,"°C",INK,true);
    /* Small cloud mark, independent of Chinese/icon-font coverage. */
    box(s_screen,186,115,20,20,ACCENT,LV_RADIUS_CIRCLE);
    box(s_screen,178,126,36,13,ACCENT,8);
    text(s_screen,18,162,204,weather_condition(w->sample.code),INK,true);
    lv_obj_t *panel=box(s_screen,16,203,208,52,PANEL,12);
    const char *labels[]={TXT_W_FEELS,TXT_W_HUMIDITY,TXT_W_WIND};
    for(unsigned i=0;i<3;i++) {
        small_text(panel,10+i*67,6,64,labels[i],MUTED);
        if(i==0)snprintf(buf,sizeof(buf),"%d°C",rounded_temperature(w->sample.feels_like));
        else if(i==1)snprintf(buf,sizeof(buf),"%u%%",w->sample.humidity);
        else snprintf(buf,sizeof(buf),"%d.%d m/s",w->sample.wind/10,w->sample.wind%10);
        small_text(panel,10+i*67,27,64,buf,INK);
    }
    time_t at=(time_t)w->sample.observed_at;struct tm tm;localtime_r(&at,&tm);
    if(w->error!=WEATHER_OK&&!w->updating)
        snprintf(buf,sizeof(buf),"%s %02d:%02d · %s",TXT_W_UPDATED,tm.tm_hour,tm.tm_min,TXT_W_CACHE);
    else snprintf(buf,sizeof(buf),"%02d:%02d · %u %s",tm.tm_hour,tm.tm_min,w->config.minutes,TXT_W_FREQUENCY);
    small_center(268,buf,MUTED);
}
void passport_ui_refresh(const passport_model_t *m,remote_status_t remote,int battery,const char *feedback)
{
    time_t now=time(NULL);struct tm tm;localtime_r(&now,&tm);
    passport_wifi_status_t wifi=passport_wifi_status();
    passport_weather_status_t weather=passport_weather_status();
    char sig[224];
    snprintf(sig,sizeof(sig),"%d:%d:%d:%d:%u:%u:%u:%u:%u:%u:%u:%u:%d:%d:%d:%lld:%u:%u:%u:%s",m->page,m->settings_view,m->fullscreen,m->settings.bluetooth,
        m->menu,m->card_panel,m->photo,m->photo_count,m->selection,m->timeout_choice,m->confirm,m->settings.timeout_s,
        remote.state,remote.pairing,battery,(long long)now/60,passport_assets_revision(),wifi.revision,weather.revision,feedback);
    if(!strcmp(sig,s_signature))return;
    snprintf(s_signature,sizeof(s_signature),"%s",sig);lv_obj_clean(s_screen);
    lv_obj_set_style_bg_color(s_screen,lv_color_hex(m->page==PASSPORT_PHOTOS?0:BG),0);
    const char *names[]={TXT_CARD,TXT_REMOTE,TXT_WEATHER_PAGE,TXT_PHOTOS,TXT_METRO,TXT_BUS,TXT_SETTINGS,TXT_MENU};
    if(m->page==PASSPORT_PHOTOS && m->photo_count) {
        bool full=m->fullscreen;
        const lv_image_dsc_t *src=passport_assets_photo(m->photo,full);
        if(!src)return;
        lv_obj_t *img=lv_image_create(s_screen);lv_image_set_src(img,src);
        lv_obj_set_pos(img,(240-src->header.w)/2,(full?0:48)+((full?320:208)-src->header.h)/2);
        if(full)return;
    }
    char buf[64];snprintf(buf,sizeof(buf),battery<0?"--%%":"%d%%",battery);
    if(m->page==PASSPORT_REMOTE){
        small_text(s_screen,18,13,140,TXT_REMOTE,INK);
        music_battery(battery,buf);
        music_page(m,remote,feedback);
        return;
    }
    text(s_screen,18,13,150,names[m->page],MUTED,false);
    const char *connection=!m->settings.bluetooth?TXT_OFF:remote.state==REMOTE_READY?TXT_READY:
        remote.state==REMOTE_CONNECTED?TXT_CONNECTING:remote.state==REMOTE_FAILED?TXT_BT_ERROR:TXT_WAIT;
    lv_obj_t *status=text(s_screen,174,13,48,buf,MUTED,false);
    lv_obj_set_style_text_align(status,LV_TEXT_ALIGN_RIGHT,0);
    const char *hint=TXT_BACK_MENU;
    switch(m->page) {
    case PASSPORT_CARD: {
        if(weather.clock_synced||weather.clock_cached){
            /* Chinese weekday is selected from a fixed UTF-8 inventory below. */
            char day[7]={0};memcpy(day,TXT_WEEKDAYS+tm.tm_wday*6,6);
            if(weather.clock_synced)snprintf(buf,sizeof(buf),"%02d/%02d %s %02d:%02d",tm.tm_mon+1,tm.tm_mday,day,tm.tm_hour,tm.tm_min);
            else snprintf(buf,sizeof(buf),"%02d/%02d %02d:%02d · %s",tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,TXT_CACHE_TIME);
        }else snprintf(buf,sizeof(buf),"%s",TXT_UNKNOWN_DATE);
        lv_label_set_text(lv_obj_get_child(s_screen,0),buf);
        lv_obj_set_style_text_font(lv_obj_get_child(s_screen,0),&passport_font_12,0);
        /* Split the 208-pixel content area after a 10-pixel gutter. Rounding
         * leaves 118 pixels for the avatar and 80 for the profile (about 60:40). */
        enum { gap=10, avatar_w=(208-gap)*60/100, info_w=208-gap-avatar_w,
               info_x=16+avatar_w+gap };
        lv_obj_t *avatar=box(s_screen,16,51,avatar_w,133,PANEL,12);
        const lv_image_dsc_t *custom=passport_assets_profile(0);
        if(custom)image(avatar,custom,0,0);
        else {
            lv_obj_t *face=text(avatar,0,45,avatar_w,"(o_o)",ACCENT,true);
            lv_obj_set_style_text_align(face,LV_TEXT_ALIGN_CENTER,0);
            lv_obj_t *caption=small_text(avatar,0,91,avatar_w,TXT_AVATAR,MUTED);
            lv_obj_set_style_text_align(caption,LV_TEXT_ALIGN_CENTER,0);
        }
        if(custom)image(s_screen,passport_assets_profile(1),info_x,52);
        else text(s_screen,info_x,52,info_w,PROFILE_NAME,INK,true);
        small_text(s_screen,info_x,93,info_w,TXT_CONSTELLATION,MUTED);
        if(custom)image(s_screen,passport_assets_profile(2),info_x,114);
        else text(s_screen,info_x,114,info_w,PROFILE_CONSTELLATION,INK,false);
        small_text(s_screen,info_x,146,info_w,TXT_ZODIAC,MUTED);
        if(custom)image(s_screen,passport_assets_profile(3),info_x,167);
        else text(s_screen,info_x,167,info_w,PROFILE_ZODIAC,INK,false);
        lv_obj_t *p=box(s_screen,16,199,208,75,PANEL,13);
        small_text(p,10,8,190,m->card_panel?TXT_WEATHER:TXT_TAG,MUTED);
        if(custom && !m->card_panel)image(p,passport_assets_profile(4),10,36);
        else {
            if(m->card_panel)weather_line(&weather,buf,sizeof(buf));
            text(p,10,34,190,m->card_panel?buf:PROFILE_TAG,ACCENT,false);
            if(m->card_panel&&weather.valid&&!wifi.internet) {
                lv_obj_t *age=small_text(p,10,58,188,TXT_W_CACHE,MUTED);lv_obj_set_style_text_align(age,LV_TEXT_ALIGN_RIGHT,0);
            }
        }
        hint=TXT_CARD_HINT;break;
    }
    case PASSPORT_MENU:
        for(unsigned i=0;i<7;i++) {
            lv_obj_t *p=box(s_screen,16,46+i*33,208,31,m->menu==i?PANEL:BG,9);
            text(p,10,5,188,names[i],m->menu==i?ACCENT:INK,false);
        }
        hint=TXT_SETTING_HINT;break;
    case PASSPORT_WEATHER:
        weather_page(&weather,&wifi);hint=TXT_W_REFRESH;break;
    case PASSPORT_REMOTE: break; /* Rendered above, with a battery-only header. */
    case PASSPORT_PHOTOS:
        if(!m->photo_count)center(135,TXT_NO_PHOTOS,true);
        else {snprintf(buf,sizeof(buf),"%u / %u",m->photo+1,m->photo_count);center(263,buf,false);}
        hint=TXT_PHOTO_HINT;break;
    case PASSPORT_METRO:case PASSPORT_BUS: {
        center(55,TXT_DEMO_CODE,false);
        /* Deliberately incomplete pattern: a UX mark, not a valid ticket. */
        lv_obj_t *code=box(s_screen,56,92,128,128,0xe7f0e8,8);
        for(int y=0;y<9;y++)for(int x=0;x<9;x++)
            if(((x*7+y*11+x*y)%5)<2)box(code,10+x*12,10+y*12,9,9,BG,0);
        center(235,TXT_DEFERRED,false);center(260,TXT_CODE_ON,false);break;
    }
    case PASSPORT_SETTINGS:
        if(m->settings_view==SETTINGS_LIST){
            const char *labels[]={TXT_TIMEOUT,TXT_HOTSPOT,TXT_HOTSPOT_INFO,TXT_EXTERNAL_WIFI,TXT_WIFI_INFO,TXT_BLUETOOTH,TXT_PAIR_MODE,TXT_CLEAR};
            const char *values[]={timeout(m->settings.timeout_s,buf,sizeof(buf)),
                wifi.busy?TXT_PENDING:wifi.enabled?TXT_ON:TXT_OFF,NULL,
                wifi.wifi_enabled?TXT_ON:TXT_OFF,NULL,m->settings.bluetooth?TXT_ON:TXT_OFF,NULL,NULL};
            unsigned start=m->selection>5?m->selection-5:0;
            for(unsigned i=start;i<start+6&&i<8;i++) {
                lv_obj_t *p=box(s_screen,16,49+(i-start)*34,208,32,m->selection==i?PANEL:BG,9);
                text(p,10,5,168,labels[i],m->selection==i?ACCENT:INK,false);
                if(values[i]){lv_obj_t *v=small_text(p,134,8,64,values[i],MUTED);lv_obj_set_style_text_align(v,LV_TEXT_ALIGN_RIGHT,0);}
            }
            snprintf(buf,sizeof(buf),"%u-%u / 8 · %s",start+1,start+6,TXT_SCROLL);
            small_center(267,feedback[0]?feedback:buf,MUTED);
            hint=TXT_SETTING_HINT;
        }else if(m->settings_view==SETTINGS_HOTSPOT_INFO){
            text(s_screen,18,51,204,TXT_HOTSPOT_INFO,INK,true);
            small_text(s_screen,18,94,204,TXT_WIFI_NAME,MUTED);text(s_screen,18,111,204,PASSPORT_WIFI_SSID,INK,false);
            small_text(s_screen,18,156,204,TXT_WIFI_PASSWORD,MUTED);text(s_screen,18,173,204,passport_wifi_password(),INK,false);
            small_text(s_screen,18,218,204,TXT_WIFI_ADDRESS,MUTED);text(s_screen,18,235,204,PASSPORT_WIFI_ADDRESS,INK,false);
            small_center(267,wifi.enabled?TXT_HOTSPOT_GUIDE:TXT_HOTSPOT_OFF,MUTED);hint=TXT_WIFI_BACK;
        }else if(m->settings_view==SETTINGS_WIFI_INFO){
            text(s_screen,18,51,204,TXT_WIFI_INFO,INK,true);
            small_text(s_screen,18,91,204,TXT_NET_STATUS,MUTED);
            text(s_screen,18,109,204,!wifi.wifi_enabled?TXT_NET_OFF:wifi.connecting?TXT_CONNECTING:
                wifi.linked?(wifi.internet?TXT_W_ONLINE:TXT_NET_CHECK):wifi.failed?TXT_NET_FAILED:TXT_NET_WAIT,INK,false);
            small_text(s_screen,18,149,204,TXT_NET_NAME,MUTED);
            char network[33];weather_ssid_label(wifi.ssid,network);
            small_text(s_screen,18,168,204,network[0]?network:"--",INK);
            small_text(s_screen,18,207,204,TXT_NET_IP,MUTED);
            text(s_screen,18,225,204,wifi.ip[0]?wifi.ip:"--",INK,false);
            small_center(267,strchr(network,'?')?TXT_NET_SSID_GUIDE:TXT_NET_GUIDE,MUTED);hint=TXT_WIFI_BACK;
        }else if(m->settings_view==SETTINGS_TIMEOUT){
            const unsigned times[]={15,30,60,0};
            for(unsigned i=0;i<4;i++)row(58+i*44,timeout(times[i],buf,sizeof(buf)),NULL,m->timeout_choice==i);
            hint=TXT_SETTING_HINT;
        }else if(m->settings_view==SETTINGS_PAIR){
            center(60,TXT_PAIR_GUIDE,false);center(132,TXT_PAIR_NAME,true);
            center(181,connection,false);center(225,TXT_PAIR_OLD,false);
        }else{
            center(65,TXT_CLEAR_QUESTION,true);center(108,TXT_CLEAR_KEEP,false);
            row(182,TXT_CANCEL,NULL,m->confirm==0);row(227,TXT_CONFIRM,NULL,m->confirm==1);
        }break;
    }
    center(292,hint,false);
}
