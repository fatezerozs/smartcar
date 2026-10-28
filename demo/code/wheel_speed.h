#ifndef WHEEL_SPEED_H
#define WHEEL_SPEED_H
#include <stdint.h>
/* Gains are Q8: permille PWM per count/10ms; ki includes the 10ms period. */
typedef struct { int32_t integral_q8; uint16_t output; } ws_pi_t;
void ws_pi_reset(ws_pi_t *p);
uint16_t ws_pi_step(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp_q8,uint16_t ki_q8,uint16_t cap,unsigned periods);
/* Measured sample duration; ki retains its confirmed nominal 10ms value.
 * cap bounds actual PWM and signed integral trim (-feedforward..cap-feedforward)
 * in Q8; it is neither a target-speed limit nor a target slew limit. */
uint16_t ws_pi_step_ff_ms(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp_q8,uint16_t ki_q8,uint16_t cap,unsigned elapsed_ms,
                    uint16_t feedforward);
/* Add a bounded target-scaled PWM baseline; signed PI trim can remove an
 * excessive baseline. Physical PWM remains nonnegative: no reverse braking.
 * A zero target, stale period or STOP still resets the complete output. */
uint16_t ws_pi_step_ff(ws_pi_t *p,int32_t target,int32_t measured,
                    uint16_t kp_q8,uint16_t ki_q8,uint16_t cap,unsigned periods,
                    uint16_t feedforward);
#endif
