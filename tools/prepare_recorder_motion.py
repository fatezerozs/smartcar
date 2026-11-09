"""Build a one-shot C26 motion experiment; never flash or access a serial port."""
from pathlib import Path
import datetime
import argparse
import hashlib
import json
import re
import subprocess
import xml.etree.ElementTree as E
from prepare_recorder_joint import ROOT, prepare, sha
from prepare_recorder_b3_static import once

POLICY = r'''#ifndef MOTION_POLICY_H
#define MOTION_POLICY_H
#include <stdint.h>
#define MOTION_LIMIT_MS 2900U
#define MOTION_PWM_CAP 2000U
typedef struct {uint32_t start;uint8_t armed,expired;} motion_policy_t;
static inline int motion_policy_arm(volatile motion_policy_t *p,uint32_t now){
 if(p->armed||p->expired)return 0;p->start=now;p->armed=1;return 1;
}
static inline int motion_policy_allowed(volatile motion_policy_t *p,uint32_t now,int on){
 return p->armed&&!p->expired&&on&&(uint32_t)(now-p->start)<MOTION_LIMIT_MS;
}
static inline uint32_t motion_policy_duty(volatile motion_policy_t *p,uint32_t now,int on,uint32_t request,int motor){
 if(!motion_policy_allowed(p,now,on))return 0;
 return motor&&request>MOTION_PWM_CAP?MOTION_PWM_CAP:request;
}
static inline int motion_policy_tick(volatile motion_policy_t *p,uint32_t now,int on){
 if(p->armed&&!p->expired&&(!on||(uint32_t)(now-p->start)>=MOTION_LIMIT_MS)){
  p->expired=1;return 1;
 }return 0;
}
#endif
'''

GUARD = r'''#include "main.h"
#include "joint.h"
#include "motion_policy.h"
extern volatile unsigned char recorder_finish_requested;
static volatile motion_policy_t policy;
static volatile uint32_t cut_ms,peak_motor;
void motion_arm(uint32_t now){(void)motion_policy_arm(&policy,now);}
uint32_t motion_duty(uint32_t pin,uint32_t request){
 int motor=pin==(uint32_t)MOTOR1_PWM||pin==(uint32_t)MOTOR2_PWM;
 if(!motor&&pin!=(uint32_t)SERVO_MOTOR_PWM)return request;
 /* PWM init precedes timer/GPIO initialization. Unarmed/expired output must
  * return zero without touching either peripheral. */
 if(!policy.armed||policy.expired)return 0;
 uint32_t duty=motion_policy_duty(&policy,timer_get(GPT_TIM_1),!gpio_get_level(C26),request,motor);
 if(motor&&duty>peak_motor)peak_motor=duty;
 return duty;
}
void motion_tick(void){
 uint32_t now=timer_get(GPT_TIM_1);
 if(motion_policy_tick(&policy,now,!gpio_get_level(C26))){
  cut_ms=now;joint_stop_requested=1;recorder_finish_requested=1;
  pwm_set_duty(MOTOR1_PWM,0);pwm_set_duty(MOTOR2_PWM,0);pwm_set_duty(SERVO_MOTOR_PWM,0);
 }
}
void motion_report(void){printf("MOTION LIMIT armed=%u expired=%u start_ms=%lu cut_ms=%lu limit_ms=2900 motor_cap=2000 motor_peak=%lu\r\n",policy.armed,policy.expired,(unsigned long)policy.start,(unsigned long)cut_ms,(unsigned long)peak_motor);}
'''

