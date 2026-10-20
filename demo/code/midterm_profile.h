#ifndef MIDTERM_PROFILE_H
#define MIDTERM_PROFILE_H
/* Separate generated train/auto targets share the same controller.
 * Do not turn a bench experiment into an autonomous acceptance profile.
 * Vehicle/camera configuration is restored by the existing CRC store.
 * Fill these values from the successful real speed-loop configuration.
 * The uvprojx enables modules but does NOT persist UART RAM parameters. */
#ifndef VISION_MIDTERM_AUTONOMOUS
#define VISION_MIDTERM_AUTONOMOUS 0
#endif
#ifndef MIDTERM_SPEED_CONFIRMED
#define MIDTERM_SPEED_CONFIRMED 0
#endif
#ifndef MIDTERM_ENCODER_LEFT_SIGN
#define MIDTERM_ENCODER_LEFT_SIGN 0
#endif
#ifndef MIDTERM_ENCODER_RIGHT_SIGN
#define MIDTERM_ENCODER_RIGHT_SIGN 0
#endif
#ifndef MIDTERM_SPEED_MAX_TICKS
#define MIDTERM_SPEED_MAX_TICKS 0U
#endif
#ifndef MIDTERM_SPEED_KP
#define MIDTERM_SPEED_KP 0.0f
#endif
#ifndef MIDTERM_SPEED_KI
#define MIDTERM_SPEED_KI 0.0f
#endif
/* A second explicit review gate: static camera-axis calibration AND the
 * raised-wheel visual steering-direction check must both have passed. */
#ifndef MIDTERM_VISUAL_REVIEWED
#define MIDTERM_VISUAL_REVIEWED 0
#endif
#define MIDTERM_BOOT_DELAY_MS 3000U
#if VISION_MIDTERM_AUTONOMOUS && (!VISION_ENCODER_ENABLE || !VISION_DRIVE_ENABLE || VISION_MOTOR_THRESHOLD_DIAG || VISION_START_SWITCH_ENABLE)
#error "Autonomous midterm requires encoders/control, without bench or C12 startup."
#endif
#endif
