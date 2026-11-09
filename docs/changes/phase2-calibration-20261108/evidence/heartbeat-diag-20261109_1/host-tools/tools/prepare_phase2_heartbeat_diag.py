"""Instrument a frozen response source tree; prepare/build only, never flash."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

from tools.prepare_phase2_response import once

VERSION = 'PHASE2_HEARTBEAT_DIAG_20261109_1'
ROOT = Path(__file__).resolve().parents[1]

HEADER = r'''#ifndef HEARTBEAT_DIAG_H
#define HEARTBEAT_DIAG_H
#include <stdint.h>
typedef struct {uint32_t count,last_ms,max_gap_ms;} hd_event_t;
typedef struct {hd_event_t rx,parsed,accepted;uint32_t stop_ms,lease_ms,reason,valid;} hd_state_t;
static inline void hd_event(hd_event_t *e,uint32_t now) {
    uint32_t gap=now-e->last_ms;
    if(e->count && gap>e->max_gap_ms)e->max_gap_ms=gap;
    ++e->count;e->last_ms=now;
}
static inline void hd_capture(hd_state_t *snapshot,const hd_state_t *live,
                             uint32_t now,uint32_t lease,unsigned reason) {
    *snapshot=*live;snapshot->stop_ms=now;snapshot->lease_ms=lease;
    snapshot->reason=reason;snapshot->valid=1;
}
#ifndef HD_HOST_TEST
extern volatile hd_state_t hd_live;
void hd_arm(void);
void hd_stop(unsigned reason,uint32_t now,uint32_t lease);
void hd_format(char *out,unsigned capacity);
#endif
#endif
'''

IMPLEMENTATION = r'''
#include "heartbeat_diag.h"
#include "app_link.h"
#include <stdio.h>
volatile hd_state_t hd_live;
static hd_state_t hd_frozen;
static uint32_t hd_radio_rx_at_arm,hd_radio_rx_at_stop;
void hd_arm(void) {
    app_link_stats_t s;app_link_stats(APP_LINK_RADIO,&s);
    memset((void *)&hd_live,0,sizeof(hd_live));memset(&hd_frozen,0,sizeof(hd_frozen));
    hd_radio_rx_at_arm=s.rx_bytes;hd_radio_rx_at_stop=0;
}
void hd_stop(unsigned reason,uint32_t now,uint32_t lease) {
    hd_state_t live=hd_live;app_link_stats_t s;app_link_stats(APP_LINK_RADIO,&s);
    hd_capture(&hd_frozen,&live,now,lease,reason);hd_radio_rx_at_stop=s.rx_bytes;
}
void hd_format(char *out,unsigned capacity) {
    uint32_t mask=__get_PRIMASK();hd_state_t z;uint32_t rx0,rx1;
    __disable_irq();z=hd_frozen.valid?hd_frozen:hd_live;
    rx0=hd_radio_rx_at_arm;rx1=hd_radio_rx_at_stop;__set_PRIMASK(mask);
    snprintf(out,capacity,"[HB_DIAG] frozen=%lu reason=%lu stop=%lu lease=%lu age=%lu rx_h=%lu/%lu/%lu parsed_h=%lu/%lu/%lu accepted_h=%lu/%lu/%lu radio_rx=%lu/%lu\r\n",
      (unsigned long)z.valid,(unsigned long)z.reason,(unsigned long)z.stop_ms,
      (unsigned long)z.lease_ms,(unsigned long)(z.valid?z.stop_ms-z.lease_ms:0),
      (unsigned long)z.rx.count,(unsigned long)z.rx.last_ms,(unsigned long)z.rx.max_gap_ms,
      (unsigned long)z.parsed.count,(unsigned long)z.parsed.last_ms,(unsigned long)z.parsed.max_gap_ms,
      (unsigned long)z.accepted.count,(unsigned long)z.accepted.last_ms,(unsigned long)z.accepted.max_gap_ms,
      (unsigned long)rx0,(unsigned long)rx1);
}
'''


def prepare(baseline, output):
    baseline, output = baseline.resolve(), output.resolve()
    allowed = ROOT / '.build/phase2-calibration-20261108'
    if not baseline.is_relative_to(allowed) or not output.is_relative_to(allowed) or output.exists():
        raise ValueError('Use an existing private baseline and a new private output below the calibration build root')
    source_hashes = {}
    for name in ('code', 'user'):
        shutil.copytree(baseline/'demo'/name, output/'demo'/name)
    def change(name, fn):
        p = output/'demo/code'/name
        raw = p.read_bytes()
        source_hashes[name] = hashlib.sha256(raw).hexdigest()
        p.write_text(fn(raw.decode('utf-8-sig').replace('\r\n','\n')), encoding='utf-8')
    change('vision_profile.h', lambda s: once(s, 'PHASE2_SPEED_RESPONSE_20261109_1', VERSION))
    change('vision_motion.c', lambda s: '#include "heartbeat_diag.h"\n'+s.replace(
        '#include "vision_drive_internal.h"', '#include "vision_drive_internal.h"\n'+IMPLEMENTATION).replace(
        '    vd.motion.phase=VD_MOTION_IDLE;vd.motion.actual_permille=0;',
        '    hd_stop(reason,now,vd.motion.heartbeat_ms);\n    vd.motion.phase=VD_MOTION_IDLE;vd.motion.actual_permille=0;').replace(
        '++vd.motion.sequence;vd.motion.kind=', 'hd_arm();\n    ++vd.motion.sequence;vd.motion.kind='))
    change('vision_drive.c', lambda s: '#include "heartbeat_diag.h"\n'+once(s,
        'if(vd.motion.phase) {vd.motion.heartbeat_ms=now;',
        'if(vd.motion.phase) {hd_event((hd_event_t *)&hd_live.accepted,now);vd.motion.heartbeat_ms=now;'))
    change('app_runtime.c', lambda s: '#include "heartbeat_diag.h"\n'+once(s,
        "            int consumed = vf_feed(&field_parser, commands[i], timer_get(GPT_TIM_1), field_response);",
        "            if((c=='h'||c=='H')&&!field_parser.active) {uint32_t hm=__get_PRIMASK();__disable_irq();hd_event((hd_event_t *)&hd_live.parsed,timer_get(GPT_TIM_1));__set_PRIMASK(hm);}\n            int consumed = vf_feed(&field_parser, commands[i], timer_get(GPT_TIM_1), field_response);"))
    change('app_link.c', lambda s: '#include "main.h"\n#include "heartbeat_diag.h"\n'+once(s,
        '    p->rx[p->rx_write%APP_LINK_RX_SIZE]=byte;',
        "    if(channel==APP_LINK_RADIO&&(byte=='h'||byte=='H'))hd_event((hd_event_t *)&hd_live.rx,timer_get(GPT_TIM_1));\n    p->rx[p->rx_write%APP_LINK_RX_SIZE]=byte;"))
    change('vision_field.c', lambda s: '#include "heartbeat_diag.h"\n'+once(s,
        '    if(!strcmp(p->line,"response_config")) {',
        '    if(!strcmp(p->line,"heartbeat_diag")){hd_format(response,VF_RESPONSE_CAPACITY);return 2;}\n    if(!strcmp(p->line,"response_config")) {'))
    (output/'demo/code/heartbeat_diag.h').write_text(HEADER, encoding='utf-8')
    for stem in ('speed_response', 'speed_response_download'):
        tree=ET.parse(baseline/(stem+'.uvprojx'))
        for element in tree.iter():
            if element.text and str(baseline).lower() in element.text.lower():
                element.text=element.text.replace(str(baseline),str(output))
        tree.write(output/(stem+'.uvprojx'),encoding='utf-8',xml_declaration=True)
        shutil.copy2(baseline/(stem+'.uvoptx'),output/(stem+'.uvoptx'))
    manifest={'version':VERSION,'baseline':str(baseline),'output':str(output),
              'baselineModifiedFilesSha256':source_hashes,'flashed':False,'buildPassed':False,
              'heartbeatTimeoutMs':500,'protectionsChanged':False,
              'limits':['Radio rx_h counts literal h bytes before field parsing, not verified heartbeat packets.',
                        'parsed_h counts top-level h offered to command dispatch; accepted_h counts active-motion lease writes.',
                        'Counters reset at explicit motion admission; snapshots survive later STOP and queries, not reset or next admission.',
                        'Query is stop-only through the existing field parser; no live query during motion.',
                        'Instrumentation changes timing; diagnostic evidence does not certify the uninstrumented image.']}
    (output/'candidate.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(manifest))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();prepare(args.baseline,args.output)