RADIO = r'''#include "main.h"
#include "app_link.h"
#include "task.h"
#include <stdio.h>
#include <stdarg.h>
static uint32_t radio_sequence;
static uint32_t radio_crc(const uint8_t *data,uint32_t n){
 uint32_t c=~0U;for(uint32_t i=0;i<n;++i){c^=data[i];for(unsigned j=0;j<8;++j)c=(c>>1)^((c&1U)?0xEDB88320U:0U);}return ~c;
}
static void radio_u32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8U*i));}
int motion_radio_write(const uint8_t *data,uint32_t length){
 uint32_t start=timer_get(GPT_TIM_1);
 if(length>APP_LINK_TX_SIZE)return 0;
 while(!app_link_send(APP_LINK_RADIO,data,length)){
  app_link_poll();
  if((uint32_t)(timer_get(GPT_TIM_1)-start)>=3000U)return 0;
  if(xTaskGetSchedulerState()==taskSCHEDULER_RUNNING)vTaskDelay(1);
 }
 app_link_poll();return 1;
}
int motion_printf(const char *format,...){
 char buffer[1024];va_list args;va_start(args,format);
 int n=vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);
 if(n<0||(unsigned)n>=sizeof(buffer))return -1;
 for(unsigned offset=0;offset<(unsigned)n;){
  uint8_t frame[110];unsigned length=(unsigned)n-offset;if(length>96U)length=96U;
  frame[0]='R';frame[1]='M';frame[2]='R';frame[3]='1';radio_u32(frame+4,radio_sequence++);
  frame[8]=(uint8_t)length;frame[9]=0;
  for(unsigned i=0;i<length;++i)frame[10+i]=(uint8_t)buffer[offset+i];
  radio_u32(frame+10+length,radio_crc(frame,10+length));
  if(!motion_radio_write(frame,14+length))return -1;
  offset+=length;
 }
 return n;
}
'''


def policy_test(folder):
    (folder / 'motion_policy.h').write_text(POLICY)
    harness = r'''#include "motion_policy.h"
#include <assert.h>
int main(void){
 volatile motion_policy_t p={0};
 assert(!motion_policy_duty(&p,0,1,10000,1));
 assert(motion_policy_arm(&p,100));assert(!motion_policy_arm(&p,200));
 assert(motion_policy_duty(&p,100,1,10000,1)==2000);
 assert(motion_policy_duty(&p,100,1,1500,1)==1500);
 assert(motion_policy_duty(&p,100,1,775,0)==775);
 assert(!motion_policy_duty(&p,100,0,10000,1));
 assert(motion_policy_allowed(&p,2999,1));
 assert(!motion_policy_duty(&p,3000,1,10000,1));
 assert(motion_policy_tick(&p,3000,1));assert(p.expired);
 assert(!motion_policy_arm(&p,3001));assert(!motion_policy_duty(&p,3001,1,10000,1));
 p=(motion_policy_t){0};assert(motion_policy_arm(&p,0xfffffff0U));
 assert(motion_policy_allowed(&p,0xfffffff0U+2899U,1));
 assert(motion_policy_tick(&p,0xfffffff0U+2900U,1));
 p=(motion_policy_t){0};assert(motion_policy_arm(&p,10));
 assert(motion_policy_tick(&p,11,0));assert(!motion_policy_duty(&p,12,1,10000,1));
 return 0;
}'''
    c = folder / 'policy-test.c'; c.write_text(harness)
    exe = folder / 'policy-test.exe'
    subprocess.run(['gcc', '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-Wno-misleading-indentation', str(c), '-o', str(exe)], check=True, timeout=60)
    subprocess.run([str(exe)], check=True, timeout=30)
    print('PASS motion policy: cap, deadline, OFF, one-shot and clock wrap', flush=True)


