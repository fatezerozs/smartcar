"""Clone the current C26 project with private diagnostic adapters only.

No original controller/profile/project edits. No SD or RTOS added. The static
driver adapter forces all three actuator duties to zero in init and set paths.
"""
from pathlib import Path
import argparse,hashlib,json,re,subprocess,xml.etree.ElementTree as E
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def mask_code(s):
 return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',lambda m:' '*len(m[0]),s,flags=re.S)
def function(s,name,change):
 m=re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{',mask_code(s));assert m,name
 opening=s.index('{',m.start());masked=mask_code(s);depth=1;end=opening+1
 while depth:
  depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
 block=s[m.start():end];return s[:m.start()]+change(block)+s[end:]
def once(s,a,b):assert s.count(a)==1,(a,s.count(a));return s.replace(a,b)
def add_end(block,code):return block[:-1]+code+'\n}'
def adapt(name,s):
 s=s.replace('\r\n','\n');s='#include "b3_trace.h"\n'+s
 if name=='main.c':s=once(s,'timer_start(GPT_TIM_1);','timer_start(GPT_TIM_1);b3_init();')
 elif name=='isr.c':s=once(s,'vision_drive_safety_tick();','vision_drive_safety_tick();b3_pit_sample();')
 elif name=='app_mode.c':s=function(s,'app_mode_sample',lambda b:add_end(b,'b3_mode(a,b,raw_mode,selected_mode,settled,revision);'))
 elif name=='board_encoder.c':s=function(s,'board_encoder_read',lambda b:add_end(b,'b3_encoder_read(*left,*right);'))
 elif name=='app_runtime.c':
  s=once(s,'uint32_t n = app_transport_read(commands, sizeof(commands));','uint32_t n = app_transport_read(commands, sizeof(commands));\n        n=b3_filter(commands,n);')
  s=once(s,'        vision_drive_poll(now);','        vision_drive_poll(now);b3_poll();')
  statement='vision_drive_observe(&result, stamp.sequence, stamp.capture_ms, timer_get(GPT_TIM_1));'
  s=once(s,statement,'{'+statement+'b3_vision(stamp.sequence,stamp.capture_ms);}')
 elif name=='vision_speed.c':
  def pi(b):
   pos=b.index('{')+1;b=b[:pos]+'\n uint32_t b3_mask_start=0,b3_mask_cycles=0,b3_consumed_snapshot=0;vd_speed_info_t b3_speed_snapshot;b3_pi_begin(now);\n'+b[pos:]
   b=b.replace('__disable_irq();','__disable_irq();b3_mask_start=DWT->CYCCNT;')
   b=b.replace('__set_PRIMASK(mask);','b3_speed_snapshot=speed;b3_consumed_snapshot=consumed;b3_mask_cycles=DWT->CYCCNT-b3_mask_start;__set_PRIMASK(mask);')
   b=b.replace('return;','b3_pi_end(0,b3_mask_cycles,&b3_speed_snapshot,b3_consumed_snapshot);return;')
   return add_end(b,'b3_pi_end(1,b3_mask_cycles,&b3_speed_snapshot,b3_consumed_snapshot);')
  s=function(s,'vd_speed_control_poll',pi)
 elif name=='zf_driver_pwm.c':
  s='#include "main.h"\n'+s
  def pwm(b):
   b=once(b,'const uint32 duty','const uint32 b3_requested');pos=b.index('{')+1
   b=b[:pos]+'\n const uint32 duty=b3_static_duty((uint32)pin,b3_requested,(uint32)MOTOR1_PWM,(uint32)MOTOR2_PWM,(uint32)SERVO_MOTOR_PWM);\n'+b[pos:]
   return add_end(b,'b3_pwm_write((uint32)pin,b3_requested,duty,PWMPTR[pwm_num]->SM[pwm_module].VAL2,PWMPTR[pwm_num]->SM[pwm_module].VAL3,PWMPTR[pwm_num]->SM[pwm_module].VAL4,PWMPTR[pwm_num]->SM[pwm_module].VAL5,PWMPTR[pwm_num]->MCTRL);')
  s=function(s,'pwm_set_duty',pwm);s=function(s,'pwm_init',pwm)
 else:raise ValueError(name)
 return s
def prepare(output,dependencies_from=None):
 out=Path(output).resolve();allowed=ROOT/'.build/recorder-b3-20261106';assert out.is_relative_to(allowed) and out!=allowed and not out.exists()
 original=ROOT/'demo/mdk/competition/MPC_20261106.uvprojx';tree=E.parse(original);target=tree.find('.//Target');defines=target.findtext('.//Cads/VariousControls/Define')
 for n,v in [('VISION_MIDTERM_AUTONOMOUS','0'),('VISION_ROUTE_SWITCH','1'),('VISION_MIDTERM_ROUTE_TRIAL','1'),('VISION_ENCODER_ENABLE','1')]:assert re.search(r'(?:^|,)\s*'+n+'='+v+r'(?:,|$)',defines)
 out.mkdir(parents=True);private=out/'adapted';private.mkdir();(out/'Objects').mkdir();(out/'Listings').mkdir()
 names={'main.c','isr.c','app_runtime.c','app_mode.c','board_encoder.c','vision_speed.c','zf_driver_pwm.c'};provenance={};inputs={str(original.relative_to(ROOT)):sha(original)}
 for f in target.findall('.//File'):
  path=(original.parent/f.findtext('FilePath').replace('\\','/')).resolve();assert path.is_file(),path
  inputs[str(path.relative_to(ROOT))]=sha(path)
  if path.name in names:
   generated=private/path.name;generated.write_bytes(adapt(path.name,path.read_bytes().decode('latin1')).encode('latin1'))
   provenance[path.name]={'original':str(path.relative_to(ROOT)),'original_sha256':sha(path),'generated_sha256':sha(generated)};path=generated
  f.find('FilePath').text=str(path)
 assert set(provenance)==names
 for e in target.findall('.//IncludePath'):
  if e.text:e.text=';'.join(map(str,[ROOT/'demo/recorder_b3/include',private,*[(original.parent/p.strip().replace('\\','/')).resolve() for p in e.text.split(';') if p.strip()]]))
 for e in target.findall('.//ScatterFile'):
  p=(original.parent/e.text.replace('\\','/')).resolve();inputs[str(p.relative_to(ROOT))]=sha(p);e.text=str(p)
 target.find('TargetName').text='RECORDER_B3_STATIC_C26_1'
 common=target.find('.//TargetCommonOption');common.find('OutputDirectory').text=str(out/'Objects')+'\\';common.find('ListingPath').text=str(out/'Listings')+'\\';common.find('OutputName').text='recorder_b3_static';common.find('CreateHexFile').text='0'
 for hook in ['BeforeCompile','BeforeMake','AfterMake']:
  for e in common.find(hook):
   if e.tag.startswith('Run') or e.tag.startswith('nStop'):e.text='0'
   elif 'Name' in e.tag:e.text=''
 target.find('.//UpdateFlashBeforeDebugging').text='0'
 c=target.find('.//Cads/VariousControls');c.find('Define').text=defines+',B3_STATIC_C26=1'
 c.find('MiscControls').text=(c.findtext('MiscControls') or '')+' -fno-common -fdata-sections -ffunction-sections -include "'+(ROOT/'demo/recorder_b3/include/b3_build.h').as_posix()+'"'
 g=E.SubElement(target.find('Groups'),'Group');E.SubElement(g,'GroupName').text='B3_STATIC_RAM_TRACE';files=E.SubElement(g,'Files')
 for p in [ROOT/'demo/recorder_b3/b3_trace.c',ROOT/'demo/recorder/recorder_core.c']:
  f=E.SubElement(files,'File');E.SubElement(f,'FileName').text=p.name;E.SubElement(f,'FileType').text='1';E.SubElement(f,'FilePath').text=str(p);inputs[str(p.relative_to(ROOT))]=sha(p)
 crc_header=ROOT/'demo/recorder/include/recorder_core.h';(private/'recorder_core.h').write_bytes(crc_header.read_bytes());inputs[str(crc_header.relative_to(ROOT))]=sha(crc_header)
 for p in private.iterdir():
  if p.suffix in ('.c','.h'):inputs[str(p.relative_to(ROOT))]=sha(p)
 for p in (ROOT/'demo/recorder_b3/include').glob('*.h'):inputs[str(p.relative_to(ROOT))]=sha(p)
 inputs['tools/prepare_recorder_b3_static.py']=sha(Path(__file__))
 if dependencies_from:
  seed=Path(dependencies_from).resolve();assert seed.is_relative_to(allowed)
  for dep in (seed/'Objects').glob('*.d'):
   for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b',dep.read_text(errors='replace')):
    p=Path(token).resolve()
    if p.is_relative_to(seed/'adapted'):p=private/p.relative_to(seed/'adapted')
    if p.name=='b3_identity.h':continue
    assert p.is_file(),p;key=str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p);inputs[key]=sha(p)
 digest=hashlib.sha256(json.dumps({'files':inputs,'defines':c.findtext('Define'),'flags':c.findtext('MiscControls')},sort_keys=True).encode()).hexdigest()
 (private/'b3_identity.h').write_text('#define B3_INPUT_DIGEST "'+digest+'"\n')
 project=ROOT/'demo/recorder_b3/mdk/recorder_b3_static.uvprojx';project.parent.mkdir(exist_ok=True);E.indent(tree,space='  ');tree.write(project,encoding='utf-8',xml_declaration=True)
 (out/'inputs.json').write_text(json.dumps({'sources':inputs,'private_adapters':provenance,'manifest_digest':digest,'defines':c.findtext('Define'),'original_project_sha256':sha(original),'project_sha256':sha(project),'role':'static prerequisite, forced-zero actuator adapter, no motion authorization or SD runtime','dependency_seed':str(dependencies_from)},indent=2));print(project)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);p.add_argument('--dependencies-from');a=p.parse_args();prepare(a.output,a.dependencies_from)
