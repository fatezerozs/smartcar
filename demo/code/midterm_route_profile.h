#ifndef MIDTERM_ROUTE_PROFILE_H
#define MIDTERM_ROUTE_PROFILE_H
/* A-to-B TRAINING candidate, 2026-10-21. No roundabouts/cross navigation.
 * Camera axis is from the current raised camera mount; recompile if moved.
 * Successful speed baseline and measured servo window are applied in drive_init.
 * Existing steering controller and frame/heartbeat/encoder protections remain.
 * This target NEVER starts on power-up and NEVER loads a saved vehicle profile.
 */
#define MT_ROUTE_AXIS_NEAR_X 0.49733f
#define MT_ROUTE_AXIS_FAR_X 0.49833f
#define MT_ROUTE_MAX_RUN_MS 60000U
#if !defined(VISION_ENCODER_ENABLE) || !VISION_ENCODER_ENABLE || !defined(VISION_MIDTERM_TRAINING) || !VISION_MIDTERM_TRAINING
#error "Route training requires explicit training and encoder build flags."
#endif
#if defined(VISION_MIDTERM_AUTONOMOUS) && VISION_MIDTERM_AUTONOMOUS
#error "Route training must not enable autonomous boot."
#endif
#endif