def guard_startup_test(folder):
    test = folder / 'guard-host'; test.mkdir()
    (test / 'main.h').write_text('''#include <stdint.h>
#include <stdio.h>
#define MOTOR1_PWM 1
#define MOTOR2_PWM 2
#define SERVO_MOTOR_PWM 3
#define GPT_TIM_1 0
#define C26 26
uint32_t timer_get(int);
int gpio_get_level(int);
void pwm_set_duty(int,uint32_t);
''')
    (test / 'joint.h').write_text('extern volatile unsigned char joint_stop_requested;\n')
    (test / 'test.c').write_text(r'''#include "main.h"
#include <assert.h>
void motion_arm(uint32_t);void motion_tick(void);uint32_t motion_duty(uint32_t,uint32_t);
volatile unsigned char joint_stop_requested,recorder_finish_requested;
static unsigned timer_reads,gpio_reads,writes;static uint32_t now;
uint32_t timer_get(int index){(void)index;++timer_reads;return now;}
int gpio_get_level(int pin){(void)pin;++gpio_reads;return 0;}
void pwm_set_duty(int pin,uint32_t request){assert(!motion_duty((uint32_t)pin,request));++writes;}
int main(void){
 assert(!motion_duty(MOTOR1_PWM,0));assert(!motion_duty(MOTOR2_PWM,10000));
 assert(!motion_duty(SERVO_MOTOR_PWM,775));assert(!timer_reads&&!gpio_reads);
 motion_arm(100);now=100;assert(motion_duty(MOTOR1_PWM,10000)==2000);
 assert(timer_reads&&gpio_reads);now=3000;motion_tick();
 assert(writes==3&&joint_stop_requested&&recorder_finish_requested);
 timer_reads=gpio_reads=0;assert(!motion_duty(MOTOR1_PWM,10000));
 assert(!timer_reads&&!gpio_reads);return 0;
}''')
    (test / 'guard.c').write_text(GUARD)
    exe = test / 'guard-test.exe'
    subprocess.run(['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',
                    '-I'+str(test),'-I'+str(folder),str(test/'guard.c'),str(test/'test.c'),'-o',str(exe)],check=True,timeout=60)
    subprocess.run([str(exe)],check=True,timeout=30)
    print('PASS actual motion guard: no timer/GPIO access before arm; independent cutoff and expiry',flush=True)


