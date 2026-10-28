#include "main.h"
#include "vision_frame.h"
#include "vision_midterm.h"
static const uint8_t *camera_buffer(void)
{
    uint8_t (* volatile *shared)[MT9V03X_W]=&mt9v03x_image;
    return (const uint8_t *)*shared;
}
int vision_frame_copy(uint8_t *destination, vision_frame_stamp_t *stamp)
{
    const uint8_t *completed;uint32_t mask=__get_PRIMASK();
    __disable_irq();
    if(!destination || !stamp || !mt9v03x_finish_flag){__set_PRIMASK(mask);return 0;}
    completed=camera_buffer();
    stamp->sequence=mt9v03x_capture_sequence;
    stamp->capture_ms=mt9v03x_capture_ms;
    stamp->time_valid=mt9v03x_capture_time_valid;
    mt9v03x_finish_flag=0;
    __DMB();__set_PRIMASK(mask);
    if(completed)memcpy(destination,completed,VISION_WIDTH*VISION_HEIGHT);
    __DMB();
    mask=__get_PRIMASK();__disable_irq();
    int stable=completed && !mt9v03x_finish_flag && completed==camera_buffer() &&
        stamp->sequence==mt9v03x_capture_sequence;
    __set_PRIMASK(mask);return stable;
}
