#ifndef VISION_DRIVE_H
#define VISION_DRIVE_H
#include "vision_profile.h"
#include "vision_control.h"

/* Runtime profile: tuning stays in RAM until explicit @save.
 * Restoring a valid profile never grants output authority. Legacy C12/UART
 * arm paths are build-specific; C31 auto-track uses track-gated boot and
 * rejects UART/probe/motion arming. Servo right_sign requires calibration. */
typedef struct {
    vc_config_t control;
    uint16_t servo_center_us, servo_min_us, servo_max_us;
    int8_t right_sign;
    uint8_t hardware_confirmed; /* Servo calibration only; not motor polarity. */
    uint8_t motor_direction_confirmed, motor1_forward_level, motor2_forward_level;
    float motor1_scale, motor2_scale; /* [0,1] PWM attenuation, not wheel speed. */
    uint32_t startup_delay_ms;
    uint8_t autostart_enabled; /* Saved permission for NEXT boot, not an arm. */
} vd_profile_t;

typedef enum { VD_AUTO_OFF=0, VD_AUTO_WAITING, VD_AUTO_STARTED,
               VD_AUTO_BLOCKED, VD_AUTO_CANCELLED } vd_auto_state_t;
typedef struct {
    vd_auto_state_t state;
    uint32_t boot_ms, delay_ms;
    uint8_t restored; /* Only a CRC-checked saved profile can schedule boot. */
    uint8_t switch_seen, switch_closed;
} vd_auto_info_t;
int vision_drive_profile_valid(const vd_profile_t *profile);
int vision_drive_profile_ready(const vd_profile_t *profile);
/* Legacy delayed-start scheduler, called once after profile restore.
 * C31 auto-track does not use this deadline path; it waits for fresh, valid
 * track observations and performs a separate startup qualification check. */
void vision_drive_schedule_boot(uint32_t boot_ms, uint32_t now_ms);
/* Logical C12 toggle enable sampled by PIT1. OFF cuts PWM immediately;
 * foreground handles debounce/countdown. No floating point/UART in ISR. */
void vision_drive_switch_sample(int closed);
/* Foreground mode gate: only midterm TEXT mode may schedule local motion.
 * Image/pause/full-observer/config parsing must not queue a later launch. */
void vision_drive_set_local_allowed(int allowed);
#if VISION_ROUTE_SWITCH
void vd_route_switch_mode_stop(void);
typedef struct {
    uint32_t sequence,ms,run_ms,faults;
    int16_t raw3,raw4,target_right,target_left;
    uint16_t pwm_right,pwm_left,previous_servo_us;
    float previous_lateral,previous_heading,previous_steering;
    uint8_t reason,speed_detail;
} vd_last_stop_t;
void vision_drive_last_stop(vd_last_stop_t *out);
#define VD_TRACE_CAPACITY 1024U
typedef struct {
    uint32_t ms,frame,faults;
    int16_t lateral,heading,steer,duty,raw3,raw4,target_right,target_left;
    uint16_t pwm_right,pwm_left,servo_us,width;
    uint8_t valid,coverage,row_flags,center,measured,inferred,state,source,obs_flags,junction,anchors;
    uint8_t ring,ring_rows,entry_guide_rows;
    int8_t ring_side;
} vd_trace_sample_t;
typedef struct {uint32_t run,revision;uint16_t count;uint8_t active,frozen;} vd_trace_info_t;
void vision_drive_trace_info(vd_trace_info_t *out);
int vision_drive_trace_read(unsigned index,vd_trace_sample_t *out);
uint16_t vision_drive_servo_command_us(void);
#endif
void vision_drive_get_auto(vd_auto_info_t *info);
int vision_drive_is_local(void);
int vision_drive_can_store(void);

/* Readback of the last accepted bench action, even after its lease ends.
 * This is commanded output, not a measured wheel angle/speed/current. */
typedef struct {
    uint32_t sequence;
    uint16_t value, hold_ms;
    uint8_t kind, channel, level, active; /* kind: 0 none, 1 servo, 2 wheel. */
} vd_probe_info_t;

void vision_drive_init(uint32_t now_ms);
void vision_drive_observe(const vision_result_t *result, uint32_t sequence,
                          uint32_t frame_ms, uint32_t now_ms);
void vision_drive_poll(uint32_t now_ms);
/* a=arm, h=heartbeat, s=stop, c=clear latched fault. No motion by default. */
int vision_drive_command(char command, uint32_t now_ms, int text_mode);
void vision_drive_stop(void);
void vision_drive_trip(uint32_t reason);
const vc_output_t *vision_drive_output(void);
int vision_drive_ready(void);
/* Small integer-only guard, called only by the reserved PIT1 safety branch. */
void vision_drive_safety_tick(void);
void vision_drive_get_profile(vd_profile_t *profile);
/* 仅已停车且无锁存故障时原子应用完整合法配置，不隐式解锁。 */
int vision_drive_configure(const vd_profile_t *profile);
/* Bench probe only: DISARMED, no fault, motor cruise/cap both zero.
 * Initial pulse is nominal, not calibrated center; max 50us per later request.
 * PIT1 and foreground expire the bounded pulse; STOP cancels immediately.
 * Does not confirm hardware, change calibration or enable driving. */
int vision_drive_servo_probe(uint16_t pulse_us, uint16_t hold_ms, uint32_t now_ms);
int vision_drive_motor_probe(uint8_t channel, uint16_t duty_permille, uint8_t level,
                             uint16_t hold_ms, uint32_t now_ms);
void vision_drive_get_probe(vd_probe_info_t *info);
/* Explicit open-loop motion experiment; never scheduled at boot. */
typedef enum { VD_MOTION_LINE=1, VD_MOTION_SHUTTLE, VD_MOTION_CIRCLE } vd_motion_kind_t;
typedef enum { VD_MOTION_IDLE=0, VD_MOTION_SETTLE, VD_MOTION_FORWARD,
               VD_MOTION_WAIT_REVERSE, VD_MOTION_REVERSE_SETTLE, VD_MOTION_REVERSE } vd_motion_phase_t;
typedef enum { VD_MOTION_NONE=0, VD_MOTION_DONE, VD_MOTION_STOP,
               VD_MOTION_HEARTBEAT, VD_MOTION_MODE, VD_MOTION_FAULT,
               VD_MOTION_WAIT_TIMEOUT } vd_motion_reason_t;
typedef struct {
    uint32_t sequence, phase_ms, heartbeat_ms, duration_ms, stopped_ms;
    uint16_t requested_permille, actual_permille, servo_us;
    uint8_t kind, phase, reason;
} vd_motion_info_t;
int vision_drive_motion_start(unsigned kind,uint16_t permille,uint16_t duration_ms,
                              uint16_t servo_us,uint32_t now_ms);
int vision_drive_motion_reverse(uint32_t now_ms,int allowed);
void vision_drive_motion_get(vd_motion_info_t *info);
int vision_drive_motion_active(void);
#endif
