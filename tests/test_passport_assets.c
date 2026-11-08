#include "passport_assets.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/passport_assets.c"

static uint8_t flash[0x400000];
static uint32_t committed,pending;
static bool commit_failed,write_failed,locked;
static const esp_partition_t part={sizeof(flash)};
static const uint8_t sample_data[]={0,0};
static const lv_image_dsc_t sample={.header={.w=1,.h=1},.data_size=2,.data=sample_data};
const lv_image_dsc_t *const passport_photos[6][2]={
    {&sample,&sample},{&sample,&sample},{&sample,&sample},
    {&sample,&sample},{&sample,&sample},{&sample,&sample}};
const esp_partition_t *esp_partition_find_first(int type,int sub,const char *label)
{assert(type==1 && sub==0x40 && !strcmp(label,"passport_data"));return &part;}
esp_err_t esp_partition_mmap(const esp_partition_t *p,size_t off,size_t len,int type,const void **ptr,esp_partition_mmap_handle_t *handle)
{(void)p;(void)type;assert(off+len<=sizeof(flash));*ptr=flash+off;*handle=1;return ESP_OK;}
esp_err_t esp_partition_erase_range(const esp_partition_t *p,size_t off,size_t n)
{(void)p;assert(!locked && off+n<=sizeof(flash));memset(flash+off,255,n);return ESP_OK;}
esp_err_t esp_partition_write(const esp_partition_t *p,size_t off,const void *data,size_t n)
{(void)p;assert(!locked && off+n<=sizeof(flash));if(write_failed)return ESP_FAIL;memcpy(flash+off,data,n);return ESP_OK;}
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h){(void)name;(void)mode;*h=1;return ESP_OK;}
esp_err_t nvs_get_u32(nvs_handle_t h,const char *k,uint32_t *v){(void)h;(void)k;if(!committed)return ESP_ERR_NVS_NOT_FOUND;*v=committed;return ESP_OK;}
esp_err_t nvs_set_u32(nvs_handle_t h,const char *k,uint32_t v){(void)h;(void)k;pending=v;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;if(commit_failed)return ESP_FAIL;committed=pending;return ESP_OK;}
bool bsp_lvgl_lock(unsigned wait){(void)wait;assert(!locked);locked=true;return true;}
void bsp_lvgl_unlock(void){assert(locked);locked=false;}
void passport_ui_clear_assets(void){assert(locked);}
int64_t esp_timer_get_time(void){return 1000;}
int httpd_req_recv(httpd_req_t *r,char *p,size_t n)
{
    if(r->timeouts){r->timeouts--;return HTTPD_SOCK_ERR_TIMEOUT;}
    if(r->fail_at && r->at>=r->fail_at)return -1;
    if(n>17)n=17;
    if(n>r->content_len-r->at)n=r->content_len-r->at;
    memcpy(p,r->body+r->at,n);r->at+=n;return (int)n;
}
esp_err_t httpd_resp_send_err(httpd_req_t *r,int status,const char *s){(void)s;r->status=status;return ESP_OK;}
esp_err_t httpd_resp_set_type(httpd_req_t *r,const char *s){(void)r;(void)s;return ESP_OK;}
esp_err_t httpd_resp_sendstr(httpd_req_t *r,const char *s){(void)s;r->status=200;return ESP_OK;}
esp_err_t httpd_resp_send(httpd_req_t *r,const char *s,size_t n){(void)r;(void)s;(void)n;return ESP_OK;}
esp_err_t httpd_resp_send_chunk(httpd_req_t *r,const char *s,size_t n){(void)r;(void)s;(void)n;return ESP_OK;}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(v>>(8*i))&255;}
int main(void)
{
    memset(flash,255,sizeof(flash));assert(passport_assets_init());assert(passport_assets_count()==6);
    uint8_t packet[1088+64]={0};memcpy(packet,"PWF1",4);put32(packet+4,2);
    packet[8]=packet[10]=packet[12]=packet[14]=4;put32(packet+32,sizeof(packet)-64);
    memset(packet+1088,0x35,64);put32(packet+36,passport_crc32(0,packet+64,sizeof(packet)-64));
    httpd_req_t r={.content_len=sizeof(packet),.body=packet,.timeouts=2};
    passport_assets_upload(&r,false,0);assert(r.status==200);
    assert(passport_assets_photo(0,false)->data[0]==0x35);
    uint32_t old=committed;unsigned revision=passport_assets_revision();
    packet[1088]=0x99; /* Wrong CRC, old object must survive. */
    r=(httpd_req_t){.content_len=sizeof(packet),.body=packet};passport_assets_upload(&r,false,0);
    assert(r.status==400 && committed==old && passport_assets_revision()==revision);
    assert(passport_assets_photo(0,false)->data[0]==0x35);
    put32(packet+36,passport_crc32(0,packet+64,sizeof(packet)-64));
    r=(httpd_req_t){.content_len=sizeof(packet),.body=packet,.fail_at=1100};
    assert(passport_assets_upload(&r,false,0)==ESP_FAIL && committed==old);
    commit_failed=true;r=(httpd_req_t){.content_len=sizeof(packet),.body=packet};
    passport_assets_upload(&r,false,0);assert(r.status==500 && committed==old);
    assert(passport_assets_photo(0,false)->data[0]==0x35);commit_failed=false;
    write_failed=true;r=(httpd_req_t){.content_len=sizeof(packet),.body=packet};
    passport_assets_upload(&r,false,0);assert(r.status==500 && committed==old);write_failed=false;
    assert(passport_assets_init());assert(passport_assets_photo(0,false)->data[0]==0x35);
    assert(passport_assets_delete(0) && passport_assets_count()==5);
    assert(!passport_assets_delete(6));assert(passport_assets_init() && passport_assets_count()==5);
    r=(httpd_req_t){.content_len=sizeof(packet),.body=packet};passport_assets_upload(&r,false,0);
    assert(r.status==200 && passport_assets_count()==6 && passport_assets_photo(0,false)->data[0]==0x99);
    assert(passport_assets_init() && passport_assets_photo(0,false)->data[0]==0x99);
    puts("Passport asset tests: PASS (partial reads, CRC, interrupted upload, failed writes/commit, restart, delete/replace)");
}
