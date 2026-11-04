#ifndef MIDTERM_ROUTE_PROFILE_H
#define MIDTERM_ROUTE_PROFILE_H
/* Route profiles: manual A-to-B training and C26 continuous-track variant.
 * Camera axis is from the current raised camera mount; recompile if moved.
 * Successful speed baseline and measured servo window are applied in drive_init.
 * Existing steering controller and frame/heartbeat/encoder protections remain.
 * Manual route target never starts on power-up. C26 switch target can start
 * with ON at boot after five valid frames. Neither loads a saved profile.
 */
#define MT_ROUTE_AXIS_NEAR_X 0.49733f
#define MT_ROUTE_AXIS_FAR_X 0.49833f
#define MT_ROUTE_MAX_RUN_MS 60000U
/* Closed-loop track driving ONLY. PWM headroom and requested speed have
 * separate limits; the feedforward baseline is 180 permille / 50 ticks.
 * Bench/motion diagnostics retain their original output limits. */
/* PWM's physical domain is 0..1000, not a chosen racing-speed ceiling.
 * Closed-loop effort may rise beyond 40%; ramp effort, not a fixed duty. */
#define MT_ROUTE_PWM_FULL_SCALE 1000U
#define MT_ROUTE_PWM_RISE_PER_SAMPLE 10U
/* Requested speed is separate from PWM headroom. Keep C19's provisional
 * 180-permille / 50-tick feedforward relation as a tuning baseline. */
#define MT_ROUTE_BASE_TICKS 50U
#define MT_ROUTE_FAST_TICKS 132U
#define MT_ROUTE_CORNER_TICKS 104U
#define MT_ROUTE_EDGE_TICKS 92U
#define MT_ROUTE_STRAIGHT_CONFIRM_MS 200U
#define MT_ROUTE_STRAIGHT_TARGET_RISE 2U
/* Operator-confirmed static endpoints, 2026-10-22: lower pulse turns right.
 * Pulse limits are measured command calibration, not wheel-angle feedback. */
#define MT_ROUTE_SERVO_RIGHT_US 1430U
#define MT_ROUTE_SERVO_CENTER_US 1550U
#define MT_ROUTE_SERVO_LEFT_US 1670U
#define MT_ROUTE_SERVO_RIGHT_SIGN (-1)
#define MT_ROUTE_SERVO_MIN_US MT_ROUTE_SERVO_RIGHT_US
#define MT_ROUTE_SERVO_MAX_US MT_ROUTE_SERVO_LEFT_US
#if !defined(VISION_ENCODER_ENABLE) || !VISION_ENCODER_ENABLE || !defined(VISION_MIDTERM_TRAINING) || !VISION_MIDTERM_TRAINING
#error "Route training requires explicit training and encoder build flags."
#endif
#if defined(VISION_MIDTERM_AUTONOMOUS) && VISION_MIDTERM_AUTONOMOUS
#error "Route training must not enable autonomous boot."
#endif
#endif
