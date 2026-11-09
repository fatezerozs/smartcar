"""Prepare bounded open-loop short arcs from frozen heartbeat V2. Never flash."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET
from tools.prepare_phase2_response import once

VERSION='PHASE2_TURN_GEOMETRY_20261109_1'
ROOT=Path(__file__).resolve().parents[1]

def prepare(baseline,output,configurable=False):
    baseline,output=baseline.resolve(),output.resolve()
    allowed=ROOT/'.build/phase2-calibration-20261108'
    if not baseline.is_relative_to(allowed) or not output.is_relative_to(allowed) or output.exists():
        raise ValueError('Existing private baseline and new private output required')
    for folder in ['code','user']:shutil.copytree(baseline/'demo'/folder,output/'demo'/folder)
    hashes={}
    def change(name,fn):
        p=output/'demo/code'/name;raw=p.read_bytes();hashes.setdefault(name,hashlib.sha256(raw).hexdigest())
        p.write_text(fn(raw.decode('utf-8-sig').replace('\r\n','\n')),encoding='utf-8')
    change('vision_profile.h',lambda s:once(s,'PHASE2_HEARTBEAT_DIAG_20261109_2',VERSION))
    change('vision_motion.c',lambda s:once(s,
        '    if(!st_can_arm() || kind!=VD_MOTION_LINE || !admission.signs_confirmed ||',
        '''    /* Equal open-loop effort permits natural rear-wheel speed differences.
     * Equal closed-loop targets would oppose a turn. */
    if(kind==VD_MOTION_CIRCLE &&
       (admission.enabled || permille>250U || duration>1500U ||
        vd.profile.servo_center_us!=1550U || (pulse!=1500U && pulse!=1600U)))return 0;
    if(!st_can_arm() || (kind!=VD_MOTION_LINE && kind!=VD_MOTION_CIRCLE) || !admission.signs_confirmed ||'''))
    change('vision_speed.c',lambda s:once(s,
        '        if((int32_t)speed.raw3*speed.sign3>120 || (int32_t)speed.raw4*speed.sign4>120)',
        '''        unsigned turn_limit=vd.motion.kind==VD_MOTION_CIRCLE?60U:120U;
        if((int32_t)speed.raw3*speed.sign3>(int32_t)turn_limit || (int32_t)speed.raw4*speed.sign4>(int32_t)turn_limit)'''))
    change('vision_telemetry.c',lambda s:once(s,
        'visual arm, probes, reverse, shuttle and circle disabled; boot stopped',
        'arc: open-loop only request<=250 duration<=1500 servo1500/1600; visual arm, probes, reverse, shuttle disabled; boot stopped'))
    version=VERSION
    if configurable:
        version='PHASE2_TURN_GEOMETRY_20261109_2'
        header='''#ifndef TURN_CONFIG_H
#define TURN_CONFIG_H
#include <stdint.h>
typedef struct {unsigned request, duration, speed, minimum_us, maximum_us;} turn_config_t;
extern turn_config_t turn_config;
extern volatile unsigned turn_effort_cap;
void turn_governor_sample(unsigned peak,unsigned requested);
void turn_config_capture(void);
int turn_config_set(unsigned request,unsigned duration,unsigned speed,unsigned minimum,unsigned maximum);
void turn_config_format(char *text,unsigned capacity,int frozen);
#endif
'''
        (output/'demo/code/turn_config.h').write_text(header,encoding='utf-8')
        change('vision_profile.h',lambda s:once(s,VERSION,version))
        def motion_config(s):
            s='#include "turn_config.h"\n'+s
            s=once(s,'unsigned hd_quiet;static unsigned hd_quiet_at_arm;',
                '''unsigned hd_quiet;static unsigned hd_quiet_at_arm;
turn_config_t turn_config={250U,1500U,60U,1500U,1600U};
volatile unsigned turn_effort_cap=600U;static unsigned turn_interventions,turn_min_cap=600U;
static turn_config_t turn_config_frozen;static unsigned turn_config_valid;
int turn_config_set(unsigned request,unsigned duration,unsigned speed,unsigned minimum,unsigned maximum) {
    if(vision_drive_motion_active() || vd.control.output.state!=VC_DISARMED || vd.probe_active ||
       !request || request>VISION_MOTION_MAX_PERMILLE || duration<VISION_MOTION_MIN_MS ||
       duration>VISION_MOTION_LINE_MAX_MS || !speed || speed>120U ||
       minimum<vd.profile.servo_min_us || maximum>vd.profile.servo_max_us || minimum>maximum)return 0;
    turn_config_t candidate={request,duration,speed,minimum,maximum};turn_config=candidate;return 1;
}
void turn_config_capture(void){turn_config_frozen=turn_config;turn_config_valid=1;turn_effort_cap=600U;turn_interventions=0;turn_min_cap=600U;}
void turn_governor_sample(unsigned peak,unsigned requested) {
    if(turn_effort_cap>requested)turn_effort_cap=requested;
    if(peak>turn_config.speed) {
        turn_effort_cap=turn_effort_cap>10U?turn_effort_cap-10U:0U;++turn_interventions;
    } else if(peak+5U<turn_config.speed && turn_effort_cap<requested)++turn_effort_cap;
    if(turn_effort_cap<turn_min_cap)turn_min_cap=turn_effort_cap;
}
void turn_config_format(char *text,unsigned capacity,int frozen) {
    turn_config_t c=frozen?turn_config_frozen:turn_config;
    snprintf(text,capacity,"[TURN_CONFIG] frozen=%u valid=%u request=%u duration=%u speed=%u servo=%u/%u soft_hits=%u min_cap=%u\\r\\n",
        frozen!=0,frozen?turn_config_valid:1,c.request,c.duration,c.speed,c.minimum_us,c.maximum_us,turn_interventions,turn_min_cap);
}''')
            s=once(s,'hd_arm();','hd_arm();turn_config_capture();')
            s=once(s,'(admission.enabled || permille>250U || duration>1500U ||\n        vd.profile.servo_center_us!=1550U || (pulse!=1500U && pulse!=1600U))',
                '(admission.enabled || permille>turn_config.request || duration>turn_config.duration ||\n        pulse<turn_config.minimum_us || pulse>turn_config.maximum_us || pulse==vd.profile.servo_center_us)')
            # Profile pulse limits and explicit per-session range now determine travel.
            s=once(s,'(kind==VD_MOTION_CIRCLE && (!delta || delta>VISION_MOTION_STEER_DELTA_US))',
                '(kind==VD_MOTION_CIRCLE && !delta)')
            return s
        change('vision_motion.c',motion_config)
        change('vision_speed.c',lambda s:'#include "turn_config.h"\n'+once(s,
            '        unsigned turn_limit=vd.motion.kind==VD_MOTION_CIRCLE?60U:120U;',
            '''        if(vd.motion.kind==VD_MOTION_CIRCLE) {
            int32_t l=(int32_t)speed.raw3*speed.sign3,r=(int32_t)speed.raw4*speed.sign4;
            unsigned peak=(unsigned)(l>r?l:r);if(l<0&&r<0)peak=0;
            turn_governor_sample(peak,vd.motion.requested_permille);
        }
        unsigned turn_limit=120U; /* Exceptional hard fallback, separate from soft operating threshold. */'''))
        change('vision_actuator.c',lambda s:'#include "turn_config.h"\n'+once(s,
            '    uint32_t duty=vd.motion.actual_permille;',
            '    uint32_t duty=vd.motion.actual_permille;\n    if(vd.motion.kind==VD_MOTION_CIRCLE && duty>turn_effort_cap)duty=turn_effort_cap;'))
        change('vision_speed.c',lambda s:once(s,
            '!speed.enabled&&vd.motion.actual_permille>=180U&&m[j]==0',
            '!speed.enabled&&vd.motion.actual_permille>=180U&&(vd.motion.kind!=VD_MOTION_CIRCLE||turn_effort_cap>=180U)&&m[j]==0'))
        change('vision_field.c',lambda s:'#include "turn_config.h"\n'+once(s,
            '    if(!strcmp(p->line,"heartbeat_diag"))',
            '''    if(!strcmp(p->line,"turn_config")||!strcmp(p->line,"turn_frozen")) {
        turn_config_format(response,VF_RESPONSE_CAPACITY,!strcmp(p->line,"turn_frozen"));return 2;
    }
    if(!strncmp(p->line,"turn_config ",12)) {
        unsigned request,duration,speed,minimum,maximum;char extra;
        if(sscanf(p->line+12,"%u %u %u %u %u%c",&request,&duration,&speed,&minimum,&maximum,&extra)==5 &&
           turn_config_set(request,duration,speed,minimum,maximum))turn_config_format(response,VF_RESPONSE_CAPACITY,0);
        else snprintf(response,VF_RESPONSE_CAPACITY,"[TURN_CONFIG] REJECT invalid or busy\\r\\n");return 2;
    }
    if(!strcmp(p->line,"heartbeat_diag"))'''))
        change('vision_telemetry.c',lambda s:once(s,
            'arc: open-loop only request<=250 duration<=1500 servo1500/1600',
            'arc: open-loop; @turn_config request ms speed min_us max_us; @turn_config / @turn_frozen readback'))
    for stem in ['speed_response','speed_response_download']:
        tree=ET.parse(baseline/(stem+'.uvprojx'))
        for e in tree.iter():
            if e.text:e.text=e.text.replace(str(baseline),str(output))
        tree.write(output/(stem+'.uvprojx'),encoding='utf-8',xml_declaration=True)
        shutil.copy2(baseline/(stem+'.uvoptx'),output/(stem+'.uvoptx'))
    manifest={'version':version,'baseline':str(baseline),'output':str(output),'sourceSha256':hashes,
        'flashed':False,'buildPassed':False,'productionApproved':False,
        'runtimeConfigurable':configurable,
        'arcLimits':{'openLoopOnly':True,'maxRequestPermille':250,'maxDurationMs':1500,
                     'servoUs':[1500,1600],'requiredCenterUs':1550,'overspeedRawCountsPerSample':60},
        'existingProtectionsRetained':['500ms heartbeat','first200/+50 request gate','PWM final request cap',
            '1m candidate encoder cutoff','signs/fresh samples/stationary admission','reverse/nocount/sample faults',
            'frozen trace release','STOP and switch veto'],
        'limits':['Left1600 measured; right1500 only mirrored reference.',
            'Equal open-loop effort does not imply equal wheel speed; no curvature compensation.',
            'Wheel counts do not establish ground curvature or tire grip.',
            'Servo PWM ends with motor zero; coast may change wheel angle. Final pose is not driven curvature.',
            '60counts/sample is a diagnostic trip, not certified ground speed or lateral acceleration bound.']}
    if configurable:
        manifest['arcLimits']={'openLoopOnly':True,'defaults':{'request':250,'duration':1500,'softSpeed':60,'servoRangeUs':[1500,1600]},
            'configurableBounds':{'request':600,'duration':3000,'speed':120,'servoRange':'within current profile'},
            'appliesOnNextExplicitAdmission':True,'frozenConfigCoveredByTraceCrc':False}
        manifest['softGovernor']={'reducePermillePerSample':10,'recoverPermillePerSample':1,'hysteresisCounts':5,'hardFallbackCounts':120,'normalThresholdTripsMotion':False}
        manifest['limits'][-1]='Configurable speed is a soft operating threshold; common120 remains hard fallback, not a certified vehicle boundary.'
    (output/'candidate.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');print(json.dumps(manifest))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--configurable',action='store_true')
    a=p.parse_args();prepare(a.baseline,a.output,a.configurable)
