#include "passport_weather_model.h"
#include "passport_text.h"
bool weather_interval_valid(unsigned m){return m==30||m==60||m==120||m==240;}
bool weather_epoch_valid(int64_t t){return t>=1704067200LL&&t<4102444800LL;}
uint32_t weather_config_encode(weather_config_t c)
{
    return 0x57500000u|(c.minutes<<1)|c.city;
}
weather_config_t weather_config_decode(uint32_t word)
{
    weather_config_t c={0,30};unsigned minutes=(word&0x1feu)>>1;
    /* 240 needs eight minute bits, including bit eight of the encoded word. */
    if((word&0xfffffe00u)==0x57500000u&&weather_interval_valid(minutes)){
        c.city=word&1;c.minutes=minutes;
    }
    return c;
}
const char *weather_city(unsigned city){return city==1?TXT_CITY_WUHAN:TXT_CITY_SHENZHEN;}
const char *weather_condition(unsigned c)
{
    switch(c){
    case 0:return TXT_W_CLEAR;case 1:return TXT_W_MAINLY_CLEAR;
    case 2:return TXT_W_CLOUDY;case 3:return TXT_W_OVERCAST;
    case 45:case 48:return TXT_W_FOG;
    case 51:case 53:case 55:return TXT_W_DRIZZLE;
    case 56:case 57:case 66:case 67:return TXT_W_FREEZING_RAIN;
    case 61:return TXT_W_LIGHT_RAIN;case 63:return TXT_W_RAIN;case 65:return TXT_W_HEAVY_RAIN;
    case 71:return TXT_W_LIGHT_SNOW;case 73:return TXT_W_SNOW;case 75:return TXT_W_HEAVY_SNOW;
    case 77:return TXT_W_SNOW_GRAINS;case 80:case 81:case 82:return TXT_W_SHOWERS;
    case 85:case 86:return TXT_W_SNOW_SHOWERS;case 95:case 96:case 99:return TXT_W_THUNDER;
    default:return TXT_W_UNKNOWN;
    }
}
bool weather_sample_valid(const weather_sample_t *s)
{
    return s&&s->version==1&&s->city<2&&weather_epoch_valid(s->observed_at)&&
        weather_epoch_valid(s->received_at)&&s->observed_at<=s->received_at+900&&
        s->received_at-s->observed_at<=86400&&s->temperature>=-900&&s->temperature<=600&&
        s->feels_like>=-1000&&s->feels_like<=900&&s->humidity<=100&&s->wind>=0&&s->wind<=1500&&
        s->code<=99;
}
void weather_ssid_label(const char *s,char out[33])
{
    unsigned n=0;
    while(*s&&n<32){unsigned char c=(unsigned char)*s++;if(c>=32&&c<127)out[n++]=(char)c;
        else {out[n++]='?';while(((unsigned char)*s&0xc0)==0x80)s++;}}
    out[n]=0;
}
bool weather_due(uint32_t now,uint32_t deadline){return (int32_t)(now-deadline)>=0;}
uint32_t weather_retry_ms(unsigned failures){return failures<=1?30000:failures==2?60000:300000;}
