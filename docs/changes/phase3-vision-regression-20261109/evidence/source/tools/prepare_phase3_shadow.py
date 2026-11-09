"""Prepare/build an isolated measured-data C26 shadow candidate. Never flash.

The download project is a separate explicit entrypoint sharing the verified
AXF. Original application defaults, PI gains and physical admission stay intact.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from tools.speed_planning.phase3_profile import PROFILE,digest,header,validate_profile
sys.path.insert(0,str(ROOT/'tools/testing'))
from process_runner import run_process

VERSION='PHASE3_C26_SHADOW_20261109_4'


def replace_once(text,old,new):
    if text.count(old)!=1:raise ValueError('Candidate source context changed: '+old[:90])
    return text.replace(old,new)


def prepare(output,profile_path=PROFILE):
    out=Path(output).resolve();profile_path=Path(profile_path).resolve()
    if out.exists() or not out.is_relative_to(ROOT/'.build') or out==ROOT/'.build':
        raise ValueError('A NEW directory below repository .build is required')
    profile=validate_profile(json.loads(profile_path.read_text('utf-8')))
    original=ROOT/'demo/mdk/competition/MPC_20261106.uvprojx';tree=ET.parse(original)
    target=tree.find('./Targets/Target')
    if len(tree.findall('./Targets/Target'))!=1:raise ValueError('One C26 target required')
    defines=target.findtext('.//Cads/VariousControls/Define') or ''
    for name,value in [('VISION_MIDTERM_AUTONOMOUS','0'),('VISION_ROUTE_SWITCH','1'),('VISION_ENCODER_ENABLE','1')]:
        if name+'='+value not in [p.strip() for p in defines.split(',')]:raise ValueError('Wrong original target: '+name)
    # Create the complete native copy; no frozen Phase2 image or old .build
    # code is silently used as the source of this Phase3 candidate.
    workspace=out/'workspace'
    for name in ('demo','libraries'):
        shutil.copytree(ROOT/name,workspace/name,ignore=shutil.ignore_patterns('Objects','Listings','*.uvoptx','*.uvguix.*','*.build_log.htm'))
    shared=workspace/'tools/speed_planning';shared.mkdir(parents=True)
    for name in ('speed_planner.c','speed_planner.h','vision_adapter.c','vision_adapter.h'):
        shutil.copyfile(ROOT/'tools/speed_planning'/name,shared/name)
    code=workspace/'demo/code';inputs={}
    def change(name,fn):
        path=code/name;inputs['demo/code/'+name]=digest(ROOT/'demo/code'/name)
        path.write_text(fn(path.read_text('utf-8-sig')),'utf-8')
    change('vision_drive.c',lambda s:replace_once(replace_once(replace_once(s,
        '#include "vision_planner_runtime.h"','#include "vision_planner_runtime.h"\n#include "vision_planner_shadow_diag.h"'),
        'vd_planner_runtime_init();','vd_planner_runtime_init();\n    vd_shadow_diag_init();'),
        'vd_planner_runtime_observe(r,sequence,frame_ms,now);','vd_shadow_diag_observe(r,sequence,frame_ms,now);').replace(
        'vd_planner_runtime_poll(now);','vd_shadow_diag_poll(now);'))
    if (code/'vision_drive.c').read_text('utf-8').count('vd_shadow_diag_poll(now);')!=2:
        raise ValueError('Expected the two original foreground runtime hooks')
    def field(s):
        s='#include "vision_planner_shadow_diag.h"\n'+s
        return replace_once(s,'    vision_drive_get_profile(&cfg);',
            '    vision_drive_get_profile(&cfg);\n    if(vd_shadow_diag_field(p->line,response,VF_RESPONSE_CAPACITY))return 2;')
    change('vision_field.c',field)
    # Human-readable identity is part of @status, not a duplicate motor lease.
    change('vision_field.c',lambda s:replace_once(s,'"[FIELD] version=%s ready=',
        '"[FIELD] build='+VERSION+' version=%s ready='))
    for name in ('vision_planner_shadow_diag.c','vision_planner_shadow_diag.h'):
        path=ROOT/'demo/candidates/phase3-shadow'/name;inputs[path.relative_to(ROOT).as_posix()]=digest(path)
        shutil.copyfile(path,code/name)
    (code/'phase3_shadow_profile.h').write_text(header(profile),'utf-8')
    out.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(profile_path,out/'profile.json')
    switches={'VISION_PLANNER_RUNTIME':'1','VISION_PLANNER_ACTIVE':'0','VISION_PLANNER_PROFILE_REVIEWED':'0'}
    parts=[p for p in defines.split(',') if p.strip().split('=')[0].strip() not in switches]
    target.find('.//Cads/VariousControls/Define').text=','.join(parts+[k+'='+v for k,v in switches.items()])
    common=target.find('./TargetOption/TargetCommonOption')
    for group in ('BeforeMake','BeforeCompile','AfterMake'):
        for node in common.find(group):
            if node.tag.startswith(('RunUserProg','nStop')):node.text='0'
            elif 'Name' in node.tag:node.text=''
    for key,folder in [('OutputDirectory',out/'Objects'),('ListingPath',out/'Listings')]:
        folder.mkdir();common.find(key).text=str(folder)+os.sep
    files=target.find('.//Group/Files')
    for name,kind in [('vision_planner_shadow_diag.c','1'),('vision_planner_shadow_diag.h','5'),('phase3_shadow_profile.h','5')]:
        item=ET.SubElement(files,'File');ET.SubElement(item,'FileName').text=name
        ET.SubElement(item,'FileType').text=kind;ET.SubElement(item,'FilePath').text='..\\..\\code\\'+name
    directory=workspace/'demo/mdk/competition'
    project=directory/'MPC_Phase3_Shadow.uvprojx';download=directory/'MPC_Phase3_Shadow_Download.uvprojx'
    # The inherited original project is source evidence only; remove the
    # unpatched duplicate entry from this isolated copy to avoid wrong builds.
    copied_original=directory/original.name
    if copied_original.is_file():copied_original.unlink()
    target.find('.//UpdateFlashBeforeDebugging').text='0';tree.write(project,encoding='utf-8',xml_declaration=True)
    download_tree=copy.deepcopy(tree);download_tree.find('.//UpdateFlashBeforeDebugging').text='1'
    download_tree.write(download,encoding='utf-8',xml_declaration=True)
    for entry in (project,download):
        shutil.copyfile(ROOT/'demo/mdk/base/rt1064.uvoptx',entry.with_suffix('.uvoptx'))
    manifest={'schemaVersion':'smartcar.phase3-shadow-candidate/1','version':VERSION,'profileId':profile['profileId'],
              'sourceProjectSha256':digest(original),'project':str(project),'downloadProject':str(download),
              'output':str(out),'target':target.findtext('TargetName'),'defines':target.findtext('.//Cads/VariousControls/Define'),
              'profileSha256':digest(out/'profile.json'),'primaryInputSha256':inputs,
              'generatedCodeSha256':{p.name:digest(p) for p in sorted(code.glob('*')) if p.is_file()},
              'buildPassed':False,'flashed':False,'productionApproved':False,'metricActive':False,
              'legacyPiAndPwmLimitsChanged':False,'autoLoadOrArm':False,
              'trace':{'rows':256,'ringOverwritesReported':True,'queriesStop':True,'noRuntimeUartOrSdWrites':True},
              'scope':'Explicit planner_load while stopped; mixed measured candidate geometry/scales and labelled reference policy; legacy motors remain under original C26 owner.'}
    (out/'candidate.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8')
    (out/'README.md').write_text('# '+VERSION+'\n\nBuild: '+str(project)+'\n\nDownload: '+str(download)+
        '\n\nProfile: '+profile['profileId']+'; runtime=1, active=0. This command only prepared files.\n'
        '\nAfter identity/STOP/OFF checks: @planner_load, @planner. Physical C26 admission remains required. '
        'Queries stop driving; read retained rows only after STOP, then explicitly @planner_clear. '
        'Original legacy PI/gains/PWM limits are unchanged.\n','utf-8')
    return manifest


def build(manifest,uv4,arm_bin):
    out=Path(manifest['output']);project=Path(manifest['project']);uv4=Path(uv4).resolve()
    fromelf=Path(arm_bin).resolve()/'fromelf.exe'
    if not uv4.is_file() or not fromelf.is_file():raise ValueError('Existing UV4/fromelf tools required')
    processes=[]
    with (out/'stdout.log').open('wb') as stdout,(out/'stderr.log').open('wb') as stderr:
        for command in ([uv4,'-r',project,'-t',manifest['target'],'-j0','-o',out/'build.log'],
                        [fromelf,'--bincombined','--output',out/'phase3-shadow.bin',out/'Objects/rt1064.axf']):
            result=run_process(list(map(str,command)),project.parent,dict(os.environ),600,stdout,stderr);processes.append(result)
            if result['exitCode'] or result['timedOut'] or not result['cleanupVerified']:
                raise RuntimeError('Build failed: '+str(out/'build.log'))
    log=(out/'build.log').read_text('utf-8',errors='replace')
    if '0 Error(s), 0 Warning(s)' not in log:raise RuntimeError('Zero errors AND warnings required')
    manifest.update(buildPassed=True,processes=processes,artifactsSha256={p.relative_to(out).as_posix():digest(p) for p in
        [out/'Objects/rt1064.axf',out/'phase3-shadow.bin',out/'build.log',project,Path(manifest['downloadProject'])]})
    (out/'candidate.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n','utf-8')
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--profile',type=Path,default=PROFILE);parser.add_argument('--build',action='store_true')
    parser.add_argument('--uv4',type=Path,default=Path('E:/ElecTronic/Keil/UV4/UV4.exe'))
    parser.add_argument('--arm-bin',type=Path,default=Path('E:/ElecTronic/Keil/ARM/ARMCLANG/bin'))
    args=parser.parse_args();manifest=prepare(args.output,args.profile)
    if args.build:manifest=build(manifest,args.uv4,args.arm_bin)
    print(json.dumps({k:manifest[k] for k in ('version','profileId','project','downloadProject','buildPassed','flashed')},indent=2))
