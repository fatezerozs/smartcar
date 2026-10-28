#ifndef VISION_FRAME_H
#define VISION_FRAME_H
#include <stdint.h>
/* Destination holds VISION_WIDTH*VISION_HEIGHT bytes. Return 1 only when
 * the completed buffer stayed stable throughout the copy. */
typedef struct {
    uint32_t sequence, capture_ms;
    uint8_t time_valid;
} vision_frame_stamp_t;
/* Stamp belongs to the completed capture, never foreground processing time.
 * An image without a trusted capture clock may be displayed, never driven. */
int vision_frame_copy(uint8_t *destination, vision_frame_stamp_t *stamp);
#endif