def build(radio=False):
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    out = ROOT / '.build/recorder-joint-20261106' / (('motion-radio-' if radio else 'motion-') + stamp)
    prepare(out)
    private = out / 'adapted'
    policy_test(private)
    guard_startup_test(private)
    (private / 'motion_guard.c').write_text(GUARD)
    (private / 'motion_guard.h').write_text('#include <stdint.h>\nvoid motion_arm(uint32_t);\nvoid motion_tick(void);\nvoid motion_report(void);\nuint32_t motion_duty(uint32_t,uint32_t);\n')
    project = out / 'candidate.uvprojx'; tree = E.parse(project)
    controls = tree.find('.//Cads/VariousControls')
    # Private motion header precedes the old static header search paths.
    controls.find('IncludePath').text = str(private) + ';' + controls.findtext('IncludePath')
    controls.find('MiscControls').text = controls.findtext('MiscControls').replace(
        (ROOT / 'demo/recorder_joint/include/joint_build.h').as_posix(),
        (private / 'motion_build.h').as_posix())
    header = (ROOT / 'demo/recorder_joint/include/joint_build.h').read_text().replace(
        'RECORDER_JOINT_STATIC_C26_1', 'RECORDER_MOTION_C26_3S_1').replace(
        'bounded_joint_static_window', 'bounded_motion_2900ms')
    if radio:
        controls.find('Define').text += ',VISION_DUAL_UART_ENABLE=1'
        header += '\n#include <stdio.h>\n#include "app_transport.h"\n#define printf app_console_printf\n'
    (private / 'motion_build.h').write_text(header + '\n#include "motion_guard.h"\n')
    for item in tree.findall('.//File'):
        source = Path(item.findtext('FilePath'))
        if source.name not in {'joint_main.c', 'joint_metrics.c', 'vision_drive.c', *(['b2_export.c'] if radio else [])}:
            continue
        text = source.read_bytes().decode('latin1')
        if source.name == 'joint_main.c':
            text = text.replace('JOINT ', 'MOTION ').replace('RECORDER_JOINT_STATIC_C26_1', 'RECORDER_MOTION_C26_3S_1')
            text = text.replace('forced_zero', 'bounded_2900ms_pwm2000').replace('frames_limit=100', 'frames_limit=200').replace('record_soft_limit_ms=3000', 'record_soft_limit_ms=4000')
            text = once(text, 'phase=2;writer_begin=now;joint_writer_start=1;printf("MOTION BEGIN phase=2 recorder=ON C26_ON',
                        'phase=2;writer_begin=now;motion_arm(now);joint_writer_start=1;printf("MOTION BEGIN phase=2 recorder=ON C26_ON')
            # There is no OFF-mode RUN path in a powered experiment.
            text = text.replace('!strcmp(line.text,"MOTION RUN")||!strcmp(line.text,"MOTION ARM")', '!strcmp(line.text,"MOTION ARM")')
            text = once(text, '(now-writer_begin)>=3000U', '(now-writer_begin)>=4000U')
            text = once(text, '   printf("MOTION END ', '   motion_report();printf("MOTION END ')
            if radio:
                # Reuse main's owner/STOP/error parser and output queue verbatim.
                # @ keeps STATUS/START letters out of its single-key STOP path.
                text=once(text,'if(app_link_take_error(APP_LINK_USB)){line->used=0;line->discard=1;joint_stop_requested=1;}',
                          'app_transport_begin(!recorder_stopped_drive);')
                text=once(text,'app_link_read(APP_LINK_USB,&data,1)','app_transport_read(&data,1)')
                text=once(text,'app_link_poll();return ready;',
                          'if(app_transport_stopped()||app_transport_changed()){joint_stop_requested=1;line->used=0;line->discard=1;ready=0;}\n'
                          ' if(ready&&line->text[0]==\'@\')memmove(line->text,line->text+1,strlen(line->text));\n'
                          ' app_transport_poll();return ready;')
                text=once(text,'debug_init();app_link_init();','debug_init();app_transport_init();')
                text = once(text,'MOTION id=RECORDER_MOTION_C26_3S_1 outputs=',
                            'MOTION id=RECORDER_MOTION_C26_3S_1 transport=main_radio115200 outputs=')
        elif source.name == 'joint_metrics.c':
            text = text.replace('JOINT ', 'MOTION ')
            text = once(text, 'void b3_pit_sample(void)\n{', 'void b3_pit_sample(void)\n{\n motion_tick();')
        elif source.name == 'vision_drive.c':
            text = once(text, 'VISION_ROUTE_SWITCH?MT_ROUTE_FAST_TICKS:40', 'VISION_ROUTE_SWITCH?20U:20U')
        else:
            text = once(text,'LPUART_WriteBlocking(LPUART1,bytes,n);',
                        'app_transport_write(bytes,(uint32_t)n);')
            # The RF module has a fixed UART baud. Never change just the MCU side.
            text = once(text,'if(baud!=115200&&baud!=921600)', 'if(baud!=115200)')
            text = text.replace('LPUART1','LPUART8')
        target = private / source.name; target.write_bytes(text.encode('latin1'))
        item.find('FilePath').text = str(target)
    pwm = private / 'zf_driver_pwm.c'; text = pwm.read_bytes().decode('latin1')
    call = 'b3_static_duty((uint32)pin,b3_requested,(uint32)MOTOR1_PWM,(uint32)MOTOR2_PWM,(uint32)SERVO_MOTOR_PWM)'
    assert text.count(call) == 2
    pwm.write_bytes(text.replace(call, 'motion_duty((uint32)pin,b3_requested)').encode('latin1'))
    capture = private / 'recorder_capture.c'; text = capture.read_text()
    text = once(text, 'counters.accepted<100U', 'counters.accepted<200U')
    text = once(text, 'counters.accepted>=100U', 'counters.accepted>=200U'); capture.write_text(text)
    files = tree.find('.//Group/Files')
    item = E.SubElement(files, 'File')
    for key, value in [('FileName', 'motion_guard.c'), ('FileType', '1'), ('FilePath', str(private / 'motion_guard.c'))]:
        E.SubElement(item, key).text = value
    tree.find('.//TargetName').text = 'RECORDER_MOTION_C26_3S_1'
    tree.find('.//TargetCommonOption/OutputName').text = 'recorder_motion_c26_3s'
    E.indent(tree, space='  '); tree.write(project, encoding='utf-8', xml_declaration=True)
    uv4 = 'E:/ElecTronic/Keil/UV4/UV4.exe'
    def compile_one():
        log = out / 'keil-build.log'
        subprocess.run([uv4, '-b', str(project), '-j0', '-o', str(log)],
                       check=True, timeout=180, creationflags=subprocess.CREATE_NO_WINDOW)
        assert '0 Error(s), 0 Warning(s)' in log.read_text(errors='replace'), str(log)
    print('Building', project, flush=True)
    compile_one()
    sources = {str(project.relative_to(ROOT)): sha(project),
               'tools/prepare_recorder_motion.py': sha(Path(__file__))}
    for f in tree.findall('.//File'):
        p = Path(f.findtext('FilePath')); sources[str(p.relative_to(ROOT))] = sha(p)
    for p in private.glob('*.h'):
        if p.name not in ['recorder_identity.h', 'joint_identity.h']:
            sources[str(p.relative_to(ROOT))] = sha(p)
    for dep in (out / 'Objects').glob('*.d'):
        for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b', dep.read_text(errors='replace')):
            p = Path(token).resolve()
            if p.name in ['recorder_identity.h', 'joint_identity.h']: continue
            sources[str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)] = sha(p)
    for entry in tree.findall('.//ScatterFile'):
        p = Path(entry.text); sources[str(p.relative_to(ROOT))] = sha(p)
    digest = hashlib.sha256(json.dumps(sources, sort_keys=True).encode()).hexdigest()
    (private / 'joint_identity.h').write_text('#define JOINT_INPUT_DIGEST "' + digest + '"\n')
    (private / 'recorder_identity.h').write_text('#define RECORDER_TREE_SHA256 "' + digest + '"\n#define RECORDER_GIT_SHA "dirty-local"\n#define RECORDER_GIT_DIRTY 1\n')
    compile_one()
    assert all(sha(ROOT / p) == h for p, h in sources.items())
    axf = out / 'Objects/recorder_motion_c26_3s.axf'; binary = out / 'Objects/recorder_motion_c26_3s.bin'
    subprocess.run(['E:/ElecTronic/Keil/ARM/ARMCLANG/bin/fromelf.exe', '--bincombined', '--output', str(binary), str(axf)], check=True, timeout=30)
    map_text = (out / 'Listings/recorder_motion_c26_3s.map').read_text(errors='replace')
    for name in ['motion_duty', 'motion_arm', 'motion_tick', 'joint_control_task', 'vd_route_switch_poll']:
        assert re.search(r'^\s+' + name + r'\s+0x[0-9a-f]+\s+Thumb Code', map_text, re.M), name
    for used, maximum in re.findall(r'Execution Region \w+ \(.*?Size: (0x[0-9a-fA-F]+), Max: (0x[0-9a-fA-F]+)', map_text):
        assert int(used,16) <= int(maximum,16)
    evidence = {'role':'AUTHORIZED_EXPERIMENT_NOT_VALIDATED', 'source_digest':digest, 'sources':sources,
                'limit_ms':2900, 'motor_pwm_cap_permille':200, 'max_speed_ticks_per_10ms':20,
                'C26_OFF_cuts_outputs':True,'one_shot_until_reset':True,'visual_protections_preserved':True,
                'PI_effectiveness':'USER_WAIVED_NOT_MEASURED_PASS', 'start_requires':'MOTION START then MOTION ARM while OFF then physical C26 ON',
                'transport':'main_radio115200' if radio else 'usb115200',
                'build_passed':True,'hardware_tested':False,'axf_sha256':sha(axf),'bin_sha256':sha(binary)}
    (out / 'motion-evidence.json').write_text(json.dumps(evidence,indent=2))
    print('PASS motion candidate:', out, flush=True)
    print('SOURCE',digest,flush=True)
    return out


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--radio',action='store_true')
    build(parser.parse_args().radio)
