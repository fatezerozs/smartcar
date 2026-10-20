#include "app_mode.h"
static volatile uint8_t raw_mode, selected_mode, settled;
static volatile uint32_t changed_ms, revision;
static uint32_t consumed;
static uint8_t decode(int a, int b)
{ return (uint8_t)((a ? 0U:1U) | (b ? 0U:2U)); }
void app_mode_init(int a,int b,uint32_t now)
{
    raw_mode=decode(a,b);selected_mode=APP_MODE_OBSERVE;
    changed_ms=now;settled=0;++revision;
}
void app_mode_sample(int a,int b,uint32_t now)
{
    uint8_t value=decode(a,b);
    if(value!=raw_mode) {raw_mode=value;changed_ms=now;settled=0;++revision;}
    if(!settled && (uint32_t)(now-changed_ms)>=30U) {
        selected_mode=raw_mode;settled=1;++revision;
    }
}
app_mode_t app_mode_current(void) {return (app_mode_t)selected_mode;}
int app_mode_allows(app_mode_t wanted)
{return settled && selected_mode==(uint8_t)wanted;}
int app_mode_take_change(void) {uint32_t event=revision;if(event==consumed)return 0;consumed=event;return 1;}
const char *app_mode_name(app_mode_t mode)
{
    static const char *const names[]={"OBSERVE","SERVO","MOTOR","DRIVE"};
    return (unsigned)mode<4U?names[mode]:"INVALID";
}
