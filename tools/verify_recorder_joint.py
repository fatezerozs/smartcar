"""Build and source isolation evidence only; never hardware acceptance."""
from pathlib import Path
import re,json,hashlib,argparse
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(directory):
 out=Path(directory).resolve();assert out.is_relative_to(ROOT/'.build/recorder-joint-20261106')
 proof=json.loads((out/'inputs.json').read_text());text=(out/'Listings/recorder_joint_static.map').read_text(errors='replace')
 assert '0 Error(s), 0 Warning(s)' in (out/'keil-build.log').read_text()
 def linked(n):return bool(re.search(r'^\s+'+re.escape(n)+r'\s+0x[0-9a-f]+\s+Thumb Code',text,re.M))
 for name in ['vision_drive_poll','vision_drive_observe','vd_route_switch_poll','vd_speed_sample_tick','vd_speed_control_poll','pwm_init','pwm_set_duty','recorder_capture_irq','recorder_capture_task','recorder_vision_task','recorder_storage_task','joint_control_task','vTaskStartScheduler','SD_Init','SD_WriteBlocks','f_write','f_sync','SVC_Handler','PendSV_Handler','SysTick_Handler']:assert linked(name),name
 for name in ['midterm_boot_poll','recorder_control_task','app_runtime_run','qsort']:assert not linked(name),name
 assert all(sha(ROOT/key)==value for key,value in proof['sources'].items())
 assert sha(ROOT/'demo/mdk/competition/MPC_20261106.uvprojx')==proof['original_project_sha256']
 pwm=(out/'adapted/zf_driver_pwm.c').read_text(encoding='latin1');assert pwm.count('const uint32 duty=b3_static_duty')==2
 capture=(out/'adapted/recorder_capture.c').read_text();assert 'counters.accepted<100U' in capture and 'counters.accepted>=100U' in capture
 storage=(out/'adapted/recorder_storage.c').read_text();assert 'while(!joint_writer_start)' in storage
 missing=set()
 for dep in (out/'Objects').glob('*.d'):
  for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b',dep.read_text(errors='replace')):
   p=Path(token).resolve()
   if p.name in ['joint_identity.h','recorder_identity.h']:continue
   key=str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)
   if key not in proof['sources']:missing.add(key)
 assert not missing,missing
 regions=[{'name':m[1],'used':int(m[2],16),'maximum':int(m[3],16)} for m in re.finditer(r'Execution Region (\w+) \(Exec base: 0x[0-9a-fA-F]+, Load base: 0x[0-9a-fA-F]+, Size: (0x[0-9a-fA-F]+), Max: (0x[0-9a-fA-F]+)',text)]
 assert regions and all(r['used']<=r['maximum'] for r in regions)
 files=[out/'candidate.uvprojx',out/'Objects/recorder_joint_static.axf',out/'Objects/recorder_joint_static.bin',out/'Listings/recorder_joint_static.map',out/'keil-build.log',out/'inputs.json']
 result={'role':'C26 Camera async Writer stationary combined candidate','build_passed':True,'warnings':0,'primary_project_unchanged':True,'C26_and_PI_linked':True,'autonomous_boot_absent':True,'forced_zero_both_driver_entrypoints':True,'producer_hard_frame_cap':100,'writer_deferred_until_operator_run':True,'frozen_inputs':len(proof['sources']),'source_digest':proof['manifest_digest'],'regions':regions,'hashes':{str(p.relative_to(ROOT)):sha(p) for p in files},'hardware_joint_regression_passed':False,'motion_accepted':False}
 (out/'build-evidence.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory');a=p.parse_args();verify(a.directory)
