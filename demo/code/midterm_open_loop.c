#include "midterm_open_loop.h"
#include "vision_profile.h"
#include <float.h>
#include <math.h>

static int range(float x, float lo, float hi) { return x == x && x >= lo && x <= hi; }
void mol_default_config(mol_config_t *c)
{
    c->lateral_weight = VISION_DUTY_LATERAL_WEIGHT;
    c->heading_weight = VISION_DUTY_HEADING_WEIGHT;
    c->steering_weight = VISION_DUTY_STEERING_WEIGHT;
    c->missing_far_scale = VISION_DUTY_MISSING_FAR_SCALE;
}
int mol_config_valid(const mol_config_t *c)
{
    return c && range(c->lateral_weight, 0, 10) && range(c->heading_weight, 0, 10) &&
        range(c->steering_weight, 0, 10) && range(c->missing_far_scale, 0, 1);
}
int mol_step(const mol_config_t *c, float lateral, float heading, float steering,
             float quality, int far_valid, float cruise, float cap,
             float rise, float previous, float dt, float *duty)
{
    float difficulty, target, increment;
    if (!duty) return 0;
    *duty = 0;
    if (!mol_config_valid(c) || !range(lateral, -FLT_MAX, FLT_MAX) ||
        !range(heading, -FLT_MAX, FLT_MAX) || !range(steering, -1, 1) ||
        !range(quality, 0, 1) || (far_valid != 0 && far_valid != 1) ||
        !range(cap, 0, 1) || !range(cruise, 0, cap) || !range(previous, 0, cap) ||
        !range(rise, 0, 1) || !range(dt, FLT_MIN, 1)) return 0;
    difficulty = c->lateral_weight * fabsf(lateral) + c->heading_weight * fabsf(heading) +
                 c->steering_weight * fabsf(steering);
    if (!range(difficulty, 0, FLT_MAX)) return 0;
    target = cruise * quality / (1 + difficulty);
    if (!far_valid) target *= c->missing_far_scale;
    if (target > cap) target = cap;
    increment = rise * dt;
    if (target > previous + increment) target = previous + increment;
    *duty = target;
    return 1;
}

int mol_split(float base, float cap, float scale1, float scale2,
              float *motor1, float *motor2)
{
    /* Fail closed before evaluating any floating-point product. In particular,
     * never convert NaN/Inf into an unsigned hardware PWM register value. */
    if (motor1) *motor1 = 0;
    if (motor2) *motor2 = 0;
    if (!motor1 || !motor2 || motor1 == motor2 || !range(cap,0,1) ||
        !range(base,0,cap) || !range(scale1,0,1) || !range(scale2,0,1)) return 0;
    *motor1 = base * scale1;
    *motor2 = base * scale2;
    return 1;
}
