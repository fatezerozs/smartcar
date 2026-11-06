"""Compile and run the composed Host reference pipeline, with optional ARM objects.

No serial, debugger, firmware download, motor access or tool installation.
"""
import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from speed_planning.reference import DEFAULT_REFERENCE,load_reference,write_c_header,derived,values
from speed_planning.vehicle_model import ROOT,load_model

CASES=['straight','preview-bend','far-short-recovery','far-time-budget','far-distance-budget',
       'near-recovery-to-degraded','near-timeout','camera-freeze','encoder-freeze',
       'STOP-during-candidate','preview-identity-mismatch','infeasible-sharp-cap']
MODES=['DISARMED','NORMAL_TRACK','DEGRADED_FAR_LOSS','RECOVERY_NO_LANE','CONTROLLED_STOP','STOP_LATCHED','FAULT_LATCHED']


def compile_command(compiler,family,mode,sources,includes,exe):
    if family=='msvc':
        flags=['/nologo','/utf-8','/std:c11','/W4','/WX']+(['/Od','/Zi','/fsanitize=address'] if mode=='asan' else ['/O2'])
        return [compiler,*flags,*['/I'+str(p) for p in includes],*map(str,sources),'/Fe'+str(exe),'/Fo'+str(exe.parent)+os.sep,'/Fd'+str(exe.parent)+os.sep]
    flags=['-O0','-g','-fsanitize=address'] if mode=='asan' else ['-O2']
    return [compiler,'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',*flags,
            *['-I'+str(p) for p in includes],*map(str,sources),'-lm','-o',str(exe)]


def analyze(folder,factor):
    path=folder/f'pipeline-replay-{factor:.1f}.csv'
    rows=list(csv.DictReader(path.open(encoding='utf-8')))
    if len(rows)!=6000:raise ValueError('Incomplete composed replay')
    for row in rows:
        if not all(math.isfinite(float(value)) for value in row.values()):raise ValueError('Nonfinite telemetry')
    reports=[]
    for case,name in enumerate(CASES):
        group=[r for r in rows if int(r['case'])==case]
        if [int(r['timeMs']) for r in group]!=list(range(10,5001,10)):raise ValueError('Invalid replay timebase')
        transitions=[];previous=None
        for row in group:
            mode=MODES[int(row['mode'])]
            if mode!=previous:transitions.append({'timeMs':int(row['timeMs']),'mode':mode});previous=mode
            if row['nearValid']=='0' or mode in ('RECOVERY_NO_LANE','STOP_LATCHED','FAULT_LATCHED'):
                if any(int(row[key]) for key in ('leftTargetTicks10ms','rightTargetTicks10ms','leftPwmPermille','rightPwmPermille','leftIntegralQ8','rightIntegralQ8')):
                    raise AssertionError('Nonzero target/PI/final output during inhibit')
        active=[r for r in group if int(r['timeMs'])>=2000 and int(r['mode']) in (1,2)]
        errors=[float(r['speedRefMps'])-float(r['measuredMps']) for r in active]
        normal_jerk=[]
        for a,b in zip(group,group[1:]):
            if a['brakeOverride']=='0' and b['brakeOverride']=='0' and a['mode']==b['mode'] and int(b['mode']) in (1,2,4):
                normal_jerk.append((float(b['accelRefMps2'])-float(a['accelRefMps2']))/.01)
        overrides={'farDistanceM':1.0} if case==3 else {'farBudgetMs':2000,'farDistanceM':.05} if case==4 else {}
        reports.append({'case':name,'referencePolicyOverrides':overrides,'transitions':transitions,'activeSteadySamples':len(errors),
             'simulatedTrackingRmseMps':math.sqrt(sum(e*e for e in errors)/len(errors)) if errors else None,
             'maxReferenceMps':max(float(r['speedRefMps']) for r in group),
             'maxAbsReferenceAccelMps2':max(abs(float(r['accelRefMps2'])) for r in group),
             'maxAbsNormalReferenceJerkMps3':max(map(abs,normal_jerk),default=0),
             'brakingCapOverrides':sum(int(r['brakeOverride']) for r in group),
             'finalMode':MODES[int(group[-1]['mode'])],'finalFaults':int(group[-1]['faults'])})
    queue=list(csv.DictReader((folder/f'pipeline-queue-{factor:.1f}.csv').open(encoding='utf-8')))
    if len(queue)!=24:raise ValueError('Missing queue experiment pairs')
    for case in range(12):
        fast,slow=[r for r in queue if int(r['case'])==case]
        if fast['controlHash']!=slow['controlHash'] or int(fast['dropped']) or int(slow['dropped'])<=0:
            raise AssertionError('Storage-pressure experiment mismatch')
    return {'sensorResolutionFactor':factor,'samples':len(rows),'cases':reports,
            'queueExperiments':queue,'csvSha256':hashlib.sha256(path.read_bytes()).hexdigest()}


