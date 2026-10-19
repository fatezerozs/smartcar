#include "vision_control.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static unsigned checks, failures;
static vc_controller_t control;
static vision_result_t observation;
static vc_config_t cfg;
static uint32_t now, seq;
static float absf(float x) { return x < 0 ? -x : x; }
static void check(int condition, const char *name) {
    ++checks; if (!condition) { ++failures; printf("FAIL: %s\n", name); }
}
/* A generic band of variable width. Error is defined in units of half road
 * width; calibrated near/far axis can independently include installation yaw. */
static void road(float near_error, float far_error, int width) {
    int y;
    memset(&observation, 0, sizeof(observation));
    memset(observation.left, 255, sizeof(observation.left));
    memset(observation.right, 255, sizeof(observation.right));
    memset(observation.center, 255, sizeof(observation.center));
    observation.valid = 1; observation.status = VISION_TRACK; observation.confidence = 90;
    for (y = VISION_ROI_TOP; y <= VISION_ROI_BOTTOM; ++y) {
        float a = ((float)y / (VISION_HEIGHT - 1) - cfg.far_y) / (cfg.near_y - cfg.far_y);
        float axis = cfg.axis_far_x + a * (cfg.axis_near_x - cfg.axis_far_x);
        float e = far_error + a * (near_error - far_error);
        float center = axis * (VISION_WIDTH - 1) + e * width * 0.5f;
        int left = (int)(center - (width - 1) * 0.5f + 0.5f), right = left + width - 1;
        if (left < 2 || right > VISION_WIDTH - 3) continue;
        observation.left[y] = (uint8_t)left; observation.right[y] = (uint8_t)right;
        observation.center[y] = (uint8_t)((left + right + 1) / 2); observation.row_flags[y] = 3;
    }
}
static void setup(void) {
    vc_default_config(&cfg);
    cfg.calibration_confirmed = 1; cfg.duty_cap = 0.2f; cfg.cruise_duty = 0.1f;
    vc_init(&control, &cfg); now = 0; seq = 0;
}
static void frame(void) { now += 20; vc_observe(&control, &observation, ++seq, now, now); }
static void prepare(float e) {
    int i; setup(); road(e, e, 80); for (i = 0; i < cfg.arm_frames; ++i) frame();
    check(vc_arm(&control, now, 1), "manual arm after fresh stable frames");
    frame();
}
int main(void) {
    int i;
    float e1;
    vc_default_config(&cfg); vc_init(&control, &cfg); now = seq = 0; road(0, 0, 80);
    for (i = 0; i < 8; ++i) frame();
    check(!vc_arm(&control, now, 1) && control.output.duty == 0, "default calibration/duty profile cannot arm");
    setup(); road(0.3f, 0.3f, 60); frame(); e1 = control.output.lateral;
    setup(); road(0.3f, 0.3f, 120); frame();
    check(absf(control.output.lateral - e1) < 0.03f, "lateral error scales by measured lane width");
    setup(); cfg.axis_near_x = 0.60f; cfg.axis_far_x = 0.54f; vc_init(&control, &cfg);
    road(0, 0, 80); frame();
    check(control.output.observation_valid && absf(control.output.lateral) < 0.03f && absf(control.output.heading) < 0.04f,
          "two-point calibration compensates offset and reference-axis yaw");
    road(0, -0.3f, 80); frame();
    check(control.output.heading < -0.20f, "calibration does not automatically zero a real bend");
    /* A pitched camera may put the old far band at the horizon. Select bands
     * within the actual road instead of reusing background as a far center. */
    setup(); cfg.near_y=.86f; cfg.far_y=.68f; vc_init(&control,&cfg);
    road(0,0,80);
    for(i=VISION_ROI_TOP;i<(int)(.58f*(VISION_HEIGHT-1));++i)observation.row_flags[i]=0;
    frame();
    check(control.output.observation_valid && control.output.far_valid && absf(control.output.heading)<.04f,
          "configured lower bands use real road despite unavailable horizon geometry");
    for(i=VISION_ROI_TOP;i<VISION_HEIGHT;++i)if((float)i/(VISION_HEIGHT-1)<.74f)observation.row_flags[i]=0;
    frame();
    check(control.output.observation_valid && !control.output.far_valid && control.output.heading==0,
          "configured far band still rejects genuinely missing geometry");
    setup(); road(0, 0, 80); frame();
    check(!vc_arm(&control, now, 1), "one good frame does not arm");
    vc_observe(&control, &observation, ++seq, now, now);
    check(control.stable_frames == 0 && !control.output.observation_valid,
          "same-timestamp samples cannot manufacture stable arm evidence");
    for (i = 0; i < 8; ++i) frame();
    check(!vc_arm(&control, now, 0), "unconfirmed hardware does not arm");
    check(control.output.state == VC_DISARMED && control.output.duty == 0, "no automatic start after stable observations");
    prepare(0.3f);
    check(control.output.steering > 0 && control.output.steering <= cfg.steer_rate * 0.0201f,
          "right visual error gives slew-limited right command");
    check(control.output.duty > 0 && control.output.duty <= cfg.duty_rise_per_s * 0.0201f,
          "duty starts at zero and ramps up");
    prepare(-0.3f); check(control.output.steering < 0, "left visual error gives left command");
    observation.valid = 0; frame();
    check(control.output.state == VC_FAULT && control.output.duty == 0 && (control.output.faults & VC_FAULT_VISION),
          "invalid vision immediately zeros duty and latches fault");
    road(0, 0, 80); for (i = 0; i < 10; ++i) frame();
    check(!vc_arm(&control, now, 1), "recovered camera never auto-clears fault");
    vc_clear_fault(&control);
    check(control.config.calibration_confirmed && absf(control.config.duty_cap - cfg.duty_cap) < 0.001f,
          "fault clear preserves configuration despite aliasing");
    check(!vc_arm(&control, now, 1), "fault clear requires new stable frames and explicit arm");
    for (i = 0; i < cfg.arm_frames; ++i) frame(); check(vc_arm(&control, now, 1), "explicit rearm after new evidence");
    prepare(0); vc_poll(&control, now + cfg.frame_timeout_ms);
    check(control.output.state == VC_FAULT && (control.output.faults & VC_FAULT_FRAME) && control.output.duty == 0,
          "no-frame deadline stops independently of new observations");
    prepare(0); e1 = (float)control.last_frame_ms;
    vc_observe(&control, &observation, seq, now + 20, now + 20);
    check(control.last_frame_ms == (uint32_t)e1, "duplicate sequence cannot refresh freshness");
    vc_poll(&control, now + cfg.frame_timeout_ms);
    check(control.output.state == VC_FAULT, "duplicate input cannot keep a stale camera alive");
    prepare(0); for (i = 0; i < 30; ++i) frame();
    check(control.output.state == VC_FAULT && (control.output.faults & VC_FAULT_COMMAND), "command lease expires despite fresh camera frames");
    vc_heartbeat(&control, now);
    check(control.output.state == VC_FAULT, "late heartbeat cannot revive a fault");
    setup(); cfg.max_run_ms = 200; vc_init(&control, &cfg); road(0, 0, 80);
    for (i = 0; i < cfg.arm_frames; ++i) frame(); check(vc_arm(&control, now, 1), "short-run arm");
    for (i = 0; i < 11; ++i) { vc_heartbeat(&control, now); frame(); }
    check(control.output.faults & VC_FAULT_RUNTIME, "independent maximum armed duration");
    prepare(0); road(1.4f, 1.4f, 40); frame();
    check(control.output.faults & VC_FAULT_OFFSET, "excessive normalized lateral error stops drive");
    prepare(0); for (i = 0; i < VISION_HEIGHT; ++i) observation.row_flags[i] = 1; frame();
    check(control.output.faults & VC_FAULT_VISION, "inferred-only lane cannot authorize drive");
    setup(); cfg.duty_rise_per_s = 1; cfg.cruise_duty = cfg.duty_cap = 0.2f;
    vc_init(&control, &cfg); road(0, 0, 80);
    for (i = 0; i < cfg.arm_frames; ++i) frame(); check(vc_arm(&control, now, 1), "degraded-view setup");
    for (i = 0; i < 15; ++i) { vc_heartbeat(&control, now); frame(); }
    e1 = control.output.duty;
    for (i = VISION_ROI_TOP; i < 70; ++i) observation.row_flags[i] = 0;
    frame();
    check(control.output.state == VC_RUNNING && !control.output.far_valid && control.output.duty < e1 * 0.75f,
          "missing far band reduces duty without fabricating a heading");
    prepare(0); vc_stop(&control);
    check(control.output.state == VC_DISARMED && control.output.duty == 0 && control.stable_frames == 0,
          "manual stop disarms and removes arm evidence");
    setup(); cfg.kp = NAN; vc_init(&control, &cfg);
    check(control.output.state == VC_FAULT && (control.output.faults & VC_FAULT_CONFIG), "NaN configuration rejected");
    setup(); cfg.axis_near_x = INFINITY; vc_init(&control, &cfg);
    check(!control.configured, "infinite calibration rejected");
    setup(); cfg.far_y = cfg.near_y; vc_init(&control, &cfg);
    check(!control.configured, "zero axis baseline rejected");
    setup(); cfg.frame_timeout_ms = 0; vc_init(&control, &cfg);
    check(!control.configured, "zero deadline rejected");
    setup(); now = UINT32_MAX - 90; road(0.2f, 0.2f, 80);
    for (i = 0; i < cfg.arm_frames; ++i) frame();
    check(vc_arm(&control, now, 1), "timestamp wrap still permits fresh observations");
    for (i = 0; i < 10; ++i) { vc_heartbeat(&control, now); frame(); }
    check(control.output.state == VC_RUNNING, "control timing remains valid across uint32 wrap");
    /* Randomized sequences exercise loss/recovery, variable width and control
     * bounds, not a claim of real-track recognition or real vehicle stability. */
    setup();
    for (i = 0; i < 3000; ++i) {
        float near_e = ((i * 37) % 101 - 50) * 0.01f;
        float far_e = near_e + ((i * 13) % 41 - 20) * 0.01f;
        road(near_e, far_e, 40 + i % 100);
        if (i % 23 == 0) observation.valid = 0;
        if (i % 47 == 0) observation.confidence = 255;
        if (i % 113 == 0) vc_clear_fault(&control);
        if (i % 19 == 0) (void)vc_arm(&control, now, 1);
        vc_heartbeat(&control, now); frame();
        check(isfinite(control.output.duty) && control.output.duty >= 0 && control.output.duty <= cfg.duty_cap,
              "randomized duty stays finite, nonnegative and capped");
        check(isfinite(control.output.steering) && absf(control.output.steering) <= 1,
              "randomized steering stays finite and bounded");
        if (control.output.state != VC_RUNNING) check(control.output.duty == 0,
              "randomized disarmed/fault states never retain motor output");
    }
    printf("CONTROL RESULT: %u checks, %u failures (synthetic control, no real vehicle claim)\n", checks, failures);
    return failures ? 1 : 0;
}
