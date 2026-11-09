#ifndef VISION_POWER_H
#define VISION_POWER_H
#include <stdint.h>
/* Current wheel effort supplies the next PI saturation boundary. Increases
 * are gradual (1% per 10ms); lower requested effort and STOP act immediately.
 * Passing this limit into PI also prevents hidden integral windup above the
 * effort that can actually be applied. Independent for the two wheels. */
static inline uint16_t vp_effort_limit(uint16_t previous,unsigned periods)
{
    uint32_t next=(uint32_t)previous+MT_ROUTE_PWM_RISE_PER_SAMPLE*periods;
    return (uint16_t)(next>MT_ROUTE_PWM_FULL_SCALE?MT_ROUTE_PWM_FULL_SCALE:next);
}
#endif
