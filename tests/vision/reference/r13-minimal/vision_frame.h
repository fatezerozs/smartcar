#ifndef VISION_FRAME_H
#define VISION_FRAME_H
#include <stdint.h>
/* Destination holds VISION_WIDTH*VISION_HEIGHT bytes. Return 1 only when
 * the completed buffer stayed stable throughout the copy. */
int vision_frame_copy(uint8_t *destination);
#endif
