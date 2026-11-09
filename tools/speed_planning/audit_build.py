"""Pure isolated build of the actual C26 project; never downloads firmware.

The output must be a new directory below this repository's .build. Original
project macros, sources and hooks are inspected. Optional planner switches are
set ONLY in the isolated copy; a control build is not physical profile approval.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/testing'))
from process_runner import run_process


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--uv4',type=Path,required=True)
    parser.add_argument('--arm-bin',type=Path,required=True)
    parser.add_argument('--planner-mode',choices=('off','shadow','control'),default='off')
    args=parser.parse_args();out=args.output.resolve()
    if not out.is_relative_to((ROOT/'.build').resolve()) or out==(ROOT/'.build').resolve() or out.exists():
        raise ValueError('Output must be a NEW directory under repository .build')
    uv4=args.uv4.resolve();fromelf=(args.arm_bin/'fromelf.exe').resolve()
    if not uv4.is_file() or not fromelf.is_file():raise ValueError('Existing UV4/fromelf required')
    original=ROOT/'demo/mdk/competition/MPC_20261106.uvprojx'
    source_bytes=original.read_bytes();tree=ET.fromstring(source_bytes)
    targets=tree.findall('./Targets/Target')
    if len(targets)!=1:raise ValueError('Expected one actual target')
    target=targets[0]
    defines=target.findtext('.//Cads/VariousControls/Define') or ''
    macros={s.strip().split('=')[0].strip():s.strip().split('=')[-1].strip() if '=' in s else '1' for s in defines.split(',')}
    for name,value in {'VISION_MIDTERM_AUTONOMOUS':'0','VISION_ROUTE_SWITCH':'1','VISION_MIDTERM_ROUTE_TRIAL':'1',
                       'VISION_ENCODER_ENABLE':'1','VISION_MIDTERM_TRAINING':'1'}.items():
        if macros.get(name)!=value:raise ValueError('Unexpected primary macro: '+name)
    for section in ('BeforeMake','BeforeCompile','AfterMake'):
        for key in ('RunUserProg1','RunUserProg2'):
            if target.findtext('.//'+section+'/'+key)!='0':raise ValueError('Enabled build hook: '+section+'/'+key)
    effective_defines=defines
    if args.planner_mode!='off':
        switches={'VISION_PLANNER_RUNTIME':'1','VISION_PLANNER_ACTIVE':'1' if args.planner_mode=='control' else '0',
                  'VISION_PLANNER_PROFILE_REVIEWED':'1' if args.planner_mode=='control' else '0'}
        parts=[s for s in defines.split(',') if s.strip().split('=')[0].strip() not in switches]
        effective_defines=','.join(parts+[name+'='+value for name,value in switches.items()])
        target.find('.//Cads/VariousControls/Define').text=effective_defines
    out.mkdir(parents=True)
    workspace=out/'workspace'
    for name in ('demo','libraries'):
        shutil.copytree(ROOT/name,workspace/name,ignore=shutil.ignore_patterns('Objects','Listings','*.uvoptx','*.uvguix.*','*.build_log.htm'))
    # The actual primary project compiles the single shared planner kernel;
    # copy only its native sources, never the tooling or calibration datasets.
    shared=workspace/'tools/speed_planning';shared.mkdir(parents=True)
    for name in ('speed_planner.c','speed_planner.h','vision_adapter.c','vision_adapter.h'):
        shutil.copyfile(ROOT/'tools/speed_planning'/name,shared/name)
    for key,folder in [('OutputDirectory',out/'Objects'),('ListingPath',out/'Listings')]:
        folder.mkdir();target.find('./TargetOption/TargetCommonOption/'+key).text=str(folder)+os.sep
    project=workspace/'demo/mdk/competition/MPC_20261106.uvprojx'
    ET.ElementTree(tree).write(project,encoding='utf-8',xml_declaration=True)
    evidence={'buildOnly':True,'sourceProjectSha256':hashlib.sha256(source_bytes).hexdigest(),
              'defines':defines,'effectiveDefines':effective_defines,'plannerMode':args.planner_mode,
              'syntheticReviewDeclaration':args.planner_mode=='control','physicalProfileApproved':False,
              'target':target.findtext('TargetName'),'processes':[]}
    with (out/'stdout.log').open('wb') as stdout,(out/'stderr.log').open('wb') as stderr:
        def run(command):
            result=run_process(list(map(str,command)),project.parent,dict(os.environ),600,stdout,stderr)
            evidence['processes'].append(result)
            if result['exitCode'] or result['timedOut'] or not result['cleanupVerified']:
                raise RuntimeError('Pure build tool failed; inspect '+str(out))
        run([uv4,'-r',project,'-t',evidence['target'],'-j0','-o',out/'build.log'])
        axf=out/'Objects/rt1064.axf'
        run([fromelf,'--bincombined','--output',out/'rt1064.bin',axf])
        run([fromelf,'--text','-c','-s','--output',out/'disassembly.txt',axf])
    evidence['zeroErrorsWarnings']='0 Error(s), 0 Warning(s)' in (out/'build.log').read_text(encoding='utf-8',errors='replace')
    evidence['artifacts']={str(p.relative_to(out)):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
        for p in [axf,out/'rt1064.bin',out/'disassembly.txt',*sorted((out/'Listings').glob('*.map'))]}
    (out/'evidence.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    if original.read_bytes()!=source_bytes:raise RuntimeError('Original project changed during audit')
    if not evidence['zeroErrorsWarnings']:raise RuntimeError('Build must have zero errors AND warnings')
    print('Pure build passed; '+str(out/'evidence.json'))


if __name__=='__main__':main()
