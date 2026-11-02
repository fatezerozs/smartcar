#ifndef SMARTCAR_SWITCH_IO_H
#define SMARTCAR_SWITCH_IO_H
#include "route_lane.h"
static int switch_a,switch_b;
static int switch_delta_left=-15,switch_delta_right=15;
static inline void switch_step(int frames)
{
    now+=10;raw3=(uint16_t)(raw3+switch_delta_left);raw4=(uint16_t)(raw4+switch_delta_right);
    app_mode_sample(switch_a,switch_b,now);vision_drive_safety_tick();
    if(app_mode_take_change())vd_route_switch_mode_stop();
    vision_drive_set_local_allowed(app_mode_allows(APP_MODE_DRIVE));vision_drive_poll(now);
    if(frames&&now%20U==0)vision_drive_observe(&route_lane,++route_sequence,now,now);
}
static inline void switch_wait(unsigned n,int frames){while(n--)switch_step(frames);}
static inline void switch_boot(int a)
{
    now=0;raw3=raw4=0;mask=0;route_sequence=0;switch_a=a;switch_b=1;switch_delta_left=-15;switch_delta_right=15;
    memset(pwm,0,sizeof(pwm));app_mode_init(a,switch_b,now);vision_drive_init(now);set_lane(0);
}

#endif
