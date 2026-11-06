#ifndef B2_BUILD_H
#define B2_BUILD_H
#include "vision_profile.h"
#include <stdint.h>
uint32_t b2_io_begin(void);
void b2_io_end(unsigned kind,uint32_t begin,uint32_t bytes,int okay);
#define RECORDER_IO_BEGIN() b2_io_begin()
#define RECORDER_IO_END(k,t,n,ok) b2_io_end((k),(t),(n),(ok))
#undef VISION_AUTO_START_ENABLE
#define VISION_AUTO_START_ENABLE 0U
#undef VISION_FIELD_BUILD
#define VISION_FIELD_BUILD "RECORDER_CAMERA_B2_STATIC_1"
#define RECORDER_VOLUME "2:"
#define RECORDER_FIRMWARE_BUILD "RECORDER_CAMERA_B2_STATIC_1"
extern const char *recorder_b2_stop_reason;
#define RECORDER_STOP_REASON recorder_b2_stop_reason
#endif
