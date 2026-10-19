#ifndef VISION_DRIVE_H
#define VISION_DRIVE_H
#include "vision_profile.h"
#include "vision_control.h"

/* 现场配置：调参先作用于 RAM；只有显式 @save 才持久化。
 * 启动先尝试恢复有效记录，否则保留零 PWM 默认值。C12 本地启动和
 * 串口 a 的授权源独立；舵机 right_sign 必须确认，不能从轮速推断。 */
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
/* Called once by startup after persistent restore, NEVER by UART commands.
 * One attempt per boot/C12 enable edge; rejected deadlines never retry until
 * an explicit new OFF>=300ms/ON cycle. MODE/RST are not application keys. */
void vision_drive_schedule_boot(uint32_t boot_ms, uint32_t now_ms);
/* Raw C12 master-enable sampled by PIT1. Opening cuts PWM immediately;
 * foreground handles debounce/countdown. No floating point/UART in ISR. */
void vision_drive_switch_sample(int closed);
/* Foreground mode gate: only midterm TEXT mode may schedule local motion.
 * Image/pause/full-observer/config parsing must not queue a later launch. */
void vision_drive_set_local_allowed(int allowed);
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
#endif
