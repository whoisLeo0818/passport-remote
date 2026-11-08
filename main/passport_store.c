#include "passport_store.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"

static QueueHandle_t s_queue;
static nvs_handle_t s_nvs;
static portMUX_TYPE s_mux=portMUX_INITIALIZER_UNLOCKED;
static bool s_failed;
static void worker(void *arg)
{
    (void)arg;uint32_t value;
    for(;;)if(xQueueReceive(s_queue,&value,portMAX_DELAY)==pdTRUE){
        esp_err_t rc=nvs_set_u32(s_nvs,"settings",value);
        if(rc==ESP_OK)rc=nvs_commit(s_nvs);
        taskENTER_CRITICAL(&s_mux);s_failed=rc!=ESP_OK;taskEXIT_CRITICAL(&s_mux);
        if(rc!=ESP_OK)ESP_LOGE("passport_store","Save: %s",esp_err_to_name(rc));
    }
}
bool passport_store_init(passport_settings_t *settings)
{
    *settings=passport_settings_decode(0);
    if(nvs_open("passport_app",NVS_READWRITE,&s_nvs)!=ESP_OK)return false;
    uint32_t value=0;
    esp_err_t rc=nvs_get_u32(s_nvs,"settings",&value);
    if(rc!=ESP_OK && rc!=ESP_ERR_NVS_NOT_FOUND)ESP_LOGW("passport_store","Using default settings: %s",esp_err_to_name(rc));
    *settings=passport_settings_decode(value);
    s_queue=xQueueCreate(1,sizeof(value));
    if(!s_queue || xTaskCreate(worker,"passport_store",2560,NULL,3,NULL)!=pdPASS){
        if(s_queue){vQueueDelete(s_queue);s_queue=NULL;}nvs_close(s_nvs);return false;
    }
    return true;
}
bool passport_store_save(passport_settings_t settings)
{
    uint32_t value=passport_settings_encode(settings);
    return s_queue && xQueueOverwrite(s_queue,&value)==pdTRUE;
}
bool passport_store_failed(void)
{
    taskENTER_CRITICAL(&s_mux);bool failed=s_failed;taskEXIT_CRITICAL(&s_mux);return failed;
}
