#ifndef VISION_STEERING_H
#define VISION_STEERING_H
/* Chassis convention: positive = right turn, independent of servo wiring.
 * Use the same shaped command for the servo and rear-wheel speed targets.
 * This is an empirical mixer; wheelbase/track/servo angle are not calibrated. */
static inline float vs_steering_shape(float command)
{
    if(command>1.0f)command=1.0f;
    if(command< -1.0f)command=-1.0f;
    return command*(2.0f/3.0f+command*command/3.0f);
}
static inline void vs_steering_mix(float shaped,float *right,float *left)
{
    float magnitude=shaped<0?-shaped:shaped;
    /* At full steering the inner target is 65% of the outer target.
     * Never accelerate the outer wheel or reverse a wheel to turn. */
    float inner=1.0f-.35f*magnitude;
    if(shaped>0)*right*=inner;
    else if(shaped<0)*left*=inner;
}
#endif
