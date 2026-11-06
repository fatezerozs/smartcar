"""Decode static RAM telemetry; never promote it to a motion Session."""
from pathlib import Path
import argparse,csv,json,re,struct,zlib,statistics
KINDS={1:'raw_counter_read',2:'encoder_sample',3:'pi_idle_or_reject',4:'pwm_driver_write',5:'c26_mode',6:'vision_observation',7:'state_fault',8:'pwm_register_snapshot',9:'pi_work_path'}
def fields(s):
 pairs=re.findall(r'(\w+)=([^\s]+)',s)
 if len(dict(pairs))!=len(pairs):raise ValueError('Duplicate metadata')
 return dict(pairs)
def decode(header,raw):
 h=fields(header)
 if not header.startswith('B3TRACE DATA ') or h.get('format')!='B3T1' or h.get('static')!='1' or h.get('synthetic') not in ('0','1'):raise ValueError('Unsupported static trace/provenance')
 n=int(h['count']);size=int(h['bytes'])
 if not 0<n<=3072 or size!=n*64 or len(raw)!=size:raise ValueError('Trace size/count mismatch')
 if zlib.crc32(raw)!=int(h['crc32'],16):raise ValueError('Trace CRC mismatch')
 rows=[];epoch=0;previous=None;counts={k:0 for k in KINDS};pi=[];samples=[];levels=set();outputs=[];raw_refs={};unmatched=[]
 for i in range(n):
  ordinal,kind,ms,cycles,*v=struct.unpack_from('<4I12i',raw,i*64)
  if ordinal!=i+1 or kind not in KINDS:raise ValueError('Event ordinal/kind')
  if previous is not None and ms<previous:
   if previous-ms<2**31:raise ValueError('Non-monotonic event time')
   epoch+=2**32
  previous=ms;counts[kind]+=1
  row={'ordinal':ordinal,'kind':KINDS[kind],'source_byte_offset':i*64,'timestamp_ms_extended':epoch+ms,'cycle_snapshot':cycles,**{'v'+str(j):x for j,x in enumerate(v)}};rows.append(row)
  if kind==1:
   if not 0<=v[0]<=65535 or not 0<=v[1]<=65535:raise ValueError('16-bit counter range')
   raw_refs[ordinal]=v
  if kind==2:
   samples.append(v)
   if v[11] not in raw_refs:unmatched.append(ordinal)
  if kind in (3,9):pi.append((kind,v))
  if kind==5:levels.add(v[0])
  if kind in (4,8):
   if v[2]!=0:raise ValueError('Static actuator applied nonzero PWM')
   if v[8] not in (0,1,2):raise ValueError('Actuator channel')
   outputs.append(v)
  if kind==6:
   age=(ms-(v[1]&0xffffffff))&0xffffffff
   if age>=2**31:raise ValueError('Future/ambiguous capture timestamp')
   row['capture_age_ms']=age
   for key,j in [('lateral',4),('heading',5),('steering',6),('near_center',7),('far_center',8)]:row[key]=struct.unpack('<f',struct.pack('<I',v[j]&0xffffffff))[0]
 gaps=[((b[1]&0xffffffff)-(a[1]&0xffffffff))&0xffffffff for a,b in zip(samples,samples[1:])]
 complete=int(h['lost'])==0 and int(h['reason'])==1 and not unmatched
 report={'format':'B3T1','role':'B3 static prerequisite RAM trace; no SD/image/motion Session','synthetic':h['synthetic']=='1','status':'complete' if complete else 'incomplete','events':n,'events_by_kind':{KINDS[k]:v for k,v in counts.items()},'source_input_digest':h.get('source'),'c26_raw_levels_seen':sorted(levels),'counter_sample_interval_ms':{'min':min(gaps) if gaps else None,'median':statistics.median(gaps) if gaps else None,'max':max(gaps) if gaps else None},'raw_sample_reference_missing':unmatched,'all_observed_applied_pwm_zero':all(v[2]==0 for v in outputs),'all_three_actuator_snapshots_present':{v[8] for v in outputs}=={0,1,2},'nonzero_requested_pwm_events':sum(v[1]>0 for v in outputs),'active_pi_observed':any(kind==9 and (v[2] or v[3]) for kind,v in pi),'pi_gap_max_ms':int(h['pi_gap_max_ms']),'pi_max_us':int(h['pi_max_cycles'])*1e6/int(h['hz']),'primask_max_us':int(h['mask_max_cycles'])*1e6/int(h['hz']),'emit_max_us':int(h['emit_max_cycles'])*1e6/int(h['hz']),'b3_motion_accepted':False,'control_parallel_sd_validated':False,'production_ready':False,'limits':['No SD/Camera raw file recording in this baseline','Applied register commands are zero by a private static driver adapter; requested PWM and PI values are not actual actuator output','C26 transitions, active PI and manual Encoder direction tests need separate observed coverage','Timebase is GPT1 milliseconds and cycle intervals; no physical switch-edge or pulse measurement','Per-stream joins must use explicit references or previous-sample age, never equate targets with measured motion']}
 return report,rows
def verify(directory):
 p=Path(directory);report,rows=decode((p/'header.txt').read_text(),(p/'trace.b3t').read_bytes())
 with (p/'events.csv').open('x',newline='',encoding='utf-8') as f:
  writer=csv.DictWriter(f,fieldnames=list(rows[0])+['capture_age_ms','lateral','heading','steering','near_center','far_center']);writer.writeheader();writer.writerows(rows)
 with (p/'static-review.json').open('x',encoding='utf-8') as f:json.dump(report,f,indent=2)
 print(json.dumps(report,indent=2));return report['status']=='complete' and not report['synthetic']
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory');a=p.parse_args();raise SystemExit(0 if verify(a.directory) else 1)
