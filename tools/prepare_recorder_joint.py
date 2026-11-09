"""Isolated zero-output C26 + Camera + asynchronous Recorder candidate."""
from pathlib import Path
import argparse, hashlib, json, re, shutil, xml.etree.ElementTree as E
from prepare_recorder_b3_static import adapt, function, once
ROOT=Path(__file__).resolve().parents[1]
ALLOWED=ROOT/'.build/recorder-joint-20261106'
SD_NAMES=['zf_device_mt9v03x.c','fsl_sdmmc_host.c','fsl_sdmmc_host.h','sdmmc_config.c','fsl_sd.c','fsl_usdhc.c','ff.c','ff.h','ffconf.h','diskio.c','diskio.h','ffunicode.c']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def paths(project):
 return [(project.parent/f.findtext('FilePath').replace('\\','/')).resolve() for f in E.parse(project).findall('.//File')]
def prepare(output,seed=None):
 out=Path(output).resolve();assert out.is_relative_to(ALLOWED) and out!=ALLOWED and not out.exists()
 original=ROOT/'demo/mdk/competition/MPC_20261106.uvprojx';b2=ROOT/'demo/recorder_b2/mdk/recorder_b2.uvprojx'
 tree=E.parse(original);t=tree.find('.//Target');c=t.find('.//Cads/VariousControls');defines=c.findtext('Define')
 for n,v in [('VISION_ENCODER_ENABLE','1'),('VISION_MIDTERM_AUTONOMOUS','0'),('VISION_MIDTERM_ROUTE_TRIAL','1'),('VISION_ROUTE_SWITCH','1')]:assert re.search(r'(?:^|,)\s*'+n+'='+v+r'(?:,|$)',defines)
 out.mkdir(parents=True);private=out/'adapted';private.mkdir();(out/'Objects').mkdir();(out/'Listings').mkdir()
 origin=ROOT/'.build/recorder-train/adapted';proof=json.loads((origin/'provenance.json').read_text())
 # Reuse only SD/Camera adapters. C26 control and board IO use the current
 # primary sources, not the old training adapters from this provenance file.
 for name,h in proof['baseline_sha256'].items():
  if Path(name).name in SD_NAMES:assert sha(ROOT/name)==h,name
 for name in SD_NAMES:assert sha(origin/name)==proof['generated_sha256'][name],name
 inputs={str(original.relative_to(ROOT)):sha(original),str(b2.relative_to(ROOT)):sha(b2)};adapters={}
 for name in SD_NAMES:shutil.copyfile(origin/name,private/name);inputs[str((origin/name).relative_to(ROOT))]=sha(origin/name)
 sources=[];skip={'main.c','app_runtime.c','vision_debug.c','midterm_boot.c','fsl_sd_disk.c','fsl_sdmmc_osa.c','zf_common_vector.c'};instrument={'isr.c','app_mode.c','board_encoder.c','vision_speed.c','zf_driver_pwm.c'}
 for p in paths(original):
  if p.suffix.lower() not in ('.c','.s','.lib'):continue
  if p.name in skip:continue
  inputs[str(p.relative_to(ROOT))]=sha(p)
  if p.name in SD_NAMES:p=private/p.name
  elif p.name in instrument:
   text=adapt(p.name,p.read_bytes().decode('latin1'))
   if p.name=='isr.c':
    text=function(text,'SysTick_Handler',lambda b:'joint_unused_systick(void){}')
    text=once(text,'CSI_DriverIRQHandler();','recorder_capture_irq();')
    text='#include "recorder_runtime.h"\n'+text+'\nvoid USDHC1_IRQHandler(void){extern void USDHC1_DriverIRQHandler(void);USDHC1_DriverIRQHandler();}\n'
   generated=private/p.name;generated.write_bytes(text.encode('latin1'));adapters[p.name]={'original':str(p.relative_to(ROOT)),'before_sha256':sha(p),'generated_sha256':sha(generated)};p=generated
  sources.append(p)
 # Add only the reviewed SD and RTOS sources missing from primary.
 mapping={p.name:p for p in paths(b2)}
 extra=[private/n for n in SD_NAMES if n.endswith('.c')]
 extra += [mapping[n] for n in ['tasks.c','queue.c','list.c','port.c']]
 extra += [ROOT/'demo/recorder'/n for n in ['recorder_core.c','recorder_capture.c','recorder_storage.c','recorder_disk.c','recorder_sd_pins.c','recorder_sd_osa.c']]
 extra += [ROOT/'demo/recorder_b2/b2_export.c']+[ROOT/'demo/recorder_joint'/n for n in ['joint_main.c','joint_control.c','joint_metrics.c']]
 for p in extra:
  if p.name=='recorder_storage.c':
   inputs[str(p.relative_to(ROOT))]=sha(p)
   text=p.read_text();text=once(text,'    recorder_storage_state=1;','    while(!joint_writer_start)vTaskDelay(1);\n    recorder_storage_state=1;')
   generated=private/p.name;generated.write_text('#include "joint.h"\n'+text);adapters[p.name]={'original':str(p.relative_to(ROOT)),'before_sha256':sha(p),'generated_sha256':sha(generated)};p=generated
  elif p.name=='recorder_capture.c':
   inputs[str(p.relative_to(ROOT))]=sha(p);text=p.read_text()
   text=once(text,'if(ack && seq>counters.base_source) {','if(ack && seq>counters.base_source && counters.accepted<100U) {')
   text=once(text,'++counters.accepted;','++counters.accepted;if(counters.accepted>=100U)recorder_finish_requested=1;')
   # Vision continues after the finite recording window. Count only processed
   # frames whose sequence belongs to that window, including while draining.
   text=once(text,'if(ack && vision[at].sequence>counters.base_source)',
             'if(ack && vision[at].sequence>counters.base_source && vision[at].sequence<=counters.last_source)')
   text=once(text,'if(depth>counters.queue_high_water)','joint_queue_depth(depth);if(depth>counters.queue_high_water)')
   text=once(text,'*frame=&recording[*at];return 1;','*frame=&recording[*at];joint_queue_depth(uxQueueMessagesWaiting(filled_queue));return 1;')
   generated=private/p.name;generated.write_text('#include "joint.h"\nextern volatile unsigned char recorder_finish_requested;\n'+text);adapters[p.name]={'original':str(p.relative_to(ROOT)),'before_sha256':sha(p),'generated_sha256':sha(generated)};p=generated
  matching=[x for x in sources if x.name==p.name]
  if matching:assert matching==[p],(matching,p)
  else:sources.append(p)
 includes=[ROOT/'demo/recorder_joint/include',ROOT/'demo/recorder_b3/include',private,ROOT/'demo/recorder_b2/include',ROOT/'demo/recorder/include']
 for project in [original,b2]:
  for e in E.parse(project).findall('.//Cads/VariousControls/IncludePath'):
   for value in e.text.split(';'):
    if value.strip():
     p=(project.parent/value.strip().replace('\\','/')).resolve()
     if p==origin or 'recorder-camera-stages-20261106' in str(p):continue
     if p not in includes:includes.append(p)
 c.find('IncludePath').text=';'.join(map(str,includes))
 c.find('Define').text=defines+',RECORDER_TRAINING=1,MIDTERM_VISUAL_REVIEWED=0,SDK_DEBUGCONSOLE=0'
 c.find('MiscControls').text='-fno-common -fdata-sections -ffunction-sections -ffreestanding -fno-builtin -mthumb -Wno-invalid-source-encoding -include "'+(ROOT/'demo/recorder_joint/include/joint_build.h').as_posix()+'"'
 common=t.find('.//TargetCommonOption');common.find('OutputDirectory').text=str(out/'Objects')+'\\';common.find('ListingPath').text=str(out/'Listings')+'\\';common.find('OutputName').text='recorder_joint_static';common.find('CreateHexFile').text='0';t.find('TargetName').text='RECORDER_JOINT_STATIC_C26_1'
 for hook in ['BeforeCompile','BeforeMake','AfterMake']:
  for e in common.find(hook):
   if e.tag.startswith('Run') or e.tag.startswith('nStop'):e.text='0'
   elif 'Name' in e.tag:e.text=''
 t.find('.//UpdateFlashBeforeDebugging').text='0'
 for e in t.findall('.//ScatterFile'):
  p=(original.parent/e.text.replace('\\','/')).resolve();e.text=str(p);inputs[str(p.relative_to(ROOT))]=sha(p)
 groups=t.find('Groups');groups.clear();g=E.SubElement(groups,'Group');E.SubElement(g,'GroupName').text='C26_CAMERA_WRITER_FORCED_ZERO';files=E.SubElement(g,'Files')
 for p in sources:
  assert p.is_file(),p;f=E.SubElement(files,'File');E.SubElement(f,'FileName').text=p.name;E.SubElement(f,'FileType').text='2' if p.suffix=='.S' else '4' if p.suffix=='.lib' else '1';E.SubElement(f,'FilePath').text=str(p);inputs[str(p.relative_to(ROOT))]=sha(p)
 for directory in [ROOT/'demo/recorder_joint/include',ROOT/'demo/recorder_b3/include',ROOT/'demo/recorder_b2/include',private]:
  for p in directory.glob('*.h'):inputs[str(p.relative_to(ROOT))]=sha(p)
 inputs[str(Path(__file__).relative_to(ROOT))]=sha(Path(__file__))
 if seed:
  seed=Path(seed).resolve();assert seed.is_relative_to(ALLOWED)
  for dep in (seed/'Objects').glob('*.d'):
   for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b',dep.read_text(errors='replace')):
    p=Path(token).resolve()
    if p.is_relative_to(seed/'adapted'):p=private/p.relative_to(seed/'adapted')
    if p.name in ['joint_identity.h','recorder_identity.h']:continue
    assert p.is_file(),p;inputs[str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)]=sha(p)
 digest=hashlib.sha256(json.dumps({'inputs':inputs,'defines':c.findtext('Define'),'flags':c.findtext('MiscControls')},sort_keys=True).encode()).hexdigest()
 (private/'joint_identity.h').write_text('#define JOINT_INPUT_DIGEST "'+digest+'"\n')
 (private/'recorder_identity.h').write_text('#define RECORDER_TREE_SHA256 "'+digest+'"\n#define RECORDER_GIT_SHA "dirty-local"\n#define RECORDER_GIT_DIRTY 1\n')
 E.indent(tree,space='  ');project=out/'candidate.uvprojx';tree.write(project,encoding='utf-8',xml_declaration=True)
 (out/'inputs.json').write_text(json.dumps({'sources':inputs,'adapters':adapters,'manifest_digest':digest,'project_sha256':sha(project),'original_project_sha256':sha(original),'dependency_seed':str(seed),'role':'stationary forced zero; C26 primary owner; bounded async Writer'},indent=2));print(project)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);p.add_argument('--dependencies-from');a=p.parse_args();prepare(a.output,a.dependencies_from)
