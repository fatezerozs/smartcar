#include "main.h"
#include "vision_profile.h"
#include "board_camera.h"
static uint8_t ready,attempted;
static uint32_t last_attempt;
/* GPT1 is initialized and started before board_init/camera service. The CSI
 * completion ISR reads this same boot-relative millisecond clock. */
static uint32_t capture_clock_ms(void){return timer_get(GPT_TIM_1);}
void board_camera_reset(void) {ready=attempted=0;last_attempt=0;}
int board_camera_poll(int requested,uint32_t now)
{
    if(!requested)return 0;
    if(ready)return 1;
    if(attempted && (uint32_t)(now-last_attempt)<VISION_CAMERA_RETRY_MS)return 0;
    attempted=1;last_attempt=now;
    mt9v03x_capture_clock_ms=capture_clock_ms;
    ready=(uint8_t)(mt9v03x_init()==0);
    return ready;
}
