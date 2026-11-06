#ifndef B2_BUILD_H
#define B2_BUILD_H
#include "vision_profile.h"
#undef VISION_AUTO_START_ENABLE
#define VISION_AUTO_START_ENABLE 0U
#undef VISION_FIELD_BUILD
#define VISION_FIELD_BUILD "RECORDER_CAMERA_B2_STATIC_1"
#define RECORDER_VOLUME "2:"
#define RECORDER_FIRMWARE_BUILD "RECORDER_CAMERA_B2_STATIC_1"
extern const char *recorder_b2_stop_reason;
#define RECORDER_STOP_REASON recorder_b2_stop_reason
#endif
