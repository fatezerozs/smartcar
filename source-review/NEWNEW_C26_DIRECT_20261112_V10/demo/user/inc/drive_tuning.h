#ifndef DRIVE_TUNING_H
#define DRIVE_TUNING_H
/* Phase2 DISTANCE_SCALE_CHECK: two reported 1m push averages, candidate scale. */
#define TARGET_SPEED_MPS 1.4f
#define LEFT_COUNTS_PER_METER 11394.0f
#define RIGHT_COUNTS_PER_METER 11389.0f
#define CONTROL_PERIOD_S .010f
#define STEER_ANGLE_PER_PIXEL .22f
#define STEER_ERROR_DEADBAND_PX 1.0f
/* Preserve straight-line gain; recover donor authority as the bend grows. */
static inline float steer_target_us(float error)
{
    float magnitude=error<0?-error:error,t=(magnitude-10.0f)/25.0f,target;
    if(magnitude<STEER_ERROR_DEADBAND_PX)error=0;
    if(t<0)t=0;if(t>1)t=1;
    target=1550.0f+error*(STEER_ANGLE_PER_PIXEL+.28f*t)*(1000.0f/90.0f);
    if(target<1430)target=1430;if(target>1670)target=1670;
    return target;
}
typedef struct { float filtered_us,command_us; } servo_smoother_t;
static inline float servo_smooth_step(servo_smoother_t *s,float requested,int enabled)
{
    float delta,travel,offset,alpha,step;
    if(!enabled){s->filtered_us=s->command_us=1550.0f;return 1550.0f;}
    if(requested<1430)requested=1430;if(requested>1670)requested=1670;
    travel=requested-s->command_us;if(travel<0)travel=-travel;
    offset=requested-1550.0f;if(offset<0)offset=-offset;
    /* Large corrections reverse promptly; near-center noise keeps V3 filtering. */
    alpha=(travel>80||offset>60)?.60f:.25f;
    step=(travel>80||offset>60)?35.0f:20.0f;
    s->filtered_us+=alpha*(requested-s->filtered_us);
    delta=s->filtered_us-s->command_us;
    if(delta>step)delta=step;if(delta<-step)delta=-step;
    if(delta>2||delta<-2)s->command_us+=delta;
    return s->command_us;
}
#endif