def run_replay(out,compiler=None,family=None,mode=None,arm_bin=None,reference_path=DEFAULT_REFERENCE):
    out=Path(out).resolve();out.mkdir(parents=True,exist_ok=True)
    compiler=compiler or os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('gcc')
    if not compiler:raise ValueError('Existing compiler required')
    family=family or os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=mode or os.environ.get('SMARTCAR_TEST_MODE','optimized')
    reference=load_reference(reference_path);model=load_model(ROOT/reference['geometryProfile']);write_c_header(out/'reference_config.h',reference,model)
    sources=[ROOT/'tests/planning/test_pipeline.c',ROOT/'tools/speed_planning/pipeline.c',
             ROOT/'tools/speed_planning/speed_planner.c',ROOT/'demo/code/wheel_speed.c']
    command=compile_command(str(compiler),family,mode,sources,[ROOT/'tools/speed_planning',ROOT/'demo/code',out],out/'pipeline.exe')
    print(subprocess.list2cmdline(command),flush=True);subprocess.run(command,cwd=out,check=True,timeout=90)
    experiments=[]
    for factor in reference['sensitivityFactors']:
        subprocess.run([out/'pipeline.exe',str(factor)],cwd=out,check=True,timeout=30)
        experiments.append(analyze(out,factor))
    result={'schemaVersion':'smartcar.composed-reference-replay/2','referenceId':reference['referenceId'],
        'evidence':'synthetic-closed-loop-reference','productionApproved':False,'hardwareAccessed':False,
        'controlPeriodMs':10,'visionPeriodMs':20,'hypothesis':derived(reference,model),
        'profileSha256':hashlib.sha256(Path(reference_path).read_bytes()).hexdigest(),
        'compilerFamily':family,'mode':mode,'compileCommand':command,'experiments':experiments,
        'notes':['Simulated measured speed comes from a fictitious first-order wheel plant; it is not vehicle evidence.',
                 'CPR factor changes both simulated sensor resolution and controller scale consistently. It does not test unknown CPR miscalibration.',
                 '0.5/1/2 factors are artificial sensitivity settings, not a confidence interval.',
                 'Final output means a mock commit record, not PWM hardware or the production vision_actuator writer.',
                 'Queue is fixed-memory single-owner Host code, not an implemented ISR-safe/FreeRTOS sidecar.']}
    if arm_bin:
        arm_bin=Path(arm_bin);arm_commands=[];artifacts={}
        target=out/'target';target.mkdir(exist_ok=True)
        for source in sources[1:]:
            obj=target/(source.stem+'.o')
            cmd=[str(arm_bin/'armclang.exe'),'--target=arm-arm-none-eabi','-mcpu=cortex-m7','-mfpu=fpv5-d16',
                 '-mfloat-abi=hard','-std=c11','-O2','-Wall','-Wextra','-Werror','-fstack-usage',
                 '-I'+str(ROOT/'tools/speed_planning'),'-I'+str(ROOT/'demo/code'),'-c',str(source),'-o',str(obj)]
            subprocess.run(cmd,cwd=out,check=True,timeout=60);arm_commands.append(cmd)
            artifacts[obj.name]={'bytes':obj.stat().st_size,'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()}
        with (target/'object-sizes.txt').open('wb') as log:
            subprocess.run([str(arm_bin/'fromelf.exe'),'--text','-z',*map(str,sorted(target.glob('*.o')))],stdout=log,check=True,timeout=30)
        result['targetObjectCompilation']={'commands':arm_commands,'artifacts':artifacts,'linkedIntoFirmware':False,
            'ownStackFrames':{p.name:p.read_text(encoding='utf-8').splitlines() for p in target.glob('*.su')}}
    (out/'replay-report.json').write_text(json.dumps(result,ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    print('Composed replay PASS: 36 case/resolution combinations, each with two storage-consumption modes; '+str(out/'replay-report.json'),flush=True)
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--compiler');parser.add_argument('--family',choices=['gcc','msvc'])
    parser.add_argument('--mode',choices=['optimized','asan']);parser.add_argument('--arm-bin',type=Path)
    parser.add_argument('--reference',type=Path,default=DEFAULT_REFERENCE)
    args=parser.parse_args();run_replay(args.output,args.compiler,args.family,args.mode,args.arm_bin,args.reference)


if __name__=='__main__':main()
