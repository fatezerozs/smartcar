"""Read-only MAP/input/isolation validation of the stationary B2 candidate."""
from pathlib import Path
import re,json,hashlib,argparse
ROOT=Path(__file__).resolve().parents[1]
def verify(output):
 out=Path(output).resolve();assert out.is_relative_to(ROOT/'.build/recorder-camera-stages-20261106')
 mp=out/'Listings/recorder_b2.map';s=mp.read_text(errors='replace');assert '0 Error(s), 0 Warning(s)' in (out/'keil-build.log').read_text()
 for n in ['pwm_init','pwm_set_duty','board_encoder_init','vision_drive_poll','recorder_control_task','recorder_link_task','midterm_boot_poll','app_runtime_run']:
  assert not re.search(r'^\s+'+n+r'\s+0x[0-9a-f]+\s+Thumb Code',s,re.M),n
 for n in ['recorder_capture_task','recorder_storage_task','monitor']:assert re.search(r'^\s+'+n+r'\s+0x[0-9a-f]+\s+Thumb Code',s,re.M),n
 for n in ['SysTick_Handler','SVC_Handler','PendSV_Handler']:assert re.search(r'^\s+'+n+r'\s+0x[0-9a-f]+\s+Thumb Code\s+\d+\s+port.o',s,re.M),n
 buffers={}
 for name in ['raw','recording','bounce','g_sd','s_sdmmcHostDmaBuffer']:
  m=re.search(r'^\s+'+name+r'\s+0x([0-9a-f]+)\s+Data\s+(\d+)\s+',s,re.M);assert m,name
  addr,size=int(m[1],16),int(m[2]);assert addr%(4 if name=='s_sdmmcHostDmaBuffer' else 64)==0 and 0x81e00000<=addr and addr+size<=0x82000000
  buffers[name]={'address':hex(addr),'bytes':size}
 regions=[{'name':m[1],'base':m[2],'used':int(m[4],16),'maximum':int(m[5],16)} for m in re.finditer(r'Execution Region (\w+) \(Exec base: (0x[0-9a-fA-F]+), Load base: (0x[0-9a-fA-F]+), Size: (0x[0-9a-fA-F]+), Max: (0x[0-9a-fA-F]+)',s)]
 sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 inputs=json.loads((out/'inputs.json').read_text());frozen=inputs['sources'];assert all(sha(ROOT/p)==h for p,h in frozen.items())
 missing=set()
 for dep in (out/'Objects').glob('*.d'):
  for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b',dep.read_text(errors='replace')):
   p=Path(token).resolve()
   if p.name=='recorder_identity.h':continue
   k=str(p.relative_to(ROOT)) if p.is_relative_to(ROOT) else str(p)
   if k not in frozen:missing.add(k)
 assert not missing,missing
 assert sha(ROOT/'demo/recorder_b2/mdk/recorder_b2.uvprojx')==inputs['project_sha256']
 files=[mp,out/'Objects/recorder_b2.axf',out/'Objects/recorder_b2.bin',out/'keil-build.log',out/'inputs.json']
 record={'build_only':True,'zero_errors_warnings':True,'no_actuator_encoder_control_symbols':True,'tasks':['Capture priority4','Writer priority2','Monitor priority5'],'buffers':buffers,'regions':regions,'frozen_file_count':len(frozen),'fresh_dependencies_match_frozen_manifest':True,'embedded_identity_manifest_digest':inputs['manifest_digest'],'adapter_inputs_verified':True,'volume':'2:','command_gate':'B2 START required before Camera/SD init','session_limit':'3s recording or 100 observed events; 10s monitor-start cap; software policy, not measured WCET','sha256':{str(p.relative_to(ROOT)):sha(p) for p in files},'hardware_accessed':False,'b1':'SKIPPED_BY_USER','b2_hardware_accepted':False}
 (out/'build-evidence.json').write_text(json.dumps(record,indent=2));print(json.dumps(record,indent=2))
 return record
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output');a=p.parse_args();verify(a.output)
