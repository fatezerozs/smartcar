#ifndef VISION_PROFILE_H
#define VISION_PROFILE_H

/* Geometry is expressed relative to the camera image, NOT a particular track
 * width, screenshot coordinate, lap time or a stored turn sequence. Defaults
 * preserve the previous 188x120 observation profile. Mounting still requires
 * calibration: two reference-axis points approximate offset/yaw, not arbitrary
 * lens distortion, roll or a complete camera-to-ground homography. */
#define VISION_ROI_TOP_PERMILLE 200
#define VISION_ROI_BOTTOM_PERMILLE 920
#define VISION_SEED_TOP_PERMILLE 600
#define VISION_MIN_WIDTH_PERMILLE 65
#define VISION_ERROR_TOP_PERMILLE 375
#define VISION_ERROR_BOTTOM_PERMILLE 840
#define VISION_NEAR_TOP_PERMILLE 667
#define VISION_MIDDLE_TOP_PERMILLE 500
#define VISION_BEND_NEAR_TOP_PERMILLE 710
#define VISION_BEND_NEAR_BOTTOM_PERMILLE 840
#define VISION_BEND_FAR_TOP_PERMILLE 375
#define VISION_BEND_FAR_BOTTOM_PERMILLE 540
#define VISION_EDGE_STEP_FRACTION 0.032f
#define VISION_CENTER_STEP_FRACTION 0.0745f
#define VISION_MAX_ROW_GAPS 4

/* Axis in normalized image coordinates (x right, y down). On a confirmed
 * centered STRAIGHT, measure lane-center x at these near/far y locations.
 * Never learn these automatically while following an unknown bend. */
#define VISION_AXIS_NEAR_Y 0.78f
#define VISION_AXIS_FAR_Y 0.46f
#define VISION_AXIS_NEAR_X 0.50f
#define VISION_AXIS_FAR_X 0.50f
#define VISION_CONTROL_BAND_HALF_HEIGHT 0.035f

/* All actuator outputs are locked by default, even if 'a' arrives over UART.
 * Zero pulse/duty fields below are intentional UNCONFIGURED values, not real
 * servo endpoints. Do not copy a guessed sign or average of old servo angles.
 * Build overrides are allowed for HOST TESTS, not an implicit drive profile. */
#ifndef VISION_DRIVE_ENABLE
#define VISION_DRIVE_ENABLE 0
#endif
#ifndef VISION_DRIVE_PROFILE_CONFIRMED
#define VISION_DRIVE_PROFILE_CONFIRMED 0
#endif
#ifndef VISION_CAMERA_CALIBRATED
#define VISION_CAMERA_CALIBRATED 0
#endif
#ifndef VISION_SERVO_CENTER_US
#define VISION_SERVO_CENTER_US 0
#endif
#ifndef VISION_SERVO_MIN_US
#define VISION_SERVO_MIN_US 0
#endif
#ifndef VISION_SERVO_MAX_US
#define VISION_SERVO_MAX_US 0
#endif
#ifndef VISION_SERVO_RIGHT_SIGN
#define VISION_SERVO_RIGHT_SIGN 0
#endif
#ifndef VISION_MOTOR_DUTY_CAP
#define VISION_MOTOR_DUTY_CAP 0.0f
#endif
#ifndef VISION_MOTOR_CRUISE_DUTY
#define VISION_MOTOR_CRUISE_DUTY 0.0f
#endif

/* Starting controller settings only, NOT track-tuned or a safe-speed claim.
 * Duty is open loop; with failed encoders there is no measured-speed loop. */
#define VISION_CONTROL_KP 0.60f
#define VISION_CONTROL_KD 0.025f
#define VISION_CONTROL_HEADING_GAIN 0.35f
#define VISION_CONTROL_D_FILTER_S 0.10f
#define VISION_CONTROL_STEER_RATE 2.0f
#define VISION_CONTROL_DUTY_RISE_PER_S 0.10f
#define VISION_CONTROL_FRAME_TIMEOUT_MS 100U
#define VISION_CONTROL_COMMAND_TIMEOUT_MS 500U
#define VISION_CONTROL_MAX_RUN_MS 5000U
#define VISION_CONTROL_ARM_FRAMES 5U
#define VISION_CONTROL_MIN_COVERAGE 40U
#define VISION_CONTROL_MIN_BAND_ROWS 3U
#define VISION_CONTROL_MAX_LATERAL 1.25f
#define VISION_CONTROL_SAFETY_TICK_MS 10U

#endif
