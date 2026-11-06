"""Validate the primary-derived forced-zero static prerequisite, not B3 motion."""
from pathlib import Path
import re,json,hashlib,argparse
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def verify(output):
 out=Path(output).resolve();assert out.is_relative_to(ROOT/'.build/recorder-b3-20261106')
 proof=json.loads((out/'inputs.json').read_text());text=(out/'Listings/recorder_b3_static.map').read_text(errors='replace');log=(out/'keil-build.log').read_text()
 assert '0 Error(s), 0 Warning(s)' in log
 for n in ['vision_drive_poll','vision_drive_observe','vd_route_switch_poll','vd_speed_control_poll','vd_speed_sample_tick','pwm_set_duty','pwm_init','b3_poll','b3_filter']:
  assert re.search(r'^\s+'+n+r'\s+0x[0-9a-f]+\s+Thumb Code',text,re.M),n
 for n in ['midterm_boot_poll','recorder_control_task','recorder_storage_task','vTaskStartScheduler','SD_Init','SD_WriteBlocks','qsort']:
  assert not re.search(r'^\s+'+n+r'\s+0x[0-9a-f]+\s+Thumb Code',text,re.M),n
 assert re.search(r'^\s+pwm_set_duty\s+0x[0-9a-f]+\s+Thumb Code\s+\d+\s+zf_driver_pwm.o',text,re.M)
 adapted=(out/'adapted/zf_driver_pwm.c').read_text(encoding='latin1');assert adapted.count('const uint32 duty=b3_static_duty')==2
 assert all(sha(ROOT/p)==h for p,h in proof['sources'].items())
 assert sha(ROOT/'demo/mdk/competition/MPC_20261106.uvprojx')==proof['original_project_sha256']
 missing=set()
 for d in (out/'Objects').glob('*.d'):
  for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b',d.read_text(errors='replace')):
   p=Path(token).resolve()
   if p.name=='b3_identity.h':continue
   key=str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)
   if key not in proof['sources']:missing.add(key)
 assert not missing,missing
 regions=[{'name':m[1],'base':m[2],'used':int(m[3],16),'maximum':int(m[4],16)} for m in re.finditer(r'Execution Region (\w+) \(Exec base: (0x[0-9a-fA-F]+), Load base: 0x[0-9a-fA-F]+, Size: (0x[0-9a-fA-F]+), Max: (0x[0-9a-fA-F]+)',text)]
 files=[out/'Objects/recorder_b3_static.axf',out/'Objects/recorder_b3_static.bin',out/'Listings/recorder_b3_static.map',out/'keil-build.log',out/'inputs.json']
 record={'role':'B3 prerequisite static baseline','keil_zero_errors_warnings':True,'c26_route_owner_reused':True,'autonomous_boot_owner_absent':True,'no_sd_rtos_runtime':True,'forced_zero_private_driver_both_init_set':True,'original_production_project_unchanged':True,'frozen_dependencies_match':True,'input_files':len(proof['sources']),'input_digest':proof['manifest_digest'],'regions':regions,'hashes':{str(p.relative_to(ROOT)):sha(p) for p in files},'hardware_tested':False,'B3_motion_accepted':False}
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2));print(json.dumps(record,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output');a=p.parse_args();verify(a.output)
