#ifndef B3_BUILD_H
#define B3_BUILD_H
#include "vision_profile.h"
#if !VISION_ROUTE_SWITCH || !VISION_MIDTERM_ROUTE_TRIAL || VISION_MIDTERM_AUTONOMOUS || !VISION_ENCODER_ENABLE
#error "B3 static prerequisite must use the C26 route owner with Encoder enabled"
#endif
#undef VISION_FIELD_BUILD
#define VISION_FIELD_BUILD "RECORDER_B3_STATIC_C26_1"
#endif
