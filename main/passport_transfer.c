#include "passport_transfer.h"
#include <string.h>
static uint16_t u16(const uint8_t *p) { return p[0]|((uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return u16(p)|((uint32_t)u16(p+2)<<16); }
bool passport_transfer_parse(const uint8_t h[64], size_t total, passport_transfer_t *out)
{
    if(memcmp(h,"PWF1",4))return false;
    passport_transfer_t t={.kind=u32(h+4),.length=u32(h+32),.crc=u32(h+36)};
    if(t.kind!=TRANSFER_PROFILE && t.kind!=TRANSFER_PHOTO)return false;
    for(unsigned i=40;i<64;i++)if(h[i])return false;
    size_t expected=TRANSFER_HEADER+TRANSFER_JSON;
    static const uint16_t profile[5][2]={{118,133},{80,40},{80,24},{80,24},{188,32}};
    for(unsigned i=0;i<6;i++) {
        unsigned w=u16(h+8+4*i),v=u16(h+10+4*i);
        if(t.kind==TRANSFER_PROFILE) {
            if(i<5 ? (w!=profile[i][0] || v!=profile[i][1]) : (w || v))return false;
        }else {
            if(i<2 ? (!w || !v || w>(i?240:208) || v>(i?320:208)) : (w || v))return false;
        }
        t.width[i]=w;t.height[i]=v;expected+=(size_t)w*v*2;
    }
    if(total!=expected || t.length!=expected-TRANSFER_HEADER)return false;
    *out=t;return true;
}
uint32_t passport_crc32(uint32_t previous,const void *data,size_t length)
{
    uint32_t crc=~previous;const uint8_t *p=data;
    while(length--) {
        crc^=*p++;
        for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1)));
    }
    return ~crc;
}
