#include "passport_transfer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(n>>(8*i))&255;}
static void dimensions(uint8_t *h,unsigned i,unsigned w,unsigned v)
{
    h[8+i*4]=w&255;h[9+i*4]=w>>8;h[10+i*4]=v&255;h[11+i*4]=v>>8;
}
int main(void)
{
    uint8_t h[64]={0};memcpy(h,"PWF1",4);put32(h+4,TRANSFER_PHOTO);
    dimensions(h,0,156,208);dimensions(h,1,240,320);
    size_t size=1088+(156*208+240*320)*2;put32(h+32,size-64);
    passport_transfer_t t;assert(passport_transfer_parse(h,size,&t));
    assert(t.width[0]==156 && t.height[1]==320);
    assert(!passport_transfer_parse(h,size-1,&t));
    h[63]=1;assert(!passport_transfer_parse(h,size,&t));h[63]=0;
    dimensions(h,0,0,208);assert(!passport_transfer_parse(h,size,&t));
    dimensions(h,0,209,208);assert(!passport_transfer_parse(h,size,&t));
    put32(h+4,TRANSFER_PROFILE);size=1088;
    const unsigned dims[][2]={{118,133},{80,40},{80,24},{80,24},{188,32}};
    for(unsigned i=0;i<5;i++){dimensions(h,i,dims[i][0],dims[i][1]);size+=dims[i][0]*dims[i][1]*2;}
    put32(h+32,size-64);assert(size<65536 && passport_transfer_parse(h,size,&t));
    dimensions(h,4,188,33);assert(!passport_transfer_parse(h,size,&t));
    put32(h+4,0xffffffff);assert(!passport_transfer_parse(h,size,&t));
    assert(passport_crc32(0,"123456789",9)==0xcbf43926);
    uint32_t a=passport_crc32(0,"1234",4);assert(passport_crc32(a,"56789",5)==0xcbf43926);
    assert(passport_crc32(0,"",0)==0);
    puts("Passport transfer tests: PASS (bounds, dimensions, malformed records, streaming CRC)");
}
