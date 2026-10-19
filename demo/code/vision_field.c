#include "vision_field.h"
#include "vision_storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

static int range(float x, float lo, float hi) { return x == x && x >= lo && x <= hi; }
void vf_init(vf_parser_t *p) { memset(p, 0, sizeof(*p)); }

/* 严格消费每个数字和整行尾部，拒绝 NaN/Inf、缺参及多余尾随命令。 */
static int numbers(const char *text, float *values, unsigned n)
{
    unsigned i;
    for (i = 0; i < n; ++i) {
        char *end;
        while (*text == ' ' || *text == '\t') ++text;
        if (!*text) return 0;
        values[i] = strtof(text, &end);
        if (end == text || !range(values[i], -FLT_MAX, FLT_MAX) ||
            (*end && *end != ' ' && *end != '\t')) return 0;
        text = end;
    }
    while (*text == ' ' || *text == '\t') ++text;
    return *text == 0;
}

/* 先构造候选完整配置，验证成功才提交；错误命令不会部分覆盖旧配置。
 * 这里没有 arm 命令，解锁仍需独立单字符 a 和连续新鲜有效帧。 */
static int execute(vf_parser_t *p, uint32_t now, char *response)
{
    vd_profile_t cfg; float v[4]; int ok = 0;
    vision_drive_get_profile(&cfg);
    if (!strcmp(p->line,"save")) {
        vs_info_t saved;
        ok=vision_drive_can_store() && vs_save(&cfg);
        vs_get_info(&saved);
        snprintf(response,VF_RESPONSE_CAPACITY,
            "[FIELD] %s; save generation=%lu error=%u; stopped; startup=%u delay_ms=%lu applies NEXT boot\r\n",
            ok?"OK":"REJECT",(unsigned long)saved.generation,saved.error,
            cfg.autostart_enabled,(unsigned long)cfg.startup_delay_ms);
        return 2;
    }
    if (!strcmp(p->line,"load")) {
        ok=vision_drive_can_store() && vs_load(&cfg) && vision_drive_configure(&cfg);
        if(ok)p->changed=1;
        snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] %s; loaded RAM only; stopped; no startup scheduled\r\n",ok?"OK":"REJECT");
        return 2;
    }
    if (!strncmp(p->line,"wheel ",6)) {
        ok=p->probe_line_permitted && numbers(p->line+6,v,4) &&
            range(v[0],1,2) && range(v[1],1,VISION_MOTOR_PROBE_MAX_PERMILLE) &&
            range(v[2],0,1) && range(v[3],VISION_MOTOR_PROBE_MIN_MS,VISION_MOTOR_PROBE_MAX_MS) &&
            v[0]==(uint8_t)v[0] && v[1]==(uint16_t)v[1] && v[2]==(uint8_t)v[2] && v[3]==(uint16_t)v[3] &&
            vision_drive_motor_probe((uint8_t)v[0],(uint16_t)v[1],(uint8_t)v[2],(uint16_t)v[3],now);
        if(ok)snprintf(response,VF_RESPONSE_CAPACITY,
            "[FIELD] OK; wheel probe channel=%u permille=%u level=%u hold_ms=%u; servo OFF; NOT calibrated\r\n",
            (unsigned)v[0],(unsigned)v[1],(unsigned)v[2],(unsigned)v[3]);
        else snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] REJECT wheel/format/fault/coast; stopped\r\n");
        return 2;
    }
    if (!strncmp(p->line,"probe ",6)) {
        /* Explicit short bench action, not a profile commit. Never sets the
         * hardware-confirmed flag or a motor duty. Parser consumes every byte. */
        ok=p->probe_line_permitted && numbers(p->line+6,v,2) && range(v[0],VISION_SERVO_PROBE_MIN_US,VISION_SERVO_PROBE_MAX_US) &&
            range(v[1],VISION_SERVO_PROBE_MIN_MS,VISION_SERVO_PROBE_MAX_MS) &&
            v[0]==(uint16_t)v[0] && v[1]==(uint16_t)v[1] &&
            vision_drive_servo_probe((uint16_t)v[0],(uint16_t)v[1],now);
        if (ok) snprintf(response,VF_RESPONSE_CAPACITY,
            "[FIELD] OK; servo probe pulse_us=%u hold_ms=%u; motors OFF; NOT calibrated\r\n",
            (unsigned)v[0],(unsigned)v[1]);
        else snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] REJECT probe/format/fault; stopped\r\n");
        return 2;
    }
    if (!strcmp(p->line,"status")) {
        vd_probe_info_t bench;vision_drive_get_probe(&bench);
        vd_auto_info_t startup;vs_info_t saved;
        vision_drive_get_auto(&startup);vs_get_info(&saved);
        snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] version=%s ready=%d cam=%u hw=%u servo=%u/%u/%u sign=%d kp=%.4f kd=%.4f kh=%.4f cruise=%.4f cap=%.4f rise=%.4f axis=%.4f/%.4f limit_ms=%lu\r\n[BANDS] near=%.4f far=%.4f half=%.4f\r\n[MOTOR] confirmed=%u dir=%u/%u scale=%.4f/%.4f\r\n[BENCH] seq=%lu kind=%u channel=%u value=%u level=%u hold_ms=%u active=%u\r\n[STARTUP] enabled=%u delay_ms=%lu state=%u restored=%u switch_seen=%u closed=%u local=%d\r\n[STORE] valid=%u slot=%u generation=%lu error=%u\r\n",
            VISION_FIELD_BUILD,vision_drive_ready(),cfg.control.calibration_confirmed,cfg.hardware_confirmed,
            cfg.servo_center_us,cfg.servo_min_us,cfg.servo_max_us,cfg.right_sign,
            (double)cfg.control.kp,(double)cfg.control.kd,(double)cfg.control.heading_gain,
            (double)cfg.control.cruise_duty,(double)cfg.control.duty_cap,(double)cfg.control.duty_rise_per_s,
            (double)cfg.control.axis_near_x,(double)cfg.control.axis_far_x,(unsigned long)cfg.control.max_run_ms,
            (double)cfg.control.near_y,(double)cfg.control.far_y,(double)cfg.control.band_half_height,
            cfg.motor_direction_confirmed,cfg.motor1_forward_level,cfg.motor2_forward_level,
            (double)cfg.motor1_scale,(double)cfg.motor2_scale,
            (unsigned long)bench.sequence,bench.kind,bench.channel,bench.value,bench.level,bench.hold_ms,bench.active,
            cfg.autostart_enabled,(unsigned long)cfg.startup_delay_ms,(unsigned)startup.state,startup.restored,
            startup.switch_seen,startup.switch_closed,vision_drive_is_local(),
            saved.valid,saved.slot,(unsigned long)saved.generation,saved.error);
        return 2;
    }
    if (!strncmp(p->line,"gain ",5) && numbers(p->line+5,v,3)) {
        cfg.control.kp=v[0]; cfg.control.kd=v[1]; cfg.control.heading_gain=v[2]; ok=1;
    } else if (!strncmp(p->line,"startup ",8) && numbers(p->line+8,v,2) &&
               (v[0]==0 || v[0]==1) && range(v[1],1000,10000) && v[1]==(uint32_t)v[1]) {
        cfg.autostart_enabled=(uint8_t)v[0];cfg.startup_delay_ms=(uint32_t)v[1];ok=1;
    } else if (!strncmp(p->line,"motor ",6) && numbers(p->line+6,v,3)) {
        cfg.control.cruise_duty=v[0]; cfg.control.duty_cap=v[1]; cfg.control.duty_rise_per_s=v[2]; ok=1;
    } else if (!strncmp(p->line,"balance ",8) && numbers(p->line+8,v,2)) {
        /* 两路只允许衰减，不用猜测左右轮身份或反转方向来消除跑偏。
         * 仍走完整配置提交：立即停车、清旧帧资格，不修改方向确认。 */
        cfg.motor1_scale=v[0]; cfg.motor2_scale=v[1]; ok=1;
    } else if (!strncmp(p->line,"motor_dir ",10) && numbers(p->line+10,v,2) &&
               (v[0]==0 || v[0]==1) && (v[1]==0 || v[1]==1)) {
        /* Only commit after operator has observed both lifted wheels.
         * Servo calibration alone cannot confirm drive polarity. */
        cfg.motor1_forward_level=(uint8_t)v[0];cfg.motor2_forward_level=(uint8_t)v[1];
        cfg.motor_direction_confirmed=1;ok=1;
    } else if (!strncmp(p->line,"duration ",9) && numbers(p->line+9,v,1) &&
               range(v[0],100,60000) && v[0]==(uint32_t)v[0]) {
        cfg.control.max_run_ms=(uint32_t)v[0]; ok=1;
    } else if (!strncmp(p->line,"bands ",6) && numbers(p->line+6,v,3)) {
        /* 安装高度/俯角变化用现场配置表达，不把某一场地地平线写进算法。
         * 更换采样行后，旧两点横坐标不再对应同一物理视线，必须重新标定。
         * vc_config_valid 统一检查 ROI、行带间距和有限数值，失败原子拒绝。 */
        cfg.control.near_y=v[0]; cfg.control.far_y=v[1]; cfg.control.band_half_height=v[2];
        cfg.control.calibration_confirmed=0; ok=1;
    } else if (!strncmp(p->line,"camera ",7) && numbers(p->line+7,v,2)) {
        cfg.control.axis_near_x=v[0]; cfg.control.axis_far_x=v[1];
        cfg.control.calibration_confirmed=1; ok=1;
    } else if (!strncmp(p->line,"servo ",6) && numbers(p->line+6,v,4) &&
        range(v[0],500,2500) && range(v[1],500,2500) && range(v[2],500,2500) &&
        (v[3]==1 || v[3]==-1) && v[0]==(uint16_t)v[0] && v[1]==(uint16_t)v[1] && v[2]==(uint16_t)v[2]) {
        cfg.servo_center_us=(uint16_t)v[0]; cfg.servo_min_us=(uint16_t)v[1];
        cfg.servo_max_us=(uint16_t)v[2]; cfg.right_sign=(int8_t)v[3];
        cfg.hardware_confirmed=1; ok=1;
    }
    ok = ok && vision_drive_configure(&cfg);
    if (ok) p->changed=1;
    snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] %s; stopped; RAM only; send @status for readback\r\n",
             ok ? "OK" : "REJECT config/format/fault");
    return 2;
}

int vf_feed(vf_parser_t *p, uint8_t byte, uint32_t now, char *response)
{
    if (byte=='!' || byte==3) {
        vision_drive_stop(); vf_init(p);
        snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] STOP\r\n"); return 2;
    }
    if (byte=='@') {
        vision_drive_stop();
        p->active=1; p->discarded=0; p->length=0; p->started_ms=now;
        p->probe_line_permitted=p->probe_permitted;
        return 1;
    }
    if (!p->active) return 0;
    if ((uint32_t)(now-p->started_ms)>=VISION_FIELD_TIMEOUT_MS) p->discarded=1;
    if (byte=='\r' || byte=='\n') {
        int result;
        p->line[p->length]=0;
        if (p->discarded) { snprintf(response,VF_RESPONSE_CAPACITY,"[FIELD] REJECT overflow/timeout\r\n"); result=2; }
        else result=execute(p,now,response);
        p->active=0; p->length=0; return result;
    }
    if (byte<32 || byte>126 || p->length>=sizeof(p->line)-1) p->discarded=1;
    if (!p->discarded) p->line[p->length++]=(char)byte;
    return 1;
}
