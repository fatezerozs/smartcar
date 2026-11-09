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

def prepare(baseline,output):
    baseline,output=baseline.resolve(),output.resolve()
    allowed=ROOT/'.build/phase2-calibration-20261108'
    if not baseline.is_relative_to(allowed) or not output.is_relative_to(allowed) or output.exists():
        raise ValueError('Existing private baseline and new private output required')
    for folder in ['code','user']:shutil.copytree(baseline/'demo'/folder,output/'demo'/folder)
    hashes={}
    def change(name,fn):
        p=output/'demo/code'/name;raw=p.read_bytes();hashes[name]=hashlib.sha256(raw).hexdigest()
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
    for stem in ['speed_response','speed_response_download']:
        tree=ET.parse(baseline/(stem+'.uvprojx'))
        for e in tree.iter():
            if e.text:e.text=e.text.replace(str(baseline),str(output))
        tree.write(output/(stem+'.uvprojx'),encoding='utf-8',xml_declaration=True)
        shutil.copy2(baseline/(stem+'.uvoptx'),output/(stem+'.uvoptx'))
    manifest={'version':VERSION,'baseline':str(baseline),'output':str(output),'sourceSha256':hashes,
        'flashed':False,'buildPassed':False,'productionApproved':False,
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
    (output/'candidate.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8');print(json.dumps(manifest))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();prepare(a.baseline,a.output)
