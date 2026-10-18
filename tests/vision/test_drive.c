#include "vision_drive.h"
#include <stdio.h>
#include <string.h>
static uint32_t clock_ms, pwm[3], irq_mask;
static unsigned checks, failures, writes;
static vision_result_t r;
static void check(int condition, const char *name) { ++checks; if (!condition) { ++failures; printf("FAIL: %s\n", name); } }
uint32_t timer_get(int channel) { (void)channel; return clock_ms; }
uint32_t __get_PRIMASK(void) { return irq_mask; }
void __disable_irq(void) { irq_mask = 1; }
void __set_PRIMASK(uint32_t mask) { irq_mask = mask; }
void __DMB(void) { }
void pwm_set_duty(int channel, uint32_t duty) { check(channel >= 0 && channel < 3 && duty <= 10000, "bounded PWM write"); pwm[channel] = duty; ++writes; }
void gpio_set_level(int pin, int value) { (void)pin; check(value == 1, "never reverse motor polarity"); }
void pit_ms_init(int channel, uint32_t period) { check(channel == 1 && period == 10, "reserve PIT1 10ms safety guard"); }
void interrupt_set_priority(int irq, uint8_t priority) { check(irq == 7 && priority == 0, "independent guard priority"); }
static void frame(uint32_t seq) { clock_ms += 20; vision_drive_observe(&r, seq, clock_ms, clock_ms); }
int main(void) {
    int y;
    memset(&r, 0, sizeof(r)); r.valid = 1; r.status = VISION_TRACK; r.confidence = 90;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) { r.left[y] = 64; r.right[y] = 143; r.center[y] = 104; r.row_flags[y] = 3; }
    vision_drive_init(0);
    for (y = 1; y <= 5; ++y) frame((uint32_t)y);
    check(pwm[0] == 0 && pwm[1] == 0, "no PWM before explicit arm");
#if VISION_DRIVE_ENABLE && VISION_DRIVE_PROFILE_CONFIRMED && VISION_CAMERA_CALIBRATED
    check(!vision_drive_command('a', clock_ms, 0), "image mode cannot arm");
    check(vision_drive_command('a', clock_ms, 1), "confirmed test profile manual arm");
    frame(6);
    check(pwm[0] > 0 && pwm[0] == pwm[1] && pwm[2] > 0, "fresh control writes bounded steering and drive PWM");
    clock_ms += 80; (void)vision_drive_command('h', clock_ms, 1);
    clock_ms += 20;
    /* Simulate main/UART blocked: no poll/observe call is made here. */
    vision_drive_safety_tick();
    check(pwm[0] == 0 && pwm[1] == 0 && pwm[2] == 0, "ISR alone zeros outputs on expired frame lease");
    frame(7);
    check(vision_drive_output()->state == VC_FAULT && pwm[0] == 0, "foreground cannot re-enable after ISR stop");
    check(vision_drive_output()->faults & VC_FAULT_FRAME, "ISR fault is reported to controller");
    check(!vision_drive_command('a', clock_ms, 1), "latched fault requires explicit clear");
    check(vision_drive_command('c', clock_ms, 1), "explicit clear while stopped");
    for (y = 8; y <= 12; ++y) frame((uint32_t)y);
    check(vision_drive_command('a', clock_ms, 1), "rearm only after clear plus fresh evidence"); frame(13);
    (void)vision_drive_command('s', clock_ms, 1);
    check(pwm[0] == 0 && pwm[1] == 0 && vision_drive_output()->state == VC_DISARMED, "manual stop immediately removes PWM");
#else
    check(!vision_drive_command('a', clock_ms, 1), "default/unconfirmed profile refuses arm");
    frame(6); clock_ms += 1000; vision_drive_safety_tick(); vision_drive_poll(clock_ms);
    check(pwm[0] == 0 && pwm[1] == 0 && pwm[2] == 0, "locked build cannot emit actuator commands");
    if (!VISION_DRIVE_ENABLE) check(writes == 0, "default build never calls PWM driver");
#endif
    check(irq_mask == 0, "critical section preserves caller IRQ mask");
    printf("DRIVE RESULT: enabled=%d confirmed=%d %u checks, %u failures\n", VISION_DRIVE_ENABLE, VISION_DRIVE_PROFILE_CONFIRMED, checks, failures);
    return failures ? 1 : 0;
}
