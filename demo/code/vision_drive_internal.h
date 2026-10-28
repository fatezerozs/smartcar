#ifndef VISION_DRIVE_INTERNAL_H
#define VISION_DRIVE_INTERNAL_H
/* Private adapter state. Only vd_* implementation units include this file.
 * ISR fields are volatile; compound updates retain their original critical
 * sections. No application/algorithm module may mutate this context. */
#include "main.h"
#include "vision_drive.h"
#include "app_mode.h"
#include "vision_speed.h"
typedef struct {
    vc_controller_t control;
    vd_profile_t profile;
    vd_probe_info_t probe_info;
    vd_auto_info_t auto_info;
#if VISION_DRIVE_ENABLE
    volatile vd_motion_info_t motion;
    uint16_t motion_last_permille, motion_scale1, motion_scale2;
    uint32_t motion_written_pwm[3];
    volatile uint8_t motion_written_mask;
    uint8_t motion_have_stopped;
    uint8_t initialized, hardware_ready;
    volatile uint32_t guard_faults, guard_frame_ms, guard_command_ms, guard_armed_ms;
    volatile uint32_t guard_max_run_ms;
    /* Source-bound lease, granted only in vision_actuator under PRIMASK.
     * Every zero revokes it; foreground targets cannot renew it. */
    volatile uint8_t output_authorized;
    /* Sticky for this initialized software session; no persistent reset claim. */
    volatile uint8_t autonomous_stopped;
    volatile uint8_t guard_running, guard_local, mode_stop_pending;
    volatile uint8_t switch_seen, switch_closed, switch_inhibit, switch_stop_pending;
    volatile uint32_t switch_changed_ms;
    uint8_t switch_rearm, switch_boot_pending, local_permitted;
    volatile uint8_t probe_active;
    volatile uint32_t probe_started_ms, probe_hold_ms, probe_duty;
    uint16_t probe_last_us, motor_last_permille[2];
    uint8_t have_motor_probe;
    uint32_t motor_probe_started_ms, motor_probe_hold_ms;
#endif
} vd_context_t;
extern vd_context_t vd;
#if VISION_DRIVE_ENABLE
void vd_output_zero(void);
void vd_output_apply(void);
int vd_output_arm(uint32_t now,int local);
#if !VISION_MIDTERM_AUTONOMOUS
int vd_output_bench_start(void);
#endif
void vd_startup_poll_switch(uint32_t now);
#if VISION_ROUTE_SWITCH
void vd_route_switch_init(void);
void vd_route_switch_cancel(void);
void vd_route_switch_mode_stop(void);
void vd_route_switch_poll(uint32_t now);
#endif
void vd_startup_poll_deadline(uint32_t now);
void vd_probe_poll(uint32_t now);
void vd_motion_cancel(unsigned reason,uint32_t now);
void vd_motion_tick(uint32_t now);
void vd_motion_poll(uint32_t now);
#endif
#endif
