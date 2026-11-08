#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "key_guard.h"
#include "passport_ui.h"
#include "passport_store.h"
#include "passport_text.h"
#include "passport_assets.h"
#include "passport_wifi.h"
#include "passport_weather.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

typedef struct {passport_key_t key;bool down;uint32_t at;} input_t;
static QueueHandle_t s_input;
static portMUX_TYPE s_mux=portMUX_INITIALIZER_UNLOCKED;
static bool s_lost;
static key_guard_t s_key_guard;
static volatile bool s_input_ready;
static passport_model_t s_model;
static bool s_store_ok,s_ble_ok;
static const char *s_feedback="";
static uint32_t s_feedback_at;
static uint32_t clock_ms(void){return (uint32_t)(esp_timer_get_time()/1000);}
static void feedback(const char *text){s_feedback=text;s_feedback_at=clock_ms()+2500;}
static void on_key(bsp_btn_t key,bsp_btn_ev_t event,void *user)
{
    (void)user;
    taskENTER_CRITICAL(&s_mux);
    bool guarded=key_guard_consume(&s_key_guard,event==BSP_BTN_RELEASE,esp_timer_get_time());
    taskEXIT_CRITICAL(&s_mux);
    if(guarded || !s_input_ready)return;
    /* Use physical down/up instead of delayed CLICK/DOUBLE callbacks so each
     * press is classified once by the same testable 500-ms state machine. */
    if(event!=BSP_BTN_PRESS && event!=BSP_BTN_RELEASE)return;
    input_t input={key==BSP_BTN_UP?KEY_UP:key==BSP_BTN_DOWN?KEY_DOWN:KEY_OK,
                   event==BSP_BTN_PRESS,clock_ms()};
    if(xQueueSend(s_input,&input,0)!=pdTRUE){
        taskENTER_CRITICAL(&s_mux);s_lost=true;taskEXIT_CRITICAL(&s_mux);
    }
}
static void apply(passport_result_t r)
{
    if(r.media!=MEDIA_NONE){
        if(!s_ble_ok || !passport_ble_send(r.media))feedback(TXT_UNAVAILABLE);
        else if(r.media==MEDIA_PREVIOUS)feedback(TXT_PREVIOUS);
        else if(r.media==MEDIA_NEXT)feedback(TXT_NEXT);
        else feedback(r.media==MEDIA_VOLUME_UP?TXT_VOLUME_UP:r.media==MEDIA_VOLUME_DOWN?TXT_VOLUME_DOWN:TXT_SENT);
    }
    if(r.effects&EFFECT_SAVE)
        if(!s_store_ok || !passport_store_save(s_model.settings))feedback(TXT_SAVE_FAILED);
    if(r.effects&EFFECT_NEED_BT)feedback(TXT_NEED_BT);
    if(r.effects&EFFECT_UNAVAILABLE)feedback(TXT_UNAVAILABLE);
    if(r.effects&EFFECT_WIFI) {
        if(!passport_wifi_enable(s_model.wifi)){s_model.wifi=false;feedback(TXT_BT_ERROR);}
    }
    if(r.effects&EFFECT_HOTSPOT) {
        if(!passport_hotspot_enable(s_model.hotspot)){s_model.hotspot=false;feedback(TXT_BT_ERROR);}
    }
    if(r.effects&EFFECT_WEATHER)passport_weather_refresh();
    passport_ble_op_t op=BLE_ENABLE;bool control=false;
    if(r.effects&EFFECT_BLUETOOTH){op=s_model.settings.bluetooth?BLE_ENABLE:BLE_DISABLE;control=true;}
    if(r.effects&EFFECT_PAIR){op=BLE_PAIR;control=true;}
    if(r.effects&EFFECT_CLEAR){op=BLE_CLEAR;control=true;}
    if(control){
        if(!s_ble_ok || !passport_ble_control(op))feedback(TXT_FAILED);
        else feedback(TXT_PENDING);
    }
}
void app_main(void)
{
    esp_err_t nvs=nvs_flash_init(); /* Never auto-erase on incompatibility. */
    passport_settings_t settings=passport_settings_decode(0);
    s_store_ok=nvs==ESP_OK && passport_store_init(&settings);
    bool assets_ok=nvs==ESP_OK && passport_assets_init();
    if(!assets_ok)ESP_LOGE("passport","File storage unavailable");
    if(nvs!=ESP_OK||!passport_weather_init())ESP_LOGE("passport","Weather service unavailable");
    passport_init(&s_model,settings,passport_assets_count(),clock_ms());
    passport_cancel_input(&s_model); /* Ignore startup-held keys until release. */
    ESP_ERROR_CHECK(bsp_display_init());
    if(!bsp_lvgl_init()){ESP_LOGE("passport","LVGL unavailable");return;}
    if(!bsp_lvgl_lock(2000))return;
    bool fonts_ok=passport_ui_init();
    remote_status_t remote={0};
    passport_ui_refresh(&s_model,remote,-1,fonts_ok?"":TXT_FONT_FAILED);
    bsp_lvgl_unlock();bsp_display_backlight(70);
    s_input=xQueueCreate(24,sizeof(input_t));
    if(!s_input){ESP_LOGE("passport","Input allocation failed");return;}
    key_guard_arm(&s_key_guard,esp_timer_get_time(),3000000);
    bool buttons_ok=bsp_button_init(on_key,NULL)==ESP_OK;
    int boot_mv=buttons_ok?bsp_button_read_mv():-1;
    taskENTER_CRITICAL(&s_mux);
    key_guard_sync_released(&s_key_guard,boot_mv>=BSP_BTN_MV_RELEASED_MIN);
    taskEXIT_CRITICAL(&s_mux);
    s_input_ready=true;
    bool battery_ok=bsp_battery_init()==ESP_OK;
    s_ble_ok=nvs==ESP_OK && passport_ble_start(settings.bluetooth)==ESP_OK;
    if(passport_wifi_init()!=ESP_OK)ESP_LOGE("passport","Wi-Fi service unavailable");
    if(!s_ble_ok)ESP_LOGE("passport","BLE unavailable, NVS=%s",esp_err_to_name(nvs));
    int battery=-1;uint32_t next_battery=0,next_heap=0,controls=0,failures=0;
    bool display_off=false;
    for(;;){
        uint32_t now=clock_ms();
        taskENTER_CRITICAL(&s_mux);bool lost=s_lost;s_lost=false;taskEXIT_CRITICAL(&s_mux);
        if(lost){xQueueReset(s_input);passport_cancel_input(&s_model);}
        remote=passport_ble_status();if(!s_ble_ok)remote.state=REMOTE_FAILED;
        passport_connection(&s_model,remote.state==REMOTE_READY);
        passport_wifi_status_t wifi=passport_wifi_status();
        s_model.wifi=wifi.wifi_enabled;s_model.hotspot=wifi.hotspot_requested;
        input_t input;
        while(xQueueReceive(s_input,&input,0)==pdTRUE)apply(passport_button(&s_model,input.key,input.down,input.at));
        /* An overflowing queue or a held startup key cannot leave a repeating
         * command active. Recovery requires a measured released keypad. */
        if(s_model.consume && s_model.held_key<0 && buttons_ok && bsp_button_read_mv()>=BSP_BTN_MV_RELEASED_MIN)
            passport_input_released(&s_model,now);
        apply(passport_tick(&s_model,now));
        if(remote.controls!=controls){feedback(remote.control_error?TXT_FAILED:TXT_DONE);controls=remote.controls;}
        if(remote.failures!=failures){feedback(TXT_SEND_FAILED);failures=remote.failures;}
        if((int32_t)(now-next_battery)>=0){battery=battery_ok?bsp_battery_soc():-1;next_battery=now+10000;}
        const char *message=!fonts_ok?TXT_FONT_FAILED:!buttons_ok?TXT_INPUT_FAILED:
            (!s_store_ok || passport_store_failed())?TXT_SAVE_FAILED:
            (int32_t)(s_feedback_at-now)>0?s_feedback:"";
        if(bsp_lvgl_lock(100)){
            s_model.photo_count=passport_assets_count();
            if(s_model.photo>=s_model.photo_count)s_model.photo=0;
            if(!s_model.photo_count)s_model.fullscreen=false;
            passport_ui_refresh(&s_model,remote,battery,message);
            bsp_lvgl_unlock();
            /* Render the preserved page before lighting the display on wake. */
            if(display_off!=s_model.asleep){display_off=s_model.asleep;bsp_display_backlight(display_off?0:70);}
        }
        if((int32_t)(now-next_heap)>=0){
            if(bsp_lvgl_lock(100)){
                lv_mem_monitor_t mem;lv_mem_monitor(&mem);
                ESP_LOGI("passport","LVGL free=%u largest=%u peak=%u",
                    (unsigned)mem.free_size,(unsigned)mem.free_biggest_size,(unsigned)mem.max_used);
                bsp_lvgl_unlock();
            }
            ESP_LOGI("passport","Heap free=%u largest=%u minimum=%u, page=%d, display_off=%d",
                (unsigned)esp_get_free_heap_size(),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
                (unsigned)esp_get_minimum_free_heap_size(),s_model.page,s_model.asleep);
            next_heap=now+60000;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
