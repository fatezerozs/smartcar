"""Prepare an isolated response experiment from repository sources; never flash.

User-defined straight request450permille, independent PWM ceiling600permille.
No physical duty-to-speed mapping is inferred from those numbers.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
VERSION = 'PHASE2_SPEED_RESPONSE_20261109_1'


def once(text, before, after):
    if text.count(before) != 1:
        raise ValueError(f'Expected one source anchor: {before[:90]} ({text.count(before)})')
    return text.replace(before, after)


def replace_body(text, name, body):
    match = re.search(r'\b'+name+r'\s*\([^;{}]*\)\s*\{', text)
    if not match:
        raise ValueError(name)
    opening = text.index('{', match.start())
    masked = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"', lambda m: ' '*len(m[0]), text, flags=re.S)
    depth, end = 1, opening+1
    while depth:
        depth += (masked[end] == '{')-(masked[end] == '}')
        end += 1
    return text[:opening+1]+'\n'+body+'\n'+text[end-1:]


FIELD_COMMANDS = r'''
    if(!strcmp(p->line,"response_config")) {
        st_info_t z;uint32_t v[ST_CONFIG_FIELDS];size_t used;st_info(&z);
        if(!z.frozen){snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE] REJECT not frozen\r\n");return 2;}
        st_get_config(v);used=(size_t)snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE_CONFIG] gen=%lu v=",(unsigned long)z.generation);
        for(unsigned f=0;f<ST_CONFIG_FIELDS;++f)used+=(size_t)snprintf(response+used,VF_RESPONSE_CAPACITY-used,"%s%08lX",f?",":"",(unsigned long)v[f]);
        snprintf(response+used,VF_RESPONSE_CAPACITY-used,"\r\n");return 2;
    }
    if(!strcmp(p->line,"response_beep")) {
        st_info_t z;st_info(&z);
        if(!vision_drive_motion_active()&&!z.recording){st_test_marker(now);snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE] BEEP OK\r\n");}
        else snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE] REJECT busy\r\n");return 2;
    }
    if(!strncmp(p->line,"response_release ",17)) {
        unsigned gen;char extra;int released=0;
        if(sscanf(p->line+17,"%u%c",&gen,&extra)==1&&!vision_drive_motion_active())released=st_release(gen);
        snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE] RELEASE %s\r\n",released?"OK":"REJECT");return 2;
    }
    if(!strcmp(p->line,"response_info")) {
        st_info_t z;st_info(&z);
        snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE_INFO] gen=%lu start_ms=%lu stop_ms=%lu count=%lu recording=%u frozen=%u stop_seen=%u overflow=%u reason=%u crc=%08lX\r\n",
          (unsigned long)z.generation,(unsigned long)z.start_ms,(unsigned long)z.stop_ms,(unsigned long)z.count,z.recording,z.frozen,z.stop_seen,z.overflow,z.reason,(unsigned long)st_crc());return 2;
    }
    if(!strncmp(p->line,"response_row ",13)) {
        unsigned index;char extra;st_row_t z;st_info_t info;st_info(&info);
        if(sscanf(p->line+13,"%u%c",&index,&extra)==1&&index<ST_CAPACITY&&st_row(index,&z)) {
          uint32_t values[ST_FIELDS];size_t used;st_values(&z,values);
          used=(size_t)snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE_ROW] gen=%lu i=%u v=",(unsigned long)info.generation,index);
          for(unsigned f=0;f<ST_FIELDS;++f)used+=(size_t)snprintf(response+used,VF_RESPONSE_CAPACITY-used,"%s%08lX",f?",":"",(unsigned long)values[f]);
          snprintf(response+used,VF_RESPONSE_CAPACITY-used,"\r\n");
        } else snprintf(response,VF_RESPONSE_CAPACITY,"[RESPONSE] REJECT not frozen/index\r\n");return 2;
    }
'''


def adapt(name, text):
    text = text.replace('\r\n', '\n')
    if name == 'vision_profile.h':
        text = once(text, '"ENCODER_SPEED_DIAG_5"', '"'+VERSION+'"')
        for old, new in [('VISION_MOTION_MAX_PERMILLE 150U', 'VISION_MOTION_MAX_PERMILLE 600U'),
                         ('VISION_MOTION_INITIAL_PERMILLE 150U', 'VISION_MOTION_INITIAL_PERMILLE 200U'),
                         ('VISION_MOTION_LINE_MAX_MS 6000U', 'VISION_MOTION_LINE_MAX_MS 3000U'),
                         ('VISION_MOTION_RISE_PERMILLE_S 100U', 'VISION_MOTION_RISE_PERMILLE_S 300U'),
                         ('VISION_MOTOR_PROBE_STEP_PERMILLE 10U', 'VISION_MOTOR_PROBE_STEP_PERMILLE 50U')]:
            text = once(text, old, new)
    elif name == 'vision_drive.c':
        text = '#include "stop_trace.h"\n'+text
        text = once(text, 'void vision_drive_init(uint32_t now)\n{', 'void vision_drive_init(uint32_t now)\n{\n    st_init();')
    elif name == 'vision_guard.c':
        text = '#include "stop_trace.h"\n#include "board_io.h"\n'+text
        text = once(text, 'void vision_drive_safety_tick(void)\n{', 'void vision_drive_safety_tick(void)\n{\n    board_buzzer_set(st_marker(timer_get(GPT_TIM_1)));')
    elif name == 'vision_motion.c':
        text = '#include "stop_trace.h"\n'+text
        anchor = '    vision_drive_poll(now);\n    delta=pulse>'
        text = once(text, anchor, '''    vd_speed_info_t admission;
    vision_drive_speed_get(&admission);
    if(!st_can_arm() || kind!=VD_MOTION_LINE || !admission.signs_confirmed ||
       !admission.samples || now-admission.sample_ms>20U ||
       admission.raw3 < -1 || admission.raw3 > 1 || admission.raw4 < -1 || admission.raw4 > 1)return 0;
    vision_drive_poll(now);
    delta=pulse>''')
        text = once(text, 'vd.motion_last_permille=permille;accepted=vd_output_bench_start();', '''vd.motion_last_permille=permille;
    {vd_speed_info_t s;uint32_t cfg[ST_CONFIG_FIELDS];vision_drive_speed_get(&s);
     cfg[0]=permille;cfg[1]=duration;cfg[2]=(uint32_t)(vd.profile.control.duty_cap*1000.0f+.001f);
     cfg[3]=s.max_ticks;cfg[4]=s.kp_q8;cfg[5]=s.ki_q8;cfg[6]=(uint32_t)(int32_t)s.sign3;cfg[7]=(uint32_t)(int32_t)s.sign4;
     cfg[8]=s.enabled;cfg[9]=pulse;st_set_config(cfg);st_arm(now,s.samples,s.total3,s.total4);}
    accepted=vd_output_bench_start();''')
        text = replace_body(text, 'vision_drive_motion_reverse', '    (void)now;(void)allowed;return 0;')
    elif name == 'vision_probe.c':
        text = replace_body(text, 'vision_drive_servo_probe', '    (void)pulse_us;(void)hold_ms;(void)now;return 0;')
        text = replace_body(text, 'vision_drive_motor_probe', '    (void)channel;(void)duty_permille;(void)level;(void)hold_ms;(void)now;return 0;')
    elif name == 'vision_actuator.c':
        text = '#include "stop_trace.h"\n'+text
        text = replace_body(text, 'vd_output_arm', '    (void)now;(void)local;return 0; /* Diagnostic straight commands only. */')
        text = once(text, '    if(cap>PWM_DUTY_MAX)cap=PWM_DUTY_MAX;', '''    if(cap>6000U)cap=6000U; /* Independent requested user hard ceiling. */
    if(source==VD_OUTPUT_MOTION && cap>(uint32_t)vd.motion.requested_permille*10U)cap=(uint32_t)vd.motion.requested_permille*10U;''')
        text = once(text, '    write_changed(0,MOTOR1_PWM,right);write_changed(1,MOTOR2_PWM,left);', '    write_changed(0,MOTOR1_PWM,right);write_changed(1,MOTOR2_PWM,left);\n    st_output((uint16_t)right,(uint16_t)left);')
        text = once(text, '    pwm_set_duty(SERVO_MOTOR_PWM, 0);', '    pwm_set_duty(SERVO_MOTOR_PWM, 0);\n    st_zero(timer_get(GPT_TIM_1),vd.motion.reason);')
    elif name == 'vision_speed.c':
        text = '#include "stop_trace.h"\n'+text
        text = once(text, 'static unsigned backwards[2],stalled[2];', 'static unsigned backwards[2],stalled[2],response_backwards[2],response_stalled[2];')
        text = once(text, 'consumed=last_control=sample_interval=0;active=0;reset_pending=1;', 'consumed=last_control=sample_interval=0;active=0;reset_pending=1;\n    memset(response_backwards,0,sizeof(response_backwards));memset(response_stalled,0,sizeof(response_stalled));')
        text = once(text, '!max_ticks||max_ticks>1000||!(kp>0&&kp<=100)||!(ki>=0&&ki<=10)', '!max_ticks||max_ticks>60||!(kp>0&&kp<=5)||!(ki>=0&&ki<=0.2f)')
        text = once(text, 'vd.control.config.duty_cap>(VISION_ROUTE_SWITCH?.18f:.15f)', 'vd.control.config.duty_cap>0.60001f')
        text = text.replace('cap>(VISION_ROUTE_SWITCH?180U:150U)', 'cap>600U')
        text = once(text, '    if(speed.enabled&&active) {', '''    st_context(elapsed,speed.faults,speed.fault_detail,speed.enabled);
    st_sample(now,speed.samples,speed.raw3,speed.raw4,speed.total3,speed.total4,vd.motion.phase,vd.motion.reason);
    if(vd.motion.phase==VD_MOTION_FORWARD) {
        if(elapsed>VD_SPEED_SAMPLE_MAX_INTERVAL_MS){trip(VS_FAULT_SAMPLE_GAP,now);return;}
        if((int32_t)speed.raw3*speed.sign3>120 || (int32_t)speed.raw4*speed.sign4>120){trip(12U,now);return;}
        {int32_t m[2]={(int32_t)speed.raw3*speed.sign3,(int32_t)speed.raw4*speed.sign4};
         for(unsigned j=0;j<2;++j){
          response_backwards[j]=m[j]<-4?response_backwards[j]+1U:0U;
          response_stalled[j]=(!speed.enabled&&vd.motion.actual_permille>=180U&&m[j]==0)?response_stalled[j]+1U:0U;
          if(response_backwards[j]>=3U){trip(j?VS_FAULT_REVERSE_RIGHT:VS_FAULT_REVERSE_LEFT,now);return;}
          if(response_stalled[j]>=50U){trip(j?VS_FAULT_NOCOUNT_RIGHT:VS_FAULT_NOCOUNT_LEFT,now);return;}
         }}
        if(st_distance_exhausted(speed.total3,speed.total4)){vd_motion_cancel(12U,now);return;}
    } else {
        memset(response_backwards,0,sizeof(response_backwards));memset(response_stalled,0,sizeof(response_stalled));
    }
    if(speed.enabled&&active) {''')
        text = once(text, 'speed.target_right=speed.target_left=0;speed.pwm_right=speed.pwm_left=0;', 'speed.target_right=speed.target_left=0;speed.pwm_right=speed.pwm_left=0;\n    st_reset_control(timer_get(GPT_TIM_1));')
        text = once(text, '        uint16_t pwm_cap=cap;', '        uint16_t pwm_cap=cap<vd.motion.requested_permille?cap:vd.motion.requested_permille;')
        text = once(text, '            int32_t targets[2]={(int32_t)right*target_ticks/cap,(int32_t)left*target_ticks/cap};', '''            /* Diagnostic request is the per-run effort ceiling; max_ticks is
             * the separate full-ramp closed-loop target in counts/10ms. */
            unsigned denominator=vd.motion.requested_permille;
            if(!denominator){work_trip(w,VS_FAULT_OUTPUT,now);return 0;}
            int32_t targets[2]={(int32_t)right*target_ticks/denominator,(int32_t)left*target_ticks/denominator};''')
        text = once(text, '        speed=w.speed;memcpy(pi,w.pi,sizeof(pi));consumed=w.consumed;last_control=w.last_control;', '''        if(w.consumed!=consumed)st_control(w.consumed,w.last_control,w.speed.target_right,w.speed.target_left,w.speed.measured_right,w.speed.measured_left,w.pi[0].integral_q8,w.pi[1].integral_q8);
        speed=w.speed;memcpy(pi,w.pi,sizeof(pi));consumed=w.consumed;last_control=w.last_control;''')
        text = once(text, '"no_count_right","no_count_left"', '"no_count_right","no_count_left","response_overspeed"')
    elif name == 'vision_field.c':
        text = '#include "stop_trace.h"\n'+text
        text = once(text, '    vision_drive_get_profile(&cfg);', '    vision_drive_get_profile(&cfg);\n'+FIELD_COMMANDS)
        text = text.replace('0<cap<=0.15', '0<cap<=0.60; response_run cap<=requested; normal_request=450')
    elif name == 'vision_telemetry.c':
        text = replace_body(text, 'vision_telemetry_help', r'''
    app_console_printf("[FIELD] build=%s; @status, @startup, @servo, @motor, @motor_dir, @balance, @encoder_sign, @speed_cfg, @speed_mode\r\n",VISION_FIELD_BUILD);
    app_console_printf("[RESPONSE] straight_request=450 hard_pwm_cap=600 first_request<=200 step<=50 ramp=300permille/s duration<=3000ms heartbeat_timeout=500ms; measured speed unknown\r\n");
    app_console_printf("[RESPONSE] @motion line request ms only; h heartbeat !=STOP; visual arm, probes, reverse, shuttle and circle disabled; boot stopped\r\n");
    app_console_printf("[RESPONSE] @response_info @response_config @response_row index @response_release generation @response_beep; frozen22field trace plus10field config CRC\r\n");
    app_console_printf("[ENCODER_SPEED] enc3 LEFT sign=-1 enc4 RIGHT sign=+1 are operator-confirmed working configuration, not auto-restored; speed_mode0=open request,1=closed max_ticks target; feedforward0\r\n");
    app_console_printf("[LINK] UART1 USB / UART8 radio1152008N1; x=claim while stopped; STOP on either port\r\n");
    app_console_printf("[MODE] both switches OFF=OBSERVE(raw1/1), both ON=DRIVE(raw0/0); mode change=STOP; no automatic start\r\n");
''')
    return text


def prepare(output):
    out = Path(output).resolve()
    if not out.is_relative_to(ROOT/'.build/phase2-calibration-20261108') or out.exists():
        raise ValueError('Use a new private directory below .build/phase2-calibration-20261108')
    base = ROOT/'demo/mdk/base/rt1064.uvprojx'
    out.mkdir(parents=True)
    for name in ('code', 'user'):
        shutil.copytree(ROOT/'demo'/name, out/'demo'/name)
    changed = {}
    for name in ('vision_profile.h','vision_drive.c','vision_guard.c','vision_motion.c','vision_probe.c','vision_actuator.c','vision_speed.c','vision_field.c','vision_telemetry.c'):
        p = out/'demo/code'/name
        old = p.read_bytes()
        new = adapt(name, old.decode('utf-8-sig')).encode('utf-8')
        p.write_bytes(new)
        changed[name] = {'originalSha256': hashlib.sha256(old).hexdigest(), 'generatedSha256': hashlib.sha256(new).hexdigest()}
    for ext in ('c', 'h'):
        s = ROOT/f'tools/speed_planning/speed_response_trace.{ext}'
        text = s.read_text(encoding='utf-8').replace('"speed_response_trace.h"', '"stop_trace.h"')
        (out/f'demo/code/stop_trace.{ext}').write_text(text, encoding='utf-8')
    tree = ET.parse(base)
    target = tree.find('.//Target')
    provenance = {}
    def rebase(path):
        path = path.resolve()
        private = any(path.is_relative_to(ROOT/'demo'/name) for name in ('code', 'user'))
        return out/path.relative_to(ROOT) if private else path
    for file in target.findall('.//File'):
        path = (base.parent/file.findtext('FilePath').replace('\\','/')).resolve()
        provenance[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
        file.find('FilePath').text = str(rebase(path))
    for include in target.findall('.//IncludePath'):
        if include.text:
            include.text = ';'.join(str(rebase(base.parent/p.strip().replace('\\','/'))) for p in include.text.split(';') if p.strip())
    for scatter in target.findall('.//ScatterFile'):
        scatter.text = str((base.parent/scatter.text.replace('\\','/')).resolve())
    common = target.find('TargetOption/TargetCommonOption')
    for name,value in [('OutputDirectory',str(out/'Objects')+'\\'),('ListingPath',str(out/'Listings')+'\\'),('OutputName','phase2_speed_response'),('CreateHexFile','0')]:
        common.find(name).text=value
    for group in ('BeforeCompile','BeforeMake','AfterMake'):
        for field in common.find(group):
            if field.tag.startswith(('Run','nStop')):field.text='0'
            elif 'Name' in field.tag:field.text=''
    defines = target.find('.//Cads/VariousControls/Define')
    defines.text = 'VISION_ENCODER_ENABLE=1,VISION_MOTION_DIAG=1,VISION_MOTOR_THRESHOLD_DIAG=1,'+defines.text
    target.find('.//UpdateFlashBeforeDebugging').text='0'
    files = target.find('.//Group/Files')
    f=ET.SubElement(files,'File');ET.SubElement(f,'FileName').text='stop_trace.c';ET.SubElement(f,'FileType').text='1';ET.SubElement(f,'FilePath').text=str(out/'demo/code/stop_trace.c')
    for name in ('Objects','Listings','tmp'):(out/name).mkdir()
    project=out/'speed_response.uvprojx';tree.write(project,encoding='utf-8',xml_declaration=True)
    target.find('.//UpdateFlashBeforeDebugging').text='1'
    download=out/'speed_response_download.uvprojx';tree.write(download,encoding='utf-8',xml_declaration=True)
    options=ROOT/'demo/mdk/base/rt1064.uvoptx'
    for name in ('speed_response','speed_response_download'):shutil.copy2(options,out/(name+'.uvoptx'))
    manifest={'schemaVersion':'smartcar.phase2-response-candidate/1','version':VERSION,'project':str(project),'downloadProject':str(download),'archive':str(out),
              'flashed':False,'runtimeIdentityVerified':False,'productionApproved':False,'buildPassed':False,
              'straightRequestPermille':450,'hardPwmCapPermille':600,'initialRequestMaxPermille':200,'requestStepPermille':50,
              'lineMaxMs':3000,'postZeroMs':2000,'heartbeatTimeoutMs':500,'risePermillePerSecond':300,
              'softwareDistanceCutoffCountsLeftRight':[11394,11389],'overspeedCountsPer10ms':120,
              'workingCountsPerMetreLeftRight':[11394,11389],
              'straight450SpeedMps':None,'closedLoopFeedforwardPermille':0,'sampleFields':22,'configFields':10,
              'changes':changed,'sourceInputsSha256':provenance}
    (out/'candidate.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:manifest[k] for k in ('version','project','downloadProject','flashed','straightRequestPermille','hardPwmCapPermille')}))
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    prepare(parser.parse_args().output)
