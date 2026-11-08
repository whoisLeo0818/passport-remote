#include "passport_wifi.h"
#include "passport_assets.h"
#include "passport_weather.h"
#include "passport_wifi_secret.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uint8_t passport_web_start[] asm("_binary_passport_web_html_start");
extern const uint8_t passport_web_end[] asm("_binary_passport_web_html_end");
typedef struct {uint32_t version;char ssid[33],password[64];uint8_t enabled;} station_config_t;
typedef struct {char ssid[33];int rssi;bool secure;} network_t;
static TaskHandle_t s_worker;
static SemaphoreHandle_t s_io;
static portMUX_TYPE s_mux=portMUX_INITIALIZER_UNLOCKED;
static passport_wifi_status_t s_status;
static httpd_handle_t s_server;
static esp_netif_t *s_netif,*s_station;
static bool s_initialized,s_started,s_dirty,s_scan_requested,s_reconfigure;
static station_config_t s_desired;
static nvs_handle_t s_nvs;
static network_t s_networks[8];
static unsigned s_network_count;
static char s_token[33];
passport_wifi_status_t passport_wifi_status(void)
{
    taskENTER_CRITICAL(&s_mux);passport_wifi_status_t status=s_status;taskEXIT_CRITICAL(&s_mux);return status;
}
const char *passport_wifi_password(void){return PASSPORT_WIFI_PASSWORD;}
bool passport_network_io_lock(unsigned wait_ms){return s_io&&xSemaphoreTake(s_io,pdMS_TO_TICKS(wait_ms))==pdTRUE;}
void passport_network_io_unlock(void){if(s_io)xSemaphoreGive(s_io);}
void passport_wifi_internet_result(bool success)
{
    taskENTER_CRITICAL(&s_mux);s_status.internet=success&&s_status.linked;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    (void)arg;
    taskENTER_CRITICAL(&s_mux);
    if(base==WIFI_EVENT) {
        if(id==WIFI_EVENT_AP_STACONNECTED)s_status.clients++;
        if(id==WIFI_EVENT_AP_STADISCONNECTED&&s_status.clients)s_status.clients--;
        if(id==WIFI_EVENT_STA_DISCONNECTED) {
            s_status.linked=s_status.internet=s_status.connecting=false;s_status.ip[0]=0;
            s_status.revision++;
        }
    }else if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event=data;
        s_status.linked=s_status.wifi_enabled;s_status.connecting=false;s_status.failed=false;
        snprintf(s_status.ip,sizeof(s_status.ip),IPSTR,IP2STR(&event->ip_info.ip));s_status.revision++;
    }
    taskEXIT_CRITICAL(&s_mux);
}
static esp_err_t network_api(httpd_req_t *r);
static bool authorized(httpd_req_t *r)
{
    /* With STA enabled, Host alone must not expose the manager on the home
     * LAN. Accept sockets addressed to the fixed Hotspot interface only. */
    struct sockaddr_storage local;socklen_t size=sizeof(local);uint32_t address=0;
    if(getsockname(httpd_req_to_sockfd(r),(struct sockaddr *)&local,&size))return false;
    if(local.ss_family==AF_INET)address=((struct sockaddr_in *)&local)->sin_addr.s_addr;
#if CONFIG_LWIP_IPV6
    else if(local.ss_family==AF_INET6) {
        struct in6_addr *ip=&((struct sockaddr_in6 *)&local)->sin6_addr;
        if(IN6_IS_ADDR_V4MAPPED(ip))memcpy(&address,&ip->s6_addr[12],sizeof(address));
    }
#endif
    if(address!=inet_addr(PASSPORT_WIFI_ADDRESS))return false;
    char token[40]={0},host[40]={0},origin[64]={0};
    if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK ||
       (strcmp(host,"192.168.4.1") && strcmp(host,"192.168.4.1:80")))return false;
    if(httpd_req_get_hdr_value_len(r,"Origin") &&
       (httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK ||
        (strcmp(origin,"http://192.168.4.1") && strcmp(origin,"http://192.168.4.1:80"))))return false;
    return r->method==HTTP_GET ||
        (httpd_req_get_hdr_value_str(r,"X-Passport-Token",token,sizeof(token))==ESP_OK && !strcmp(token,s_token));
}
static esp_err_t route(httpd_req_t *r)
{
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    if(!authorized(r))return httpd_resp_send_err(r,HTTPD_403_FORBIDDEN,"Local access only");
    if(!strcmp(r->uri,"/") && r->method==HTTP_GET) {
        httpd_resp_set_type(r,"text/html; charset=utf-8");
        httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; img-src 'self' blob: data:; connect-src 'self'; frame-ancestors 'none'");
        return httpd_resp_send(r,(const char *)passport_web_start,passport_web_end-passport_web_start);
    }
    if(!strcmp(r->uri,"/api/state") && r->method==HTTP_GET)return passport_assets_state(r,s_token);
    if(!strncmp(r->uri,"/api/network",12))return network_api(r);
    if((!strcmp(r->uri,"/api/weather")||!strcmp(r->uri,"/api/weather/refresh"))&&
       (r->method==HTTP_GET||r->method==HTTP_POST))return passport_weather_api(r);
    if(!strcmp(r->uri,"/api/profile")) {
        if(r->method==HTTP_GET)return passport_assets_download(r,true,0);
        if(r->method==HTTP_POST) {
            if(!passport_network_io_lock(0))return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Network busy, retry");
            esp_err_t rc=passport_assets_upload(r,true,0);passport_network_io_unlock();return rc;
        }
    }
    const char *prefix="/api/photo/";size_t n=strlen(prefix);
    if(!strncmp(r->uri,prefix,n) && r->uri[n]>='0' && r->uri[n]<='5' && !r->uri[n+1]) {
        unsigned i=r->uri[n]-'0';
        if(r->method==HTTP_GET)return passport_assets_download(r,false,i);
        if(r->method==HTTP_POST) {
            if(!passport_network_io_lock(0))return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Network busy, retry");
            esp_err_t rc=passport_assets_upload(r,false,i);passport_network_io_unlock();return rc;
        }
        if(r->method==HTTP_DELETE) {
            if(!passport_assets_delete(i))return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Save failed");
            httpd_resp_set_type(r,"application/json");return httpd_resp_sendstr(r,"{\"ok\":true}");
        }
    }
    return httpd_resp_send_err(r,HTTPD_404_NOT_FOUND,"Not found");
}
static void stop(void)
{
    if(s_server){httpd_stop(s_server);s_server=NULL;}
    if(s_started){esp_wifi_stop();s_started=false;}
    if(s_initialized){esp_wifi_deinit();s_initialized=false;}
    if(s_netif){esp_netif_destroy_default_wifi(s_netif);s_netif=NULL;}
    if(s_station){esp_netif_destroy_default_wifi(s_station);s_station=NULL;}
    taskENTER_CRITICAL(&s_mux);s_status.clients=0;s_status.enabled=false;
    s_status.linked=s_status.internet=s_status.connecting=false;s_status.ip[0]=0;s_status.revision++;
    taskEXIT_CRITICAL(&s_mux);
}
static esp_err_t prepare(void)
{
    if(s_initialized)return ESP_OK;
    s_netif=esp_netif_create_default_wifi_ap();if(!s_netif)return ESP_ERR_NO_MEM;
    s_station=esp_netif_create_default_wifi_sta();if(!s_station)return ESP_ERR_NO_MEM;
    esp_netif_ip_info_t ip={0};
    esp_netif_str_to_ip4("192.168.4.1",&ip.ip);ip.gw=ip.ip;
    esp_netif_str_to_ip4("255.255.255.0",&ip.netmask);
    esp_netif_dhcps_stop(s_netif);
    esp_err_t rc=esp_netif_set_ip_info(s_netif,&ip);if(rc!=ESP_OK)return rc;
    rc=esp_netif_dhcps_start(s_netif);if(rc!=ESP_OK)return rc;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    rc=esp_wifi_init(&init);if(rc!=ESP_OK)return rc;s_initialized=true;
    esp_netif_set_default_netif(s_station);
    return esp_wifi_set_storage(WIFI_STORAGE_RAM);
}
static esp_err_t start_server(void)
{
    uint8_t random[16];esp_fill_random(random,sizeof(random));
    for(unsigned i=0;i<16;i++)snprintf(s_token+i*2,3,"%02x",random[i]);
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.stack_size=6144;
    config.max_open_sockets=4;config.lru_purge_enable=true;config.recv_wait_timeout=3;
    config.send_wait_timeout=5;config.uri_match_fn=httpd_uri_match_wildcard;
    esp_err_t rc=httpd_start(&s_server,&config);if(rc!=ESP_OK)return rc;
    const httpd_method_t methods[]={HTTP_GET,HTTP_POST,HTTP_DELETE};
    for(unsigned i=0;i<3;i++) {
        httpd_uri_t uri={.uri="/*",.method=methods[i],.handler=route};
        rc=httpd_register_uri_handler(s_server,&uri);if(rc!=ESP_OK)return rc;
    }
    return ESP_OK;
}
static esp_err_t configure(bool ap,bool station,const station_config_t *desired,bool reconnect)
{
    if(!ap&&!station){stop();return ESP_OK;}
    esp_err_t rc=prepare();if(rc!=ESP_OK){stop();return rc;}
    if(!ap&&s_server){httpd_stop(s_server);s_server=NULL;}
    wifi_mode_t mode=ap?(station?WIFI_MODE_APSTA:WIFI_MODE_AP):WIFI_MODE_STA;
    if((rc=esp_wifi_set_mode(mode))!=ESP_OK)return rc;
    if(ap&&!passport_wifi_status().enabled) {
        wifi_config_t cfg={.ap={.channel=6,.max_connection=2,.authmode=WIFI_AUTH_WPA2_PSK,
            .pmf_cfg={.capable=true,.required=false}}};
        memcpy(cfg.ap.ssid,PASSPORT_WIFI_SSID,strlen(PASSPORT_WIFI_SSID));cfg.ap.ssid_len=strlen(PASSPORT_WIFI_SSID);
        memcpy(cfg.ap.password,PASSPORT_WIFI_PASSWORD,strlen(PASSPORT_WIFI_PASSWORD));
        if((rc=esp_wifi_set_config(WIFI_IF_AP,&cfg))!=ESP_OK)return rc;
    }
    if(station&&(reconnect||!s_started)) {
        if(s_started)esp_wifi_disconnect();
        wifi_config_t cfg={0};memcpy(cfg.sta.ssid,desired->ssid,strlen(desired->ssid));
        memcpy(cfg.sta.password,desired->password,strlen(desired->password));
        cfg.sta.threshold.authmode=desired->password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
        cfg.sta.pmf_cfg.capable=true;
        if((rc=esp_wifi_set_config(WIFI_IF_STA,&cfg))!=ESP_OK)return rc;
        taskENTER_CRITICAL(&s_mux);s_status.linked=s_status.internet=s_status.connecting=false;s_status.ip[0]=0;taskEXIT_CRITICAL(&s_mux);
    }
    if(!s_started){if((rc=esp_wifi_start())!=ESP_OK)return rc;s_started=true;}
    if(ap&&!s_server&&(rc=start_server())!=ESP_OK)return rc;
    taskENTER_CRITICAL(&s_mux);s_status.enabled=ap;
    if(!ap)s_status.clients=0;
    if(!station){s_status.linked=s_status.internet=s_status.connecting=false;s_status.ip[0]=0;}
    s_status.revision++;taskEXIT_CRITICAL(&s_mux);return ESP_OK;
}
static void scan(bool ap,bool station)
{
    if(!passport_network_io_lock(0)) {
        taskENTER_CRITICAL(&s_mux);s_scan_requested=true;taskEXIT_CRITICAL(&s_mux);return;
    }
    taskENTER_CRITICAL(&s_mux);s_status.scan_busy=true;taskEXIT_CRITICAL(&s_mux);
    /* A temporarily enabled STA interface allows scanning with the external
     * switch off. It never initiates a connection without the saved intent. */
    esp_err_t rc=esp_wifi_set_mode(ap?WIFI_MODE_APSTA:WIFI_MODE_STA);
    wifi_scan_config_t cfg={.show_hidden=false,.scan_type=WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active={.min=60,.max=120}};
    if(rc==ESP_OK)rc=esp_wifi_scan_start(&cfg,true);
    uint16_t count=8;wifi_ap_record_t *records=calloc(count,sizeof(*records));
    if(!records)rc=ESP_ERR_NO_MEM;
    if(rc==ESP_OK)rc=esp_wifi_scan_get_ap_records(&count,records);
    if(rc!=ESP_OK)esp_wifi_clear_ap_list();
    taskENTER_CRITICAL(&s_mux);
    if(rc==ESP_OK) {
        s_network_count=count;
        for(unsigned i=0;i<count;i++) {
            memcpy(s_networks[i].ssid,records[i].ssid,32);s_networks[i].ssid[32]=0;
            s_networks[i].rssi=records[i].rssi;s_networks[i].secure=records[i].authmode!=WIFI_AUTH_OPEN;
        }
        s_status.scan_valid=true;
    }
    s_status.scan_busy=false;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
    free(records);
    if(ap&&!station)esp_wifi_set_mode(WIFI_MODE_AP);
    passport_network_io_unlock();
}
static void worker(void *arg)
{
    (void)arg;uint32_t retry_at=0,connect_at=0;unsigned attempts=0;
    for(;;) {
        station_config_t desired;bool ap,dirty,reconnect,scanning;
        taskENTER_CRITICAL(&s_mux);desired=s_desired;ap=s_status.hotspot_requested;
        dirty=s_dirty;s_dirty=false;reconnect=s_reconfigure;s_reconfigure=false;
        scanning=s_scan_requested;s_scan_requested=false;taskEXIT_CRITICAL(&s_mux);
        if(dirty) {
            taskENTER_CRITICAL(&s_mux);s_status.busy=true;s_status.failed=false;taskEXIT_CRITICAL(&s_mux);
            bool saved=nvs_set_blob(s_nvs,"station",&desired,sizeof(desired))==ESP_OK&&nvs_commit(s_nvs)==ESP_OK;
            esp_err_t rc=configure(ap,desired.enabled,&desired,reconnect);
            if(rc!=ESP_OK)stop();
            taskENTER_CRITICAL(&s_mux);s_status.busy=false;s_status.failed=rc!=ESP_OK;
            s_status.store_failed=!saved;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
            if(reconnect){retry_at=0;attempts=0;}
        }
        passport_wifi_status_t status=passport_wifi_status();uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
        if(scanning&&s_started)scan(ap,desired.enabled);
        if(s_started&&desired.enabled&&desired.ssid[0]&&!status.linked) {
            if(status.connecting&&(int32_t)(now-connect_at)>=20000) {
                esp_wifi_disconnect();
                taskENTER_CRITICAL(&s_mux);s_status.connecting=false;s_status.failed=true;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
            }else if(!status.connecting&&(int32_t)(now-retry_at)>=0) {
                esp_err_t rc=esp_wifi_connect();attempts++;connect_at=now;
                retry_at=now+(attempts<3?10000:60000);
                taskENTER_CRITICAL(&s_mux);s_status.connecting=rc==ESP_OK;s_status.failed=rc!=ESP_OK||attempts>1;s_status.revision++;taskEXIT_CRITICAL(&s_mux);
            }
        }else if(status.linked)attempts=0;
        ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(200));
    }
}
esp_err_t passport_wifi_init(void)
{
    esp_err_t rc=esp_netif_init();if(rc!=ESP_OK)return rc;
    rc=esp_event_loop_create_default();if(rc!=ESP_OK && rc!=ESP_ERR_INVALID_STATE)return rc;
    rc=esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL);if(rc!=ESP_OK)return rc;
    rc=esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL);if(rc!=ESP_OK)return rc;
    if(nvs_open("passport_net",NVS_READWRITE,&s_nvs)!=ESP_OK)return ESP_FAIL;
    size_t size=sizeof(s_desired);
    if(nvs_get_blob(s_nvs,"station",&s_desired,&size)!=ESP_OK||size!=sizeof(s_desired)||
       s_desired.version!=1||!memchr(s_desired.ssid,0,sizeof(s_desired.ssid))||
       !memchr(s_desired.password,0,sizeof(s_desired.password))||s_desired.enabled>1)memset(&s_desired,0,sizeof(s_desired));
    s_desired.version=1;s_status.wifi_enabled=s_desired.enabled;
    memcpy(s_status.ssid,s_desired.ssid,sizeof(s_status.ssid));s_dirty=s_desired.enabled;
    s_io=xSemaphoreCreateMutex();if(!s_io)return ESP_ERR_NO_MEM;
    if(xTaskCreate(worker,"passport_wifi",6144,NULL,3,&s_worker)!=pdPASS)return ESP_ERR_NO_MEM;
    return ESP_OK;
}
bool passport_wifi_enable(bool enable)
{
    if(!s_worker)return false;
    taskENTER_CRITICAL(&s_mux);s_desired.enabled=enable;s_status.wifi_enabled=enable;
    s_dirty=s_reconfigure=true;s_status.revision++;taskEXIT_CRITICAL(&s_mux);xTaskNotifyGive(s_worker);return true;
}
bool passport_hotspot_enable(bool enable)
{
    if(!s_worker)return false;
    taskENTER_CRITICAL(&s_mux);s_status.hotspot_requested=enable;s_dirty=true;s_status.revision++;
    taskEXIT_CRITICAL(&s_mux);xTaskNotifyGive(s_worker);return true;
}
static esp_err_t network_state(httpd_req_t *r)
{
    passport_wifi_status_t s=passport_wifi_status();network_t networks[8];unsigned count;
    taskENTER_CRITICAL(&s_mux);count=s_network_count;memcpy(networks,s_networks,sizeof(networks));taskEXIT_CRITICAL(&s_mux);
    cJSON *j=cJSON_CreateObject();if(!j)return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(j,"enabled",s.wifi_enabled);cJSON_AddBoolToObject(j,"linked",s.linked);
    cJSON_AddBoolToObject(j,"internet",s.internet);cJSON_AddBoolToObject(j,"connecting",s.connecting);
    cJSON_AddBoolToObject(j,"failed",s.failed);cJSON_AddBoolToObject(j,"storeFailed",s.store_failed);
    cJSON_AddBoolToObject(j,"scanBusy",s.scan_busy);cJSON_AddBoolToObject(j,"scanValid",s.scan_valid);
    cJSON_AddStringToObject(j,"ssid",s.ssid);cJSON_AddStringToObject(j,"ip",s.ip);
    cJSON *list=cJSON_AddArrayToObject(j,"networks");
    for(unsigned i=0;i<count;i++) {
        cJSON *n=cJSON_CreateObject();cJSON_AddStringToObject(n,"ssid",networks[i].ssid);
        cJSON_AddNumberToObject(n,"rssi",networks[i].rssi);cJSON_AddBoolToObject(n,"secure",networks[i].secure);cJSON_AddItemToArray(list,n);
    }
    char *text=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!text)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r,"application/json");esp_err_t rc=httpd_resp_sendstr(r,text);free(text);return rc;
}
static esp_err_t network_api(httpd_req_t *r)
{
    if(!strcmp(r->uri,"/api/network")&&r->method==HTTP_GET)return network_state(r);
    if(!s_worker)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Network unavailable");
    if(!strcmp(r->uri,"/api/network/scan")&&r->method==HTTP_POST) {
        taskENTER_CRITICAL(&s_mux);s_scan_requested=true;s_status.scan_valid=false;taskEXIT_CRITICAL(&s_mux);
        xTaskNotifyGive(s_worker);return httpd_resp_sendstr(r,"{\"ok\":true}");
    }
    if(strcmp(r->uri,"/api/network")||r->method!=HTTP_POST)return httpd_resp_send_err(r,HTTPD_404_NOT_FOUND,"Not found");
    char body[385];size_t used=0;
    if(!r->content_len||r->content_len>=sizeof(body))return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid configuration");
    while(used<r->content_len){int n=httpd_req_recv(r,body+used,r->content_len-used);if(n<=0)return ESP_FAIL;used+=n;}body[used]=0;
    cJSON *j=cJSON_Parse(body),*enabled=cJSON_GetObjectItemCaseSensitive(j,"enabled"),
        *ssid=cJSON_GetObjectItemCaseSensitive(j,"ssid"),*password=cJSON_GetObjectItemCaseSensitive(j,"password");
    bool credentials=ssid||password;
    bool valid=cJSON_IsBool(enabled)&&(!credentials||(cJSON_IsString(ssid)&&cJSON_IsString(password)&&
        strlen(ssid->valuestring)>0&&strlen(ssid->valuestring)<=32&&strlen(password->valuestring)<=63&&
        (!password->valuestring[0]||strlen(password->valuestring)>=8)));
    if(!valid){cJSON_Delete(j);return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid SSID or password");}
    taskENTER_CRITICAL(&s_mux);
    if(credentials){snprintf(s_desired.ssid,sizeof(s_desired.ssid),"%s",ssid->valuestring);
        snprintf(s_desired.password,sizeof(s_desired.password),"%s",password->valuestring);
        memcpy(s_status.ssid,s_desired.ssid,sizeof(s_status.ssid));}
    s_desired.enabled=cJSON_IsTrue(enabled);s_status.wifi_enabled=s_desired.enabled;
    s_dirty=s_reconfigure=true;s_status.revision++;taskEXIT_CRITICAL(&s_mux);cJSON_Delete(j);
    memset(body,0,sizeof(body));xTaskNotifyGive(s_worker);return network_state(r);
}
