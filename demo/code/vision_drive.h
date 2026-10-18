#ifndef VISION_DRIVE_H
#define VISION_DRIVE_H
#include "vision_profile.h"
#include "vision_control.h"

void vision_drive_init(uint32_t now_ms);
void vision_drive_observe(const vision_result_t *result, uint32_t sequence,
                          uint32_t frame_ms, uint32_t now_ms);
void vision_drive_poll(uint32_t now_ms);
/* a=arm, h=heartbeat, s=stop, c=clear latched fault. No motion by default. */
int vision_drive_command(char command, uint32_t now_ms, int text_mode);
void vision_drive_stop(void);
void vision_drive_trip(uint32_t reason);
const vc_output_t *vision_drive_output(void);
int vision_drive_ready(void);
/* Small integer-only guard, called only by the reserved PIT1 safety branch. */
void vision_drive_safety_tick(void);
#endif
