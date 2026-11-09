#ifndef SMARTCAR_SPEED_HELPERS_H
#define SMARTCAR_SPEED_HELPERS_H
#include "speed_io.h"
static inline vd_speed_info_t info(void){vd_speed_info_t s;vision_drive_speed_get(&s);return s;}
static inline void tick(int left,int right,int heartbeat)
{
    raw3=(uint16_t)(raw3+left);raw4=(uint16_t)(raw4+right);now+=10;
    if(heartbeat&&(now%100U==0))(void)vision_drive_command('h',now,1);
    vision_drive_safety_tick();vision_drive_poll(now);
}

#endif
