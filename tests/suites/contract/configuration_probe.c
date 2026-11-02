/* Compile the actual production guards. No replacement configuration policy. */
#include "vision_profile.h"
#include "midterm_profile.h"
#include "vision_midterm.h"
#ifndef EXPECT_C31_AUTONOMOUS
#define EXPECT_C31_AUTONOMOUS 0
#endif
_Static_assert((!!VISION_MIDTERM_C31_AUTONOMOUS) == EXPECT_C31_AUTONOMOUS,
               "C31 source mode must follow the three explicit build flags");
_Static_assert(VISION_START_SWITCH_ENABLE == 0, "Physical startup gate stays disabled");
_Static_assert(MIDTERM_VISUAL_REVIEWED == 0, "Compilation must not assert a hardware review");
int main(void) { return 0; }
