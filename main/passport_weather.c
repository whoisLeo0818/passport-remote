#include "passport_weather.h"
#include "passport_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_netif_sntp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static portMUX_TYPE s_mux=portMUX_INITIALIZER_UNLOCKED;
static passport_weather_status_t s_status={.config={0,30}};
static bool s_refresh;
static TaskHandle_t s_task;
static nvs_handle_t s_nvs;
static uint32_t ms(void){return (uint32_t)(esp_timer_get_time()/1000);}
passport_weather_status_t passport_weather_status(void)
{
    taskENTER_CRITICAL(&s_mux);passport_weather_status_t v=s_status;taskEXIT_CRITICAL(&s_mux);return v;
}
static void error(weather_error_t e,bool updating)
{
    taskENTER_CRITICAL(&s_mux);s_status.error=e;s_status.updating=updating;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
}
static const char *cache_key(unsigned city){return city?"wh":"sz";}
static bool cached(nvs_handle_t h,unsigned city,weather_sample_t *out)
{
    size_t size=sizeof(*out);
    return nvs_get_blob(h,cache_key(city),out,&size)==ESP_OK&&size==sizeof(*out)&&
        weather_sample_valid(out)&&out->city==city;
}
void passport_weather_refresh(void)
{
    taskENTER_CRITICAL(&s_mux);s_refresh=true;taskEXIT_CRITICAL(&s_mux);
    if(s_task)xTaskNotifyGive(s_task);
}
static bool synchronize_clock(void)
{
    esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com");
    if(esp_netif_sntp_init(&config)!=ESP_OK)return false;
    bool ok=esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000))==ESP_OK;
    esp_netif_sntp_deinit();
    int64_t epoch=time(NULL);ok=ok&&weather_epoch_valid(epoch);
    if(ok) {
        bool saved=nvs_set_i64(s_nvs,"clock",epoch)==ESP_OK&&nvs_commit(s_nvs)==ESP_OK;
        taskENTER_CRITICAL(&s_mux);s_status.clock_synced=true;s_status.clock_cached=true;
        s_status.store_failed|=!saved;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
    }
    return ok;
}
static bool number(cJSON *obj,const char *key,double *out)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,key);
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;
    *out=v->valuedouble;return true;
}
static bool parse(const char *data,unsigned city,weather_sample_t *sample)
{
    cJSON *root=cJSON_Parse(data);if(!root)return false;
    cJSON *current=cJSON_GetObjectItemCaseSensitive(root,"current");
    double at,t,f,h,w,c;bool ok=cJSON_IsObject(current)&&number(current,"time",&at)&&
        number(current,"temperature_2m",&t)&&number(current,"apparent_temperature",&f)&&
        number(current,"relative_humidity_2m",&h)&&number(current,"wind_speed_10m",&w)&&
        number(current,"weather_code",&c);
    if(ok) {
        ok=at>=1704067200.0&&at<4102444800.0&&floor(at)==at&&t>=-90&&t<=60&&
            f>=-100&&f<=90&&h>=0&&h<=100&&w>=0&&w<=150&&c>=0&&c<=99&&floor(c)==c;
        if(ok) {
            *sample=(weather_sample_t){.version=1,.city=city,.observed_at=(int64_t)at,
                .received_at=time(NULL),.temperature=(int16_t)lround(t*10),.feels_like=(int16_t)lround(f*10),
                .wind=(int16_t)lround(w*10),.humidity=(uint16_t)lround(h),.code=(uint16_t)c};
            ok=weather_sample_valid(sample);
        }
    }
    cJSON_Delete(root);return ok;
}
static weather_error_t fetch(unsigned city,weather_sample_t *sample)
{
    /* Serialize TLS with uploads/scans so their peak allocations cannot stack.
     * A busy device retries later rather than blocking UI or clearing cache. */
    if(!passport_network_io_lock(0))return WEATHER_MEMORY;
    weather_error_t result=WEATHER_NETWORK;
    size_t largest=heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    /* Allow space for record framing and cipher overhead beyond the payload. */
    if(largest<CONFIG_MBEDTLS_SSL_IN_CONTENT_LEN+1024u) {
        ESP_LOGW("passport_wx","TLS deferred: largest heap block=%u",(unsigned)largest);
        passport_network_io_unlock();
        return WEATHER_MEMORY;
    }
    char url[320];
    snprintf(url,sizeof(url),"https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m&wind_speed_unit=ms&timeformat=unixtime&forecast_days=1",
        city?"30.5931":"22.5431",city?"114.3054":"114.0579");
    esp_http_client_config_t cfg={.url=url,.crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=7000,
        .buffer_size=1024,.buffer_size_tx=512,.disable_auto_redirect=true};
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    char *body=NULL;size_t used=0;
    if(!client)result=WEATHER_MEMORY;
    else if(esp_http_client_open(client,0)==ESP_OK) {
        int64_t deadline=esp_timer_get_time()+15000000;
        int64_t length=esp_http_client_fetch_headers(client);
        int status=esp_http_client_get_status_code(client);
        if(status==200&&length<4096) {
            size_t capacity=length>0?(size_t)length+1:4096;
            body=malloc(capacity);
            if(!body){result=WEATHER_MEMORY;goto done;}
            bool complete=false;
            while(used<capacity-1&&esp_timer_get_time()<deadline) {
                int n=esp_http_client_read(client,body+used,capacity-1-used);
                if(n<0)break;
                if(!n){complete=esp_http_client_is_complete_data_received(client);break;}
                used+=n;
                if(esp_http_client_is_complete_data_received(client)){complete=true;break;}
            }
            body[used]=0;
            esp_http_client_cleanup(client);
            client=NULL;
            result=complete&&parse(body,city,sample)?WEATHER_OK:WEATHER_DATA;
        }
    }
done:
    if(client){esp_http_client_close(client);esp_http_client_cleanup(client);}free(body);
    passport_network_io_unlock();
    passport_wifi_internet_result(result==WEATHER_OK);
    return result;
}
static void worker(void *arg)
{
    (void)arg;uint32_t next=0,clock_retry=0,last_start=0,last_config=0;unsigned failures=0;bool attempted=false,was_linked=false;
    for(;;) {
        passport_wifi_status_t wifi=passport_wifi_status();
        passport_weather_status_t current=passport_weather_status();uint32_t now=ms();
        uint32_t config=weather_config_encode(current.config);
        if(config!=last_config){last_config=config;next=now;}
        if(wifi.linked&&!was_linked){next=now;clock_retry=now;}
        was_linked=wifi.linked;
        taskENTER_CRITICAL(&s_mux);bool refresh=s_refresh;s_refresh=false;taskEXIT_CRITICAL(&s_mux);
        if(refresh)next=now;
        if(wifi.linked&&!current.clock_synced&&weather_due(now,clock_retry)) {
            if(!synchronize_clock())error(WEATHER_CLOCK,false);
            clock_retry=ms()+120000;current=passport_weather_status();
        }
        if(wifi.linked&&current.clock_synced&&weather_due(ms(),next)&&
           (!attempted||weather_due(ms(),last_start+60000))) {
            last_start=ms();attempted=true;error(WEATHER_OK,true);
            weather_sample_t sample={0};weather_error_t rc=fetch(current.config.city,&sample);
            if(rc==WEATHER_OK) {
                bool saved=nvs_set_blob(s_nvs,cache_key(sample.city),&sample,sizeof(sample))==ESP_OK&&nvs_commit(s_nvs)==ESP_OK;
                taskENTER_CRITICAL(&s_mux);
                if(s_status.config.city==sample.city){s_status.sample=sample;s_status.valid=true;}
                s_status.store_failed|=!saved;s_status.updating=false;s_status.error=saved?WEATHER_OK:WEATHER_STORAGE;s_status.revision++;
                unsigned minutes=s_status.config.minutes;taskEXIT_CRITICAL(&s_mux);
                failures=0;next=ms()+minutes*60000u;
            }else {error(rc,false);next=ms()+weather_retry_ms(++failures);}
        }
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(1000));
    }
}
bool passport_weather_init(void)
{
    setenv("TZ","CST-8",1);tzset();
    if(nvs_open("passport_wx",NVS_READWRITE,&s_nvs)!=ESP_OK){s_status.store_failed=true;return false;}
    uint32_t config=0;nvs_get_u32(s_nvs,"config",&config);s_status.config=weather_config_decode(config);
    s_status.valid=cached(s_nvs,s_status.config.city,&s_status.sample);
    int64_t epoch=0;
    if(nvs_get_i64(s_nvs,"clock",&epoch)==ESP_OK&&weather_epoch_valid(epoch)) {
        struct timeval time={.tv_sec=epoch};settimeofday(&time,NULL);s_status.clock_cached=true;
    }
    return xTaskCreate(worker,"passport_wx",6144,NULL,2,&s_task)==pdPASS;
}
static esp_err_t state_json(httpd_req_t *req)
{
    passport_weather_status_t s=passport_weather_status();cJSON *j=cJSON_CreateObject();if(!j)return ESP_ERR_NO_MEM;
    cJSON_AddNumberToObject(j,"city",s.config.city);cJSON_AddStringToObject(j,"cityName",weather_city(s.config.city));
    cJSON_AddNumberToObject(j,"minutes",s.config.minutes);cJSON_AddBoolToObject(j,"valid",s.valid);
    cJSON_AddBoolToObject(j,"updating",s.updating);cJSON_AddBoolToObject(j,"clockSynced",s.clock_synced);
    cJSON_AddBoolToObject(j,"clockCached",s.clock_cached);cJSON_AddBoolToObject(j,"storeFailed",s.store_failed);
    cJSON_AddNumberToObject(j,"error",s.error);cJSON_AddNumberToObject(j,"epoch",time(NULL));
    if(s.valid) {
        cJSON_AddStringToObject(j,"condition",weather_condition(s.sample.code));
        cJSON_AddNumberToObject(j,"temperature",s.sample.temperature);cJSON_AddNumberToObject(j,"feelsLike",s.sample.feels_like);
        cJSON_AddNumberToObject(j,"humidity",s.sample.humidity);cJSON_AddNumberToObject(j,"wind",s.sample.wind);
        cJSON_AddNumberToObject(j,"observedAt",(double)s.sample.observed_at);
    }
    cJSON_AddStringToObject(j,"source","Open-Meteo (CC BY 4.0)");
    char *text=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!text)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req,"application/json");esp_err_t rc=httpd_resp_sendstr(req,text);free(text);return rc;
}
esp_err_t passport_weather_api(httpd_req_t *req)
{
    if(req->method==HTTP_GET)return state_json(req);
    if(!s_task)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Weather service unavailable");
    if(!strcmp(req->uri,"/api/weather/refresh")){passport_weather_refresh();return httpd_resp_sendstr(req,"{\"ok\":true}");}
    char body[161];size_t used=0;
    if(!req->content_len||req->content_len>=sizeof(body))return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid configuration");
    while(used<req->content_len){int n=httpd_req_recv(req,body+used,req->content_len-used);if(n<=0)return ESP_FAIL;used+=n;}body[used]=0;
    cJSON *j=cJSON_Parse(body);double city,minutes;
    bool ok=j&&number(j,"city",&city)&&number(j,"minutes",&minutes)&&
        (city==0||city==1)&&minutes>=0&&minutes<=240&&floor(minutes)==minutes&&weather_interval_valid((unsigned)minutes);
    cJSON_Delete(j);if(!ok)return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid city or interval");
    weather_config_t config={(unsigned)city,(unsigned)minutes};nvs_handle_t h;
    if(nvs_open("passport_wx",NVS_READWRITE,&h)!=ESP_OK)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Save failed");
    ok=nvs_set_u32(h,"config",weather_config_encode(config))==ESP_OK&&nvs_commit(h)==ESP_OK;
    weather_sample_t sample={0};bool valid=ok&&cached(h,config.city,&sample);nvs_close(h);
    if(!ok)return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Save failed");
    taskENTER_CRITICAL(&s_mux);s_status.config=config;s_status.valid=valid;s_status.sample=sample;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
    passport_weather_refresh();return state_json(req);
}
