#include "passport_assets.h"
#include "passport_photos.h"
#include "passport_ui.h"
#include "passport_text.h"
#include "bsp_display.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Two banks per logical object. Only one committed NVS word points to the
 * active banks. Interrupted writes never replace a previously valid object. */
enum { PROFILE_SIZE=0x10000, PHOTO_SIZE=0x40000, PHOTO_BASE=0x20000,
       META_MAGIC=0xa7000000, DEFAULT_META=META_MAGIC|63 };
static const esp_partition_t *s_part;
static const uint8_t *s_map;
static esp_partition_mmap_handle_t s_mapping;
static nvs_handle_t s_nvs;
static uint32_t s_meta=DEFAULT_META;
static unsigned s_revision;
static lv_image_dsc_t s_profile[5],s_photos[6][2];
static bool s_ready;
static unsigned offset(bool profile,unsigned index,uint32_t meta,bool inactive)
{
    unsigned bit=profile?18:6+index;
    unsigned bank=((meta>>bit)&1)^(unsigned)inactive;
    return profile?bank*PROFILE_SIZE:PHOTO_BASE+(index*2+bank)*PHOTO_SIZE;
}
static void descriptors(unsigned off,lv_image_dsc_t *images,unsigned count)
{
    passport_transfer_t t;size_t size=64+1024;
    for(unsigned i=0;i<count;i++)size+=(s_map[off+8+4*i]|(s_map[off+9+4*i]<<8))*
        (s_map[off+10+4*i]|(s_map[off+11+4*i]<<8))*2u;
    if(!passport_transfer_parse(s_map+off,size,&t))return;
    unsigned at=off+64+1024;
    for(unsigned i=0;i<count;i++) {
        images[i]=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
            .w=t.width[i],.h=t.height[i],.stride=t.width[i]*2},
            .data_size=t.width[i]*t.height[i]*2u,.data=s_map+at};
        at+=images[i].data_size;
    }
}
static bool valid_record(bool profile,unsigned i)
{
    unsigned off=offset(profile,i,s_meta,false);const uint8_t *h=s_map+off;
    uint32_t len=h[32]|((uint32_t)h[33]<<8)|((uint32_t)h[34]<<16)|((uint32_t)h[35]<<24);
    passport_transfer_t t;
    return len<(profile?PROFILE_SIZE:PHOTO_SIZE)-64 &&
        passport_transfer_parse(h,len+64,&t) && t.kind==(profile?1u:2u) &&
        passport_crc32(0,h+64,len)==t.crc;
}
static void rebuild(void)
{
    memset(s_profile,0,sizeof(s_profile));memset(s_photos,0,sizeof(s_photos));
    if(s_meta&(1u<<19))descriptors(offset(true,0,s_meta,false),s_profile,5);
    for(unsigned i=0;i<6;i++)if(s_meta&(1u<<(12+i)))descriptors(offset(false,i,s_meta,false),s_photos[i],2);
    s_revision++;
}
bool passport_assets_init(void)
{
    s_part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,0x40,"passport_data");
    if(!s_part || s_part->size<PHOTO_BASE+12*PHOTO_SIZE ||
       esp_partition_mmap(s_part,0,s_part->size,ESP_PARTITION_MMAP_DATA,(const void **)&s_map,&s_mapping)!=ESP_OK ||
       nvs_open("passport_files",NVS_READWRITE,&s_nvs)!=ESP_OK)return false;
    uint32_t value;esp_err_t rc=nvs_get_u32(s_nvs,"active",&value);
    if(rc!=ESP_OK && rc!=ESP_ERR_NVS_NOT_FOUND)return false;
    if(rc==ESP_OK) {
        if((value&0xfff00000u)!=META_MAGIC)return false;
        s_meta=value;
    }
    if((s_meta&(1u<<19)) && !valid_record(true,0))s_meta&=~(1u<<19);
    for(unsigned i=0;i<6;i++)if((s_meta&(1u<<(12+i))) && !valid_record(false,i))
        s_meta&=~((1u<<i)|(1u<<(12+i)));
    rebuild();s_ready=true;return true;
}
unsigned passport_assets_revision(void){return s_revision;}
unsigned passport_assets_count(void)
{
    unsigned n=0;for(unsigned i=0;i<6;i++)if(s_meta&(1u<<i))n++;return n;
}
const lv_image_dsc_t *passport_assets_photo(unsigned index,bool full)
{
    for(unsigned i=0;i<6;i++)if(s_meta&(1u<<i)) {
        if(!index--)return s_meta&(1u<<(12+i))?&s_photos[i][full]:passport_photos[i][full];
    }
    return NULL;
}
const lv_image_dsc_t *passport_assets_profile(unsigned section)
{
    return section<5 && (s_meta&(1u<<19))?&s_profile[section]:NULL;
}
static bool commit(uint32_t meta)
{
    /* Invalidate every old UI source before reusing an inactive Flash bank.
     * A following upload can otherwise erase a still-visible old descriptor. */
    if(!bsp_lvgl_lock(1000))return false;
    bool ok=nvs_set_u32(s_nvs,"active",meta)==ESP_OK && nvs_commit(s_nvs)==ESP_OK;
    if(ok){passport_ui_clear_assets();s_meta=meta;rebuild();}
    bsp_lvgl_unlock();return ok;
}
static bool recv_exact(httpd_req_t *req,uint8_t *data,size_t n)
{
    int64_t deadline=esp_timer_get_time()+15000000;
    while(n) {
        int got=httpd_req_recv(req,(char *)data,n);
        if(got==HTTPD_SOCK_ERR_TIMEOUT && esp_timer_get_time()<deadline)continue;
        if(got<=0 || esp_timer_get_time()>deadline)return false;
        data+=got;n-=got;
    }
    return true;
}
static bool profile_json_valid(const uint8_t *data)
{
    if(!memchr(data,0,1024))return false;
    cJSON *j=cJSON_Parse((const char *)data);if(!j)return false;
    const char *keys[]={"name","star","zodiac","tag"};bool ok=true;
    for(unsigned i=0;i<4;i++) {
        cJSON *v=cJSON_GetObjectItemCaseSensitive(j,keys[i]);
        if(!cJSON_IsString(v) || !v->valuestring[0] || strlen(v->valuestring)>(i==3?192u:48u))ok=false;
    }
    cJSON_Delete(j);return ok;
}
esp_err_t passport_assets_upload(httpd_req_t *req,bool profile,unsigned index)
{
    uint8_t header[64];passport_transfer_t t;
    if(!s_ready || index>=6 || req->content_len<1088 || req->content_len>(profile?PROFILE_SIZE:PHOTO_SIZE) ||
       !recv_exact(req,header,64) || !passport_transfer_parse(header,req->content_len,&t) ||
       t.kind!=(profile?1u:2u))return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid transfer");
    /* Clear stale LVGL sources BEFORE any inactive bank is erased. */
    if(!bsp_lvgl_lock(1000))return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Display busy, retry");
    passport_ui_clear_assets();bsp_lvgl_unlock();
    unsigned off=offset(profile,index,s_meta,true),size=profile?PROFILE_SIZE:PHOTO_SIZE;
    if(esp_partition_erase_range(s_part,off,size)!=ESP_OK || esp_partition_write(s_part,off,header,64)!=ESP_OK)
        return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Storage write failed");
    uint8_t buffer[2048];unsigned at=64;uint32_t crc=0;
    while(at<(unsigned)req->content_len) {
        size_t n=(unsigned)req->content_len-at;if(n>sizeof(buffer))n=sizeof(buffer);
        if(!recv_exact(req,buffer,n))return ESP_FAIL;
        if(profile && at==64 && !profile_json_valid(buffer))
            return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Invalid profile");
        crc=passport_crc32(crc,buffer,n);
        if(esp_partition_write(s_part,off+at,buffer,n)!=ESP_OK)
            return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Storage write failed");
        at+=n;
    }
    if(crc!=t.crc || passport_crc32(0,s_map+off+64,t.length)!=t.crc)
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Checksum mismatch");
    uint32_t meta=s_meta^(1u<<(profile?18:6+index));
    meta|=profile?(1u<<19):((1u<<index)|(1u<<(12+index)));
    if(!commit(meta))return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Save failed");
    httpd_resp_set_type(req,"application/json");return httpd_resp_sendstr(req,"{\"ok\":true}");
}
bool passport_assets_delete(unsigned i)
{
    return s_ready && i<6 && commit(s_meta&~((1u<<i)|(1u<<(12+i))));
}
esp_err_t passport_assets_state(httpd_req_t *req,const char *token)
{
    cJSON *j=cJSON_CreateObject();if(!j)return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(j,"token",token);cJSON_AddBoolToObject(j,"storageReady",s_ready);
    cJSON *profile=NULL;
    if(s_meta&(1u<<19))profile=cJSON_Parse((const char *)s_map+offset(true,0,s_meta,false)+64);
    if(!profile) {
        profile=cJSON_CreateObject();cJSON_AddStringToObject(profile,"name",PROFILE_NAME);
        cJSON_AddStringToObject(profile,"star",PROFILE_CONSTELLATION);
        cJSON_AddStringToObject(profile,"zodiac",PROFILE_ZODIAC);
        cJSON_AddStringToObject(profile,"tag",PROFILE_TAG);
    }
    cJSON_AddItemToObject(j,"profile",profile);cJSON_AddBoolToObject(j,"customProfile",(s_meta&(1u<<19))!=0);
    cJSON *photos=cJSON_AddArrayToObject(j,"photos");
    for(unsigned i=0;i<6;i++)if(s_meta&(1u<<i)) {
        cJSON *p=cJSON_CreateObject();cJSON_AddNumberToObject(p,"id",i);
        cJSON_AddBoolToObject(p,"custom",(s_meta&(1u<<(12+i)))!=0);cJSON_AddItemToArray(photos,p);
    }
    char *out=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!out)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req,"application/json");esp_err_t rc=httpd_resp_sendstr(req,out);free(out);return rc;
}
esp_err_t passport_assets_download(httpd_req_t *req,bool profile,unsigned i)
{
    if(profile) {
        if(!(s_meta&(1u<<19)))return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"Default profile");
        unsigned off=offset(true,0,s_meta,false);
        httpd_resp_set_type(req,"application/octet-stream");
        return httpd_resp_send(req,(const char *)s_map+off+1088,118*133*2);
    }
    if(i>=6 || !(s_meta&(1u<<i)))return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"No photo");
    const lv_image_dsc_t *d=s_meta&(1u<<(12+i))?&s_photos[i][0]:passport_photos[i][0];
    uint8_t head[4]={d->header.w&255,d->header.w>>8,d->header.h&255,d->header.h>>8};
    httpd_resp_set_type(req,"application/octet-stream");
    esp_err_t rc=httpd_resp_send_chunk(req,(char *)head,4);
    if(rc==ESP_OK)rc=httpd_resp_send_chunk(req,(const char *)d->data,d->data_size);
    if(rc==ESP_OK)rc=httpd_resp_send_chunk(req,NULL,0);
    return rc;
}
