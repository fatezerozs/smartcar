"""Retained SCRAW-1 fixtures from the existing C producer. No hardware or SD use."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys,struct,zlib
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from vision_data.archive import import_session
from vision_data.continuous import build_report
from vision_data.common import encoded,data_root
from vision_data import scraw_v1

def control(tag,obj):
 payload=encoded(obj);first=struct.pack('<4sIQQIII',tag,44+len(payload),0,0,len(payload),zlib.crc32(payload),1)
 head=first+struct.pack('<I',zlib.crc32(first));return head+payload+struct.pack('<I',zlib.crc32(head+payload))
def run(output):
 out=Path(output).resolve()
 if out.exists() or not out.is_relative_to(ROOT.parent) or out.is_relative_to(ROOT):raise ValueError('New output under workspace parent, outside Git required')
 data_root(out);support=out/'support';support.mkdir();exe=support/'core_producer.exe'
 command=['gcc','-O2','-std=c99','-Wall','-Wextra','-Werror','-Idemo/recorder/include','demo/recorder/recorder_core.c','tests/recorder/core_harness.c','-o',str(exe)]
 p=subprocess.run(command,cwd=ROOT,capture_output=True,text=True,timeout=60);(support/'compile.log').write_text(p.stdout+p.stderr);p.check_returncode()
 results=[];archive=out/'archive'
 for kind in ['complete','gap','corrupt','truncated']:
  sid='b2-synthetic-'+kind;source=out/'sources'/sid;source.mkdir(parents=True)
  meta={'format':'SCRAW-1','schema_version':'1.0','session_id':sid,'synthetic':True,'width':188,'height':120,'stride':188,'pixel_format':'GRAY8','timestamp_unit':'us','timestamp_semantics':'csi_irq_gpt1_ms_x1000','timestamp_resolution_us':1000,'session_uuid_hex':bytes(range(16)).hex(),'firmware_build':'HOST_SYNTHETIC_ONLY','configured_sensor_fps':50,'actual_exposure_readback':None}
  segment=source/'000000.scraw';subprocess.run([str(exe),str(segment),json.dumps(meta,separators=(',',':'))],check=True,capture_output=True,timeout=30)
  original=segment.read_bytes();scan={'issues':[],'ending':None};frames=list(scraw_v1.scan(segment,scraw_v1.expected(dict(meta,segments=['000000.scraw']),0),scan))
  ending={'observed_capture':3,'capture_frames':None,'vision_processed_frames':2,'accepted':3,'recorded_frames':3,'synced_before_end':0,'base_source_sequence':2**32-1,'first_source_sequence':2**32,'last_source_sequence':2**32+2,'queue_full':0,'pool_full':0,'capture_ambiguous':0,'unknown_source_loss':False,'storage_errors':0,'status':'complete_observed_window'}
  raw=bytearray(original)
  if kind=='gap':
   start=frames[1][0]['payload_offset']-40;next_start=frames[2][0]['payload_offset']-40;raw=raw[:start]+raw[next_start:]
   raw+=control(b'GAP1',{'reason':'host_fault_injection_queue_loss','queue_full_delta':1,'pool_full_delta':0,'capture_ambiguous_delta':0,'unknown_loss':False,'first_source_sequence':None,'last_source_sequence':None})
   ending.update(accepted=2,recorded_frames=2,queue_full=1,status='incomplete')
  if kind=='corrupt':raw[frames[1][0]['payload_offset']+100]^=1
  if kind=='truncated':raw=raw[:-1000]
  else:
   raw+=control(b'END1',ending);(source/'session.json').write_bytes(encoded(dict(meta,segments=['000000.scraw'])))
  segment.write_bytes(raw);before=hashlib.sha256(raw).hexdigest();imported=import_session(source,archive)
  quality=build_report(archive,sid,out/'reports'/kind,every=2)
  assert quality['status']==('complete' if kind=='complete' else 'incomplete'),quality
  assert quality['synthetic'] and not quality['b2_hardware_accepted']
  assert hashlib.sha256(segment.read_bytes()).hexdigest()==before
  results.append({'fixture':kind,'source':str(source),'quality_report':str(out/'reports'/kind/'continuous_report.json'),'source_sha256':before,'status':quality['status'],'frames':quality['frames'],'synthetic':True,'hardware_verified':False,'source_unchanged':True})
 record={'synthetic_only':True,'b1_skipped':True,'b2_hardware_accepted':False,'compiler_command':command,'sources_sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in ['demo/recorder/recorder_core.c','tests/recorder/core_harness.c']},'cases':results}
 (out/'demo-result.json').write_text(json.dumps(record,indent=2),encoding='utf-8');print(json.dumps(record,indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);a=p.parse_args();run(a.output)
