#ifndef VISION_PROFILE_H
#define VISION_PROFILE_H

#ifndef VISION_ROUTE_SWITCH
#define VISION_ROUTE_SWITCH 0
#endif
#ifndef VISION_MIDTERM_ROUTE_TRIAL
#define VISION_MIDTERM_ROUTE_TRIAL 0
#endif
#if VISION_MIDTERM_ROUTE_TRIAL
#include "midterm_route_profile.h"
#endif

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
/* 跟踪观测参数：集中配置，不是赛道位置或环岛步骤。默认值保持既有
 * 回归行为；覆盖分数是行覆盖，不得称作贝叶斯后验或正确率。 */
#define VISION_MIN_CONTRAST 18
#define VISION_ROW_THRESHOLD_DEADBAND 8
#define VISION_MAX_INFER_AGE 5
#define VISION_GRAY_PERCENTILE_DENOMINATOR 50U
#define VISION_TRACK_DISTANCE_WEIGHT 4.0f
#define VISION_TRACK_WIDTH_WEIGHT 0.25f
#define VISION_TRACK_INFER_PENALTY_FRACTION (20.0f / 188.0f)
#define VISION_TRACK_NEAR_WEIGHT 3.0f
#define VISION_TRACK_MIDDLE_WEIGHT 2.0f
#define VISION_TRACK_FAR_WEIGHT 1.0f
#define VISION_TRACK_INFER_WEIGHT 0.5f
#define VISION_TRACK_MIN_NEAR_PERMILLE 50
#define VISION_TRACK_MIN_TOTAL_PERMILLE 100
#define VISION_TRACK_MIN_BEND_PERMILLE 34
#define VISION_TRACK_MIN_COVERAGE 25
#define VISION_TRACK_HISTORY_WEIGHT 0.6f
/* 使用 tools/fit_track_model.py 的标注数据模型时显式启用；不是安全解锁。 */
#ifndef VISION_BAYES_FITTED_MODEL
#define VISION_BAYES_FITTED_MODEL 0
#endif
/* 板级时序/载波属于硬件参数，不是赛道定时动作。 */
#define VISION_MOTOR_PWM_HZ 17000U
#define VISION_POWER_SETTLE_MS 300U
#define VISION_CAMERA_RETRY_MS 1000U
/* Legacy single-port routing only: 0=UART1, 1=UART8. Dual mode below
 * replaces UART1/UART8 RX callbacks with independent application rings. */
#ifndef VISION_WIRELESS_UART_ENABLE
#define VISION_WIRELESS_UART_ENABLE 0
#endif
/* Legacy single-port builds remain available for regression testing. */
#ifndef VISION_DUAL_UART_ENABLE
#define VISION_DUAL_UART_ENABLE 1
#endif
#ifndef VISION_LINK_MVP
#define VISION_LINK_MVP 0
#endif

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
#define VISION_DRIVE_ENABLE 1
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
/* Independent attenuation only; neutral defaults preserve equal commands.
 * These are not measured speed ratios and do not enable the drive profile. */
#define VISION_MOTOR1_SCALE 1.0f
#define VISION_MOTOR2_SCALE 1.0f

/* Starting controller settings only, NOT track-tuned or a safe-speed claim.
 * Duty is open loop; with failed encoders there is no measured-speed loop. */
#define VISION_CONTROL_KP 0.60f
#define VISION_CONTROL_KD 0.025f
#define VISION_CONTROL_HEADING_GAIN 0.35f
#define VISION_CONTROL_D_FILTER_S 0.10f
#define VISION_CONTROL_STEER_RATE 2.0f
#define VISION_CONTROL_DUTY_RISE_PER_S 0.10f
/* 中期电机开环降额项；没有编码器时不可标称为速度控制。 */
#define VISION_DUTY_LATERAL_WEIGHT 1.0f
#define VISION_DUTY_HEADING_WEIGHT 1.0f
#define VISION_DUTY_STEERING_WEIGHT 1.0f
#define VISION_DUTY_MISSING_FAR_SCALE 0.5f
#define VISION_CONTROL_FRAME_TIMEOUT_MS 100U
#define VISION_CONTROL_COMMAND_TIMEOUT_MS 500U
#define VISION_CONTROL_MAX_RUN_MS 5000U
#define VISION_CONTROL_ARM_FRAMES 5U
#define VISION_CONTROL_MIN_COVERAGE 40U
#define VISION_CONTROL_MIN_BAND_ROWS 3U
#define VISION_CONTROL_MAX_LATERAL 1.25f
#define VISION_CONTROL_SAFETY_TICK_MS 10U
/* 无有效保存记录时，硬件/相机确认与 PWM 都为零。FIELD_4 可以在
 * 完整记录恢复后按 C12 开关延时本地启动；调参/保存本身不会发车。 */
