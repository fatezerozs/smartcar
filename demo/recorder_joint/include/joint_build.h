#ifndef JOINT_BUILD_H
#define JOINT_BUILD_H
#include "vision_profile.h"
#if !VISION_ROUTE_SWITCH || !VISION_MIDTERM_ROUTE_TRIAL || VISION_MIDTERM_AUTONOMOUS || !VISION_ENCODER_ENABLE
#error "Joint static regression requires the original C26 route owner"
#endif
#include <stdint.h>
uint32_t b2_io_begin(void);
void b2_io_end(unsigned,uint32_t,uint32_t,int);
#define RECORDER_IO_BEGIN() b2_io_begin()
#define RECORDER_IO_END(k,t,n,ok) b2_io_end(k,t,n,ok)
#define RECORDER_VOLUME "2:"
#define RECORDER_FIRMWARE_BUILD "RECORDER_JOINT_STATIC_C26_1"
#define RECORDER_STOP_REASON "bounded_joint_static_window"
#undef VISION_FIELD_BUILD
#define VISION_FIELD_BUILD "RECORDER_JOINT_STATIC_C26_1"
#endif
