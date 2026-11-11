#include "app_mode.h"
#include "vision_profile.h"
static volatile uint8_t raw_mode, selected_mode, settled;
static volatile uint32_t changed_ms, revision;
static uint32_t consumed;
#if VISION_ROUTE_SWITCH
static volatile uint32_t start_request;
static uint8_t off_seen;
#endif
static uint8_t decode(int a, int b)
{
#if VISION_ROUTE_SWITCH
    (void)b;return a?APP_MODE_OBSERVE:APP_MODE_DRIVE;
#else
    return (uint8_t)((a ? 0U:1U) | (b ? 0U:2U));
#endif
}
void app_mode_init(int a,int b,uint32_t now)
{
#if VISION_ROUTE_SWITCH
    start_request=0;off_seen=0;
#endif
    raw_mode=decode(a,b);selected_mode=APP_MODE_OBSERVE;
    changed_ms=now;settled=0;++revision;
}
void app_mode_sample(int a,int b,uint32_t now)
{
    uint8_t value=decode(a,b);
    if(value!=raw_mode) {raw_mode=value;changed_ms=now;settled=0;++revision;}
    if(!settled && (uint32_t)(now-changed_ms)>=30U) {
        selected_mode=raw_mode;settled=1;++revision;
#if VISION_ROUTE_SWITCH
        if(selected_mode==APP_MODE_OBSERVE)off_seen=1;
        else if(off_seen){++start_request;off_seen=0;}
#endif
    }
}
app_mode_t app_mode_current(void) {
#if VISION_MIDTERM_AUTONOMOUS
return APP_MODE_DRIVE;
#else
return (app_mode_t)selected_mode;
#endif
}
int app_mode_allows(app_mode_t wanted)
{
#if VISION_MIDTERM_AUTONOMOUS
    return wanted==APP_MODE_DRIVE; /* Dedicated acceptance target; no physical selector dependency. */
#else
    return settled && selected_mode==(uint8_t)wanted;
#endif
}
int app_mode_take_change(void) {
#if VISION_MIDTERM_AUTONOMOUS
    consumed=revision;return 0;
#else
    uint32_t event=revision;if(event==consumed)return 0;consumed=event;return 1;
#endif
}
const char *app_mode_name(app_mode_t mode)
{
    static const char *const names[]={"OBSERVE","SERVO","MOTOR","DRIVE"};
    return (unsigned)mode<4U?names[mode]:"INVALID";
}

uint32_t app_mode_start_request(void)
{
#if VISION_ROUTE_SWITCH
    return start_request;
#else
    return 0;
#endif
}

int app_mode_change_pending(void){return revision!=consumed;}
