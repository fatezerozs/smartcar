"""Validate actual board measurement logs; never invent missing measurements."""
import argparse,json,re
from pathlib import Path

def review(log):
 text=Path(log).read_text(encoding='utf-8-sig');groups=[]
 for line in text.splitlines():
  if not line.startswith('B2METRIC '):continue
  values={}
  for key,value in re.findall(r'(\w+)=([^\s]+)',line):
   if key in ('csi_cr1','csi_cr18'):values[key]=int(value,16)
   elif value.isdigit():values[key]=int(value)
   else:values[key]=value
  groups.append(values)
 tasks=[g for g in groups if 'task' in g];io=[g for g in groups if 'io' in g]
 device=next((g for g in groups if 'csi_width_cycles' in g),{})
 runtime=next((g for g in groups if 'runtime_total_us' in g),{})
 checks={
  'csi_programmed_geometry_readback':device.get('csi_width_cycles')==188 and device.get('csi_height')==120,
  'sd_actual_clock_and_bus_readback':0<device.get('sd_hz',0)<=25000000 and device.get('sd_bits')==1,
  'all_task_runtime_and_stack_measurements':{g['task'] for g in tasks}=={'Capture','Writer','Monitor','IDLE'} and all(g.get('stack_free_words',0)>0 for g in tasks),
  'runtime_and_irq_measurements':runtime.get('runtime_total_us',0)>0 and 0<=runtime.get('nonidle_permille',-1)<=1000 and runtime.get('csi_irq_count',0)>0 and runtime.get('sd_irq_count',0)>0,
  'complete_error_free_io_samples':{g['io'] for g in io}=={'write','sync','close'} and all(g.get('count',0)>0 and g.get('sample_overflow',-1)==0 and g.get('errors',-1)==0 and 0<=g.get('p50_us',-1)<=g.get('p95_us',-1)<=g.get('p99_us',-1)<=g.get('max_us',-1) for g in io),
 }
 result={'checks':checks,'measurements_present_and_valid':all(checks.values()),'device':device,'tasks':tasks,'runtime':runtime,'io':io,'b2_full_accepted':False,'scope':'measurement log only; requires paired Session full PC verification and review','limitations':['task runtime attributes interrupt time to the interrupted task; nonidle share is a CPU load estimate, not cycle-accurate total utilization','IRQ maxima inclusive of any higher-priority preemption; not WCET','I/O latencies include task preemption and filesystem processing; no pure device latency claim','CSI geometry is peripheral register configuration, not sensor exposure register readback','bounded short window does not establish long-duration stability or control deadlines']}
 print(json.dumps(result,indent=2));return result
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('log');p.add_argument('--output',required=True);a=p.parse_args();r=review(a.log);out=Path(a.output)
 with out.open('x',encoding='utf-8') as f:json.dump(r,f,indent=2)
 raise SystemExit(0 if r['measurements_present_and_valid'] else 1)
