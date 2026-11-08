#include "passport_weather_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    const unsigned intervals[]={30,60,120,240};
    for(unsigned c=0;c<2;c++)for(unsigned i=0;i<4;i++) {
        weather_config_t s={c,intervals[i]},r=weather_config_decode(weather_config_encode(s));
        assert(r.city==c&&r.minutes==s.minutes);
    }
    assert(!weather_interval_valid(0)&&!weather_interval_valid(31));
    assert(weather_config_decode(0xffffffff).minutes==30);
    assert(weather_config_decode(0).city==0);
    assert(weather_config_decode(0x5750fe01).minutes==30);
    weather_sample_t s={.version=1,.city=1,.observed_at=1794015000,.received_at=1794015100,
        .temperature=260,.feels_like=280,.wind=24,.humidity=72,.code=2};
    assert(weather_sample_valid(&s));s.city=2;assert(!weather_sample_valid(&s));s.city=1;
    s.humidity=101;assert(!weather_sample_valid(&s));s.humidity=72;
    s.temperature=-901;assert(!weather_sample_valid(&s));s.temperature=260;
    s.observed_at=s.received_at+901;assert(!weather_sample_valid(&s));
    s.observed_at=s.received_at-86401;assert(!weather_sample_valid(&s));
    assert(!weather_epoch_valid(0)&&!weather_epoch_valid(4102444800LL));
    assert(weather_epoch_valid(1704067200));
    assert(strcmp(weather_city(0),weather_city(1)));
    assert(strcmp(weather_condition(0),weather_condition(99)));
    assert(!strcmp(weather_condition(0xffff),weather_condition(100)));
    char label[33];weather_ssid_label("Home_2.4G",label);assert(!strcmp(label,"Home_2.4G"));
    weather_ssid_label("中文热点-5G",label);assert(!strncmp(label,"????",4)&&!strcmp(label+4,"-5G"));
    weather_ssid_label("123456789012345678901234567890123456",label);assert(strlen(label)==32);
    assert(!weather_due(UINT32_MAX-10,20));assert(weather_due(20,20));assert(weather_due(21,20));
    assert(weather_retry_ms(1)==30000&&weather_retry_ms(2)==60000&&weather_retry_ms(100)==300000);
    puts("Passport weather model tests: PASS (config migration, 4 intervals/2 cities, cache bounds, clock range, SSID fallback, retry/wraparound)");
}
