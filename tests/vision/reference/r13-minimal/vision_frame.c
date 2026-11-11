#include "main.h"
#include "vision_frame.h"
#include "vision_midterm.h"
static const uint8_t *camera_buffer(void)
{
    uint8_t (* volatile *shared)[MT9V03X_W]=&mt9v03x_image;
    return (const uint8_t *)*shared;
}
int vision_frame_copy(uint8_t *destination)
{
    const uint8_t *completed;uint32_t mask=__get_PRIMASK();
    __disable_irq();completed=camera_buffer();mt9v03x_finish_flag=0;
    __DMB();__set_PRIMASK(mask);
    if(completed)memcpy(destination,completed,VISION_WIDTH*VISION_HEIGHT);
    __DMB();
    return completed && !mt9v03x_finish_flag && completed==camera_buffer();
}