#define VISION_FIELD_LINE_CAPACITY 96U
#define VISION_FIELD_TIMEOUT_MS 1000U
#ifndef VISION_MOTOR_THRESHOLD_DIAG
#define VISION_MOTOR_THRESHOLD_DIAG 0
#endif
#ifndef VISION_MOTION_DIAG
#define VISION_MOTION_DIAG 0
#endif
#ifndef VISION_ENCODER_ENABLE
#define VISION_ENCODER_ENABLE 0
#endif
#if VISION_MOTION_DIAG && !VISION_MOTOR_THRESHOLD_DIAG
#error "Motion diagnostic requires the isolated threshold diagnostic safety profile."
#endif
#if VISION_ROUTE_SWITCH
#define VISION_FIELD_BUILD "MIDTERM_C26_19"
#elif VISION_MIDTERM_ROUTE_TRIAL
#define VISION_FIELD_BUILD "MIDTERM_ROUTE_TRAIN_1"
#elif VISION_MIDTERM_AUTONOMOUS
#define VISION_FIELD_BUILD "MIDTERM_AUTO_1"
#elif defined(VISION_MIDTERM_TRAINING) && VISION_MIDTERM_TRAINING
#define VISION_FIELD_BUILD "MIDTERM_TRAIN_1"
#elif VISION_ENCODER_ENABLE
#define VISION_FIELD_BUILD "ENCODER_SPEED_DIAG_5"
#elif VISION_MOTION_DIAG
#define VISION_FIELD_BUILD "MOTION_DIAG_2"
#elif VISION_MOTOR_THRESHOLD_DIAG
#define VISION_FIELD_BUILD "MOTOR_THRESHOLD_DIAG_1"
#else
#define VISION_FIELD_BUILD "MIDTERM_FIELD_4"
#endif
/* Explicit user-selected boot behavior. A complete CRC-checked saved vehicle
 * profile is still required; blank flash/default zero duty cannot start.
 * Delay is relative to MCU timer startup, NOT sensed motor-board power. */
#define VISION_AUTO_START_ENABLE 1U
#define VISION_AUTO_START_DELAY_MS 2000U
#define VISION_START_SWITCH_ACTIVE_LEVEL 0U /* C12 -> GND enables; open stops. */
#ifndef VISION_START_SWITCH_ENABLE
#define VISION_START_SWITCH_ENABLE 0U /* Physical key control removed; UART arm remains explicit. */
#endif
#define VISION_START_SWITCH_DEBOUNCE_MS 30U
#define VISION_START_SWITCH_REARM_MS 300U
#ifndef VISION_MOTOR_DIRECTION_CONFIRMED
#define VISION_MOTOR_DIRECTION_CONFIRMED 0
#endif
/* Bench motor experiment bounds, not route speed or encoder substitutes.
 * Start <=3%, then at most +1% per request; never exceed 10% / 500ms.
 * PWM is not measured speed/current. Operator must observe a lifted wheel.
 * Quiet time does not prove mechanical stop: confirm it before polarity edits. */
#if VISION_MOTOR_THRESHOLD_DIAG
/* Explicit separate build: compare with the working 20% / 1500ms bench test.
 * Manual one-wheel requests only; no sweep and no automatic repetition. */
#define VISION_MOTOR_PROBE_INITIAL_PERMILLE 100U
#define VISION_MOTOR_PROBE_MAX_PERMILLE 200U
#define VISION_MOTOR_PROBE_MAX_MS 1500U
#else
#define VISION_MOTOR_PROBE_INITIAL_PERMILLE 30U
#define VISION_MOTOR_PROBE_MAX_PERMILLE 100U
#define VISION_MOTOR_PROBE_MAX_MS 500U
#endif
#define VISION_MOTOR_PROBE_STEP_PERMILLE 10U
#define VISION_MOTOR_PROBE_MIN_MS 100U
#define VISION_MOTOR_PROBE_COAST_MS 2000U
/* Explicit fixed-steering tests. PWM is not speed, time is not distance. */
#if VISION_MOTION_DIAG
/* User-observed working points: M1/right=150, M2/left=100 permille.
 * Scaling and hardware confirmation still require explicit RAM commands. */
#define VISION_MOTION_MAX_PERMILLE 150U
#define VISION_MOTION_INITIAL_PERMILLE 150U
#else
#define VISION_MOTION_MAX_PERMILLE 100U
#define VISION_MOTION_INITIAL_PERMILLE VISION_MOTOR_PROBE_INITIAL_PERMILLE
#endif
#define VISION_MOTION_MIN_MS 100U
#define VISION_MOTION_MAX_MS 3000U
#if VISION_ENCODER_ENABLE && VISION_MOTION_DIAG
#define VISION_MOTION_LINE_MAX_MS 6000U
#else
#define VISION_MOTION_LINE_MAX_MS VISION_MOTION_MAX_MS
#endif
#define VISION_MOTION_SETTLE_MS 200U
#define VISION_MOTION_COAST_MS 2000U
#define VISION_MOTION_REVERSE_WAIT_MS 8000U
#define VISION_MOTION_RISE_PERMILLE_S 100U
#define VISION_MOTION_STEER_DELTA_US 50U

/* Bench-only servo probing. These are electrical experiment bounds, NOT
 * measured mechanical center/endpoints or a driving profile. First pulse is
 * nominal 1.5ms (the existing main.h maps 90 degrees to this pulse); subsequent
 * requests move at most 50us. Operator must lift wheels and observe binding.
 * No automatic sweep, no encoder dependency, no motor PWM during probing. */
#define VISION_SERVO_PROBE_INITIAL_US 1500U
#define VISION_SERVO_PROBE_STEP_US 50U
#define VISION_SERVO_PROBE_MIN_US 500U
#define VISION_SERVO_PROBE_MAX_US 2500U
#define VISION_SERVO_PROBE_MIN_MS 100U
#define VISION_SERVO_PROBE_MAX_MS 1000U

#include "midterm_profile.h"
#endif
