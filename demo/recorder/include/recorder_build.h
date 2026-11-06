#ifndef RECORDER_BUILD_H
#define RECORDER_BUILD_H
#if !RECORDER_TRAINING
#error "This header belongs only to the recorder training target."
#endif
#include "vision_profile.h"
#undef VISION_FIELD_BUILD
#define VISION_FIELD_BUILD "RECORDER_TRAIN_1"
#endif
