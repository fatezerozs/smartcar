"""Pure build of the unchanged C26 project using explicit XML source paths only."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,xml.etree.ElementTree as E,re
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(output):
 out=Path(output).resolve();allowed=(ROOT/'.build/recorder-camera-stages-20261106').resolve()
 if not out.is_relative_to(allowed) or out==allowed or out.exists():raise ValueError('NEW scoped output required')
 original=ROOT/'demo/mdk/competition/MPC_20261106.uvprojx';before=sha(original);tree=E.parse(original)
 target=tree.find('.//Target');defines=target.findtext('.//Cads/VariousControls/Define')
 for name,value in [('VISION_MIDTERM_AUTONOMOUS','0'),('VISION_ROUTE_SWITCH','1')]:
  assert re.search(r'(?:^|,)\s*'+name+'='+value+r'(?:,|$)',defines)
 for hook in ['BeforeMake','BeforeCompile','AfterMake']:
  for flag in ['RunUserProg1','RunUserProg2']:assert target.findtext('.//'+hook+'/'+flag)=='0'
 inputs={str(original.relative_to(ROOT)):before}
 for e in tree.findall('.//FilePath'):
  p=(original.parent/e.text.replace('\\','/')).resolve();assert p.is_relative_to(ROOT) and p.is_file(),p
  inputs[str(p.relative_to(ROOT))]=sha(p);e.text=str(p)
 for e in tree.findall('.//IncludePath'):
  if e.text:e.text=';'.join(str((original.parent/x.strip().replace('\\','/')).resolve()) for x in e.text.split(';') if x.strip())
 for e in tree.findall('.//ScatterFile'):
  p=(original.parent/e.text.replace('\\','/')).resolve();inputs[str(p.relative_to(ROOT))]=sha(p);e.text=str(p)
 out.mkdir(parents=True);(out/'Objects').mkdir();(out/'Listings').mkdir()
 c=target.find('.//TargetCommonOption');c.find('OutputDirectory').text=str(out/'Objects')+'\\';c.find('ListingPath').text=str(out/'Listings')+'\\'
 target.find('.//UpdateFlashBeforeDebugging').text='0'
 project=out/'c26_audit.uvprojx';E.indent(tree,space='  ');tree.write(project,encoding='utf-8',xml_declaration=True)
 uv=Path('E:/ElecTronic/Keil/UV4/UV4.exe');arm=uv.parent.parent/'ARM/ARMCLANG/bin'
 startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
 command=[str(uv),'-b',str(project),'-j0','-o',str(out/'keil-build.log')]
 process=subprocess.Popen(command,cwd=out,startupinfo=startup)
 try:code=process.wait(timeout=180)
 except subprocess.TimeoutExpired:
  subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],capture_output=True);raise
 log=(out/'keil-build.log').read_text(errors='replace');assert code==0 and '0 Error(s), 0 Warning(s)' in log,log[-3000:]
 axf=out/'Objects/rt1064.axf';binary=out/'Objects/rt1064.bin';subprocess.run([str(arm/'fromelf.exe'),'--bincombined','--output',str(binary),str(axf)],check=True)
 mp=out/'Listings/rt1064.map';text=mp.read_text(errors='replace')
 for symbol in ['recorder_storage_task','recorder_capture_task','vTaskStartScheduler','sp_plan']:
  assert not re.search(r'^\s+'+symbol+r'\s+0x[0-9a-f]+\s+Thumb Code',text,re.M),symbol
 assert sha(original)==before
 assert all(sha(ROOT/n)==h for n,h in inputs.items()),'Build inputs changed'
 record={'buildOnly':True,'defines':defines,'primaryProjectUnchanged':True,'sourceProjectSha256':before,'target':target.findtext('TargetName'),'inputs_sha256':inputs,'axf_sha256':sha(axf),'bin_sha256':sha(binary),'map_sha256':sha(mp),'no_recorder_or_planner_runtime_linked':True,'flashed':False,'hardware_verified':False,'log':str(out/'keil-build.log'),'map':str(mp)}
 (out/'evidence.json').write_text(json.dumps(record,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in record.items() if k!='inputs_sha256'},indent=2))
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);a=p.parse_args();build(a.output)
