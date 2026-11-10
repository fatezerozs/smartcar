"""Real gray-to-planner composition contracts and scene compatibility audit.

Synthetic geometry uses an explicit synthetic map. Historical 94x60 field
previews are static poses, not native camera frames or calibrated motion.
Operational gaps are reported separately from fail-safe contract correctness.
"""
import argparse
import base64
from collections import Counter
import csv
import hashlib
from html import escape
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
sys.path.insert(0,str(ROOT/'tools/testing'))
from tools.speed_planning.phase3_profile import PROFILE,header
from tools.speed_planning.replay import compile_command
from vision_image_benchmark import FLAGS,SOURCES
from user_report import png_gray

SIZE=188*120

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,data):p.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def table(p):
    with p.open(encoding='utf-8',newline='') as f:return list(csv.DictReader(f))

def road(bend=0):
    return bytes(210 if 94+bend*(110-y)//2-40<=x<=94+bend*(110-y)//2+40 else 30
                 for y in range(120) for x in range(188))

def mirror(data):return b''.join(data[y*188:(y+1)*188][::-1] for y in range(120))

def island(side,clipped=False,opened=False):
    data=bytearray(road())
    for y in range(120):
        for x in range(188):
            if clipped:
                if 35<=y<=95 and x<=100:data[y*188+x]=210
                if 45<=y<=85 and x<=50 and (x-10)**2*20**2+(y-65)**2*40**2<=40**2*20**2:data[y*188+x]=30
            else:
                if (x-55)**2*52**2+(y-65)**2*64**2<=64**2*52**2:data[y*188+x]=210
                if (x-55)**2*20**2+(y-65)**2*18**2<=18**2*20**2:data[y*188+x]=30
            if opened and 62<=y<=68 and x<=55:data[y*188+x]=30
    return mirror(data) if side>0 else bytes(data)

def stripe(data,first,last,value):
    data=bytearray(data)
    for y in range(first,last+1):data[y*188:(y+1)*188]=bytes([value])*188
    return bytes(data)

def exit_opening(side):
    data=bytearray(road())
    for y in range(84,111):
        for x in range(94,188):data[y*188+x]=210
    return mirror(data) if side>0 else bytes(data)

def bright_floor(data):
    # A derived analytic exposure fixture, not a new camera observation.
    return bytes(250 if v==210 else 20 if i%188<10 else 140+(i%188)//4 for i,v in enumerate(data))

def phase(tag,data,count,**expect):return dict(tag=tag,data=data,count=count,expect=expect)

def scenarios():
    straight=road();result=[]
    def add(name,family,phases,**extra):result.append(dict(name=name,family=family,phases=phases,**extra))
    add('straight','直道',[phase('straight',straight,40,ring=0,metric=True)])
    for side in (-1,1):
        suffix='left' if side<0 else 'right'
        add('bend-'+suffix,'弯道',[phase('warmup',straight,20),phase('bend',road(side),30,ring=0,metric=True)])
        ring=[phase('warmup',straight,20),phase('entry',island(side),12,ring=1,side=side),
              phase('inside',road(side),6,ring=2,side=side),phase('tangent',straight,18,ring=2,side=side),
              phase('inside-again',road(side),6,ring=2,side=side),phase('exit',exit_opening(side),6,ring=3,side=side),
              phase('recover',straight,4,ring=4),phase('idle',straight,8,ring=0)]
        add('ring-'+suffix,'环岛完整状态序列',ring)
        add('ring-'+suffix+'-gray-floor','环岛亮灰地面派生',
            [dict(p,data=bright_floor(p['data'])) for p in ring])
        add('clipped-'+suffix,'侧裁切环岛',[phase('warmup',straight,20),phase('entry',island(side,clipped=True),6,ring=1,side=side),
            phase('inside',road(side),6,ring=2,side=side),phase('clear',road(-side),14,ring=0)])
        add('open-interior-'+suffix,'非环岛反例',[phase('warmup',straight,20),phase('open',island(side,opened=True),12,ring=0)])
    add('crossing','路口',[phase('warmup',straight,20),phase('cross',stripe(straight,40,95,210),8,ring=0),
        phase('return',straight,20,ring=0)])
    add('near-brief','短时近端缺失',[phase('warmup',straight,20),phase('near-loss',stripe(straight,99,101,30),2,near=0,full=1),
        phase('return',straight,12,metric=True,mode=1)])
    add('near-long','长时近端缺失',[phase('warmup',straight,20),phase('near-loss',stripe(straight,99,101,30),20,near=0,full=1),
        phase('return',straight,20,latched=True)])
    add('far-brief','短时远端缺失',[phase('warmup',straight,20),phase('far-loss',stripe(straight,49,51,30),4,near=1,full=0),
        phase('return',straight,12,metric=True,mode=1)])
    add('far-brief-above-cap','高规划速度突发远端缺失',[phase('warmup',straight,20),
        phase('far-loss',stripe(straight,49,51,30),4,near=1,full=0),phase('return',straight,12,latched=True)],requested=1)
    add('far-long','长时远端缺失',[phase('warmup',straight,20),phase('far-loss',stripe(straight,49,51,30),120,near=1,full=0),
        phase('return',straight,60,latched=True)])
    add('all-black','整帧失明',[phase('warmup',straight,20),phase('blind',bytes(SIZE),20,near=0,full=0),
        phase('return',straight,20,latched=True)])
    data=bytearray(straight)
    for y in range(120):data[y*188+94:(y+1)*188]=bytes([210])*94
    add('single-edge','学习后单边推算',[phase('warmup',straight,20),phase('inferred',bytes(data),8,near=0,full=0)])
    fixture=ROOT/'tests/vision/fixtures/tangent-preview';manifest=json.loads((fixture/'manifest.json').read_text('utf-8'))
    for item in manifest['files']:
        path=fixture/f"frame-{item['frame']:06d}.gray"
        if sha(path)!=item['sha256']:raise ValueError('Field preview hash mismatch')
        raw=path.read_bytes()
        if len(raw)!=94*60:raise ValueError('Field preview size mismatch')
        positive=item['frame'] in (40,41,43,44,45,65)
        for reflected in ((False,) if item['frame']==65 else (False,True)):
            data=bytes(raw[y//2*94+(93-x//2 if reflected else x//2)] for y in range(120) for x in range(188))
            expected=dict(ring=1,side=(1 if reflected else -1) if item['frame']!=65 else 1) if positive else dict(ring=0)
            add(f"preview-{item['frame']}-{'mirror' if reflected else 'original'}",'实拍静态预览',
                [phase('static',data,24,**expected)],source=dict(path=path.relative_to(ROOT).as_posix(),sha256=item['sha256'],
                    originalSize=[94,60],mirror=reflected,positive=positive))
    return result

def run(output):
    out=Path(output).resolve()
    if out.exists() or not any(out.is_relative_to(ROOT/d) for d in ('.build','.test-results')):
        raise ValueError('Use a NEW output folder under .build or .test-results')
    cases=scenarios();out.mkdir(parents=True)
    (out/'phase3_shadow_profile.h').write_text(header(json.loads(PROFILE.read_text('utf-8'))),encoding='utf-8')
    family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
    compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family=='msvc' else 'gcc')
    if not compiler:raise ValueError('Native compiler unavailable')
    names=['tools/testing/vision_scene_pipeline.c',*SOURCES,'demo/code/vision_control.c','demo/code/midterm_open_loop.c']
    command=compile_command(compiler,family,mode,[ROOT/p for p in names],[ROOT/'demo/code',out],out/'pipeline.exe')
    command[1:1]=[('/D' if family=='msvc' else '-D')+f for f in FLAGS]
    p=subprocess.run(command,cwd=out,capture_output=True,text=True,timeout=90)
    (out/'build.log').write_text(p.stdout+p.stderr,encoding='utf-8');p.check_returncode()
    timing_run=subprocess.run([str(out/'pipeline.exe'),'--timing-contract'],capture_output=True,text=True,timeout=30)
    (out/'timing-contract.log').write_text(timing_run.stdout+timing_run.stderr,encoding='utf-8');timing_run.check_returncode()
    results=[];failures=[]
    for case in cases:
        folder=out/case['name'];folder.mkdir()
        raw=b''.join(p['data']*p['count'] for p in case['phases']);(folder/'input.gray').write_bytes(raw)
        phases=[];start=0
        for phase_spec in case['phases']:
            phases.append(dict(tag=phase_spec['tag'],start=start,count=phase_spec['count'],expect=phase_spec['expect']))
            start+=phase_spec['count']
        save(folder/'input.json',dict(name=case['name'],family=case['family'],phases=phases,inputSha256=sha(folder/'input.gray'),source=case.get('source')))
        for bundle in (('synthetic','measured') if case.get('source') else ('synthetic',)):
            dest=folder/bundle;dest.mkdir()
            args=[out/'pipeline.exe',folder/'input.gray',dest/'trace.csv',dest/'masks.gray',dest/'rows.csv',bundle]
            if 'requested' in case:args.append(case['requested'])
            save(dest/'command.json',list(map(str,args)))
            p=subprocess.run(list(map(str,args)),cwd=out,capture_output=True,text=True,timeout=60)
            (dest/'run.log').write_text(p.stdout+p.stderr,encoding='utf-8');p.check_returncode()
            trace=table(dest/'trace.csv');geometry=table(dest/'rows.csv');issues=[]
            def check(condition,message):
                if not condition:issues.append(message)
            check(len(trace)==len(raw)//SIZE and len(geometry)==len(trace)*120,'Incomplete native outputs')
            check(len((dest/'masks.gray').read_bytes())==len(raw),'Incomplete masks')
            for i,r in enumerate(trace):
                check(int(r['index'])==i,'Trace order')
                check(r['bridge_status']=='0',f'frame{i}: odometry/vision input failed')
                for key in ('cap_m_s','target_speed_m_s','steering','near_peak_curvature','full_peak_curvature'):
                    check(math.isfinite(float(r[key])),f'frame{i}: nonfinite {key}')
                for key,bad in [('near','near_bad_row'),('full','full_bad_row')]:
                    if r[key]=='1':check(r['raw_valid']=='1' and int(r['confidence'])>=(50 if bundle=='synthetic' else 25),f'frame{i}: {key} accepted invalid source')
                if r['motor_inhibit']=='1' or int(r['supervisor_mode']) in (5,6):
                    check(r['left_ticks_10ms']==r['right_ticks_10ms']=='0',f'frame{i}: nonzero inhibited target')
                if r['planner_status']=='2':
                    check(r['candidate_matches']=='1',f'frame{i}: candidate identity/age mismatch')
                    check(0<=float(r['target_speed_m_s'])<=float(r['maximum_speed_m_s'])+1e-6,f'frame{i}: speed bound')
                    if r['far_active']=='1':check(float(r['cap_m_s'])<=float(r['near_cap_m_s'])+1e-6,f'frame{i}: far-loss cap')
            # Independently reconstruct synthetic metric coordinates, including
            # navigation guides and the documented median of three x samples.
            if bundle=='synthetic':
                for i,r in enumerate(trace):
                    for key,sampled in [('near',[110,100,90,80,70]),('full',[110,95,80,65,50])]:
                        if r[key]!='1':continue
                        selected=[geometry[i*120+y] for y in sampled];xs=[]
                        for g in selected:
                            flag=int(g['flags']);width=int(g['width'])
                            check(flag in (1,2,3,4),f'frame{i}: accepted absent row')
                            if flag==1:x=int(g['left'])+(width-1)/2
                            elif flag==2:x=int(g['right'])-(width-1)/2
                            else:x=(int(g['left'])+int(g['right']))/2
                            if r['ring_state']=='1' and int(r['guide_rows'])>=12 and int(g['guide'])<188:x=int(g['guide'])
                            xs.append(.01*x-.935)
                        expected=[xs[0]]+[sorted(xs[j-1:j+2])[1] for j in range(1,4)]+[xs[-1]]
                        for g,x in zip(selected,expected):check(abs(float(g[key+'_ground_x'])-x)<1e-5,f'frame{i}: {key} path/guide mismatch')
            for phase_spec in phases:
                tail=trace[phase_spec['start']+phase_spec['count']-1];expected=phase_spec['expect']
                for key,column in [('ring','ring_state'),('side','ring_side')]:
                    if key in expected:check(int(tail[column])==expected[key],f"{phase_spec['tag']}: {column}={tail[column]}, expected{expected[key]}")
                if bundle=='synthetic':
                    for key in ('near','full'):
                        if key in expected:check(int(tail[key])==expected[key],f"{phase_spec['tag']}: {key}={tail[key]}, expected{expected[key]}")
                    if expected.get('metric'):check(tail['near']==tail['full']=='1' and tail['planner_status']=='2',phase_spec['tag']+': no ready metric candidate')
                    if 'mode' in expected:check(int(tail['supervisor_mode'])==expected['mode'],phase_spec['tag']+': recovery mode')
                    if expected.get('latched'):check(int(tail['supervisor_mode']) in (5,6) and not int(tail['left_ticks_10ms']) and not int(tail['right_ticks_10ms']),phase_spec['tag']+': no retained stop latch')
            # Fail-safe correctness is different from continuous scene support.
            active=[r for r in trace if r['ring_state'] in ('1','2','3')]
            phase_summary=[]
            for spec in phases:
                selected=trace[spec['start']:spec['start']+spec['count']];last=selected[-1]
                phase_summary.append(dict(tag=spec['tag'],count=len(selected),near=sum(r['near']=='1' for r in selected),full=sum(r['full']=='1' for r in selected),
                    ready=sum(r['planner_status']=='2' for r in selected),endRing=int(last['ring_state']),endMode=int(last['supervisor_mode']),endGuide=int(last['guide_rows'])))
            if not case.get('source'):
                masks=(dest/'masks.gray').read_bytes()
                for spec,summary in zip(phases,phase_summary):
                    begin=(spec['start']+spec['count']-1)*SIZE
                    truth=raw[begin+24*188:begin+111*188];actual=masks[begin+24*188:begin+111*188]
                    intersection=sum(v>=210 and b==255 for v,b in zip(truth,actual))
                    union=sum(v>=210 or b==255 for v,b in zip(truth,actual))
                    summary['analyticWhiteIou']=intersection/union if union else 1.0
                    expected=[]
                    for y in range(24,111):
                        row=raw[begin+y*188:begin+(y+1)*188];white=[v>=210 for v in row]
                        expected.extend([white[0]]+[sum(white[x-1:x+2])>=2 for x in range(1,187)]+[white[-1]])
                    summary['filteredMaskErrors']=sum(a!=(b==255) for frame in range(spec['start'],spec['start']+spec['count']) for a,b in zip(expected,masks[frame*SIZE+24*188:frame*SIZE+111*188]))
                    check(summary['filteredMaskErrors']==0,spec['tag']+': analytic mask differs beyond documented 3-pixel vote')
            fault=next((r for r in trace if r['faults']!='0'),None)
            result=dict(name=case['name'],family=case['family'],bundle=bundle,frames=len(trace),contractPassed=not issues,issues=issues,
                near=sum(r['near']=='1' for r in trace),full=sum(r['full']=='1' for r in trace),ready=sum(r['planner_status']=='2' for r in trace),
                ringFrames=len(active),ringNear=sum(r['near']=='1' for r in active),ringFull=sum(r['full']=='1' for r in active),
                ringLegacyNear=sum(r['legacy_near']=='1' for r in active),ringLegacyFar=sum(r['legacy_far']=='1' for r in active),
                ringMetricInhibited=sum(r['motor_inhibit']=='1' for r in active),phases=phase_summary,
                source=case.get('source'),endMode=int(trace[-1]['supervisor_mode']),
                endRing=int(trace[-1]['ring_state']),endSide=int(trace[-1]['ring_side']),
                nearBadRows=dict(Counter(r['near_bad_row'] for r in trace if r['near']=='0')),
                fullBadRows=dict(Counter(r['full_bad_row'] for r in trace if r['full']=='0')),
                nearBadFlags=dict(Counter(r['near_bad_flag'] for r in trace if r['near_bad_row']!='-1')),
                firstFault={k:fault[k] for k in ('index','faults','candidate_status')} if fault else None,
                armEvents=sum(int(r['arm_event']) for r in trace))
            results.append(result);failures.extend(f'{case["name"]}/{bundle}: {i}' for i in issues)
            print(f"{'PASS' if not issues else 'FAIL'} {case['name']}/{bundle} n={len(trace)} near={result['near']} full={result['full']} ready={result['ready']} endMode={result['endMode']}",flush=True)
    continuous=[r for r in results if r['family'] in ('环岛完整状态序列','环岛亮灰地面派生')]
    not_ready=[r['name'] for r in continuous if r['ringMetricInhibited'] or r['endMode'] in (5,6) or r['armEvents']!=1 or any(p['ready']!=p['count'] or p['near']!=p['count'] or p['full']!=p['count'] for p in r['phases'] if p['tag']!='warmup')]
    report=dict(status='PASS' if not failures else 'FAIL',operationalAcceptance='NOT_READY' if not_ready else 'PASS',
        timingContract='PASS',cameraPeriodMs=20,encoderPeriodMs=10,timingContractTicks=80,cameraStallTicks=9,
        continuousMetricRingFailures=not_ready,nominalTickMs=20,syntheticFeedbackSpeedMps=.3,cases=results,failures=failures,compilerCommand=command,
        sourceSha256={p:sha(ROOT/p) for p in names+['tools/testing/vision_scene_integration.py',str(PROFILE.relative_to(ROOT))]+
            [p.relative_to(ROOT).as_posix() for p in (ROOT/'demo/code').glob('*.h')]},
        scope='Fresh native gray -> real metric IO/odometry -> real shadow planner, legacy controller parallel; contracts only, continuous metric scene support assessed separately.',
        hardwareAccessed=False,productionCodeModifiedByTest=False,
        limitations=['Synthetic map/policy is not camera calibration','94x60 historical previews upscaled 2x, static repeats and derived mirrors, not native-resolution temporal ring recordings',
            'Measured bundle on historical previews is a cross-camera compatibility probe, not a valid physical projection accuracy test','No closed-loop vehicle dynamics, board WCET, PWM or autonomous orbit acceptance'])
    save(out/'report.json',report)
    details=['# 场景逐项记录','','| 场景/配置 | 契约 | near/full可用帧 | 规划候选帧 | 结束监督状态 |','| --- | --- | ---: | ---: | ---: |']
    for r in results:details.append(f"| [{r['name']}/{r['bundle']}]({r['name']}/{r['bundle']}/trace.csv) | {'通过' if r['contractPassed'] else '失败'} | {r['near']}/{r['full']} / {r['frames']} | {r['ready']} | {r['endMode']} |")
    for r in results:
        details+=['',f"## {r['name']} / {r['bundle']}",'','| 阶段 | 帧数 | near/full | 规划候选 | 末帧环岛/监督状态 | 合成白区IoU |','| --- | ---: | ---: | ---: | --- | ---: |']
        for p in r['phases']:
            iou=f"{100*p['analyticWhiteIou']:.2f}%" if 'analyticWhiteIou' in p else '无真值'
            details.append(f"| {p['tag']} | {p['count']} | {p['near']}/{p['full']} | {p['ready']} | {p['endRing']}/{p['endMode']} | {iou} |")
        details+=['',*r['issues']]
    (out/'cases.md').write_text('\n'.join(details)+'\n',encoding='utf-8')
    write_user_report(out,report)
    return report

def write_user_report(out,report):
    examples=[('ring-left','entry'),('ring-left','exit'),('ring-left-gray-floor','entry'),('crossing','cross'),('preview-40-original','static')]
    parts=['<svg xmlns="http://www.w3.org/2000/svg" width="820" height="1480" viewBox="0 0 820 1480">',
        '<rect width="100%" height="100%" fill="#fafaf8"/><g font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="16" fill="#25332d">']
    for k,(name,tag) in enumerate(examples):
        meta=json.loads((out/name/'input.json').read_text('utf-8'));p=next(p for p in meta['phases'] if p['tag']==tag)
        index=p['start']+p['count']-1;y=28+k*290
        parts.append(f'<text x="16" y="{y}">{escape(name)} / {escape(tag)}：灰度输入 → 实际二值图</text>')
        for c,path in enumerate((out/name/'input.gray',out/name/'synthetic/masks.gray')):
            data=path.read_bytes()[index*SIZE:(index+1)*SIZE]
            uri='data:image/png;base64,'+base64.b64encode(png_gray(188,120,data)).decode()
            parts.append(f'<image x="{16+c*400}" y="{y+12}" width="376" height="240" href="{uri}" style="image-rendering:pixelated"/>')
    (out/'scene-examples.svg').write_text(''.join(parts)+'</g></svg>',encoding='utf-8')
    active=[r for r in report['cases'] if r['family'] in ('环岛完整状态序列','环岛亮灰地面派生','侧裁切环岛')]
    lines=['# 环岛及视觉—速度规划器集成报告','',
        f"**接口与保护契约：{report['status']}；连续米制环岛驾驶验收：{report['operationalAcceptance']}。** 本轮 {len(report['cases'])} 个场景/配置组合，失败 {len(report['failures'])} 条契约断言。识别出环岛不等于规划器已接纳路径。",'',
        '额外时序契约：相机20ms、编码器/规划10ms，共80个周期及9个相机停更周期；过期缓存抑制输出，重复图像不能刷新历史，99ms接纳、100ms过期。', '',
        '输入从灰度图开始，运行当前原生C视觉、near/full适配、累计编码器时间对齐及影子速度规划器，旁路记录旧控制器。没有直接注入行边界或near/far有效位。','',
        '| 环岛场景/配置 | 环岛状态帧 | 旧控制near/far可用 | 米制near/full可用 | 米制电机抑制帧 |','| --- | ---: | ---: | ---: | ---: |']
    for r in active:lines.append(f"| {r['name']} / {r['bundle']} | {r['ringFrames']} | {r['ringLegacyNear']}/{r['ringLegacyFar']} | {r['ringNear']}/{r['ringFull']} | {r['ringMetricInhibited']} |")
    positive=[r for r in report['cases'] if r['bundle']=='synthetic' and r['source'] and r['source']['positive']]
    negative=[r for r in report['cases'] if r['bundle']=='synthetic' and r['source'] and not r['source']['positive']]
    recognized=sum(r['endRing']==1 and r['endSide']==(1 if r['source']['mirror'] or r['name'].startswith('preview-65-') else -1) for r in positive)
    rejected=sum(r['endRing']==0 for r in negative)
    admitted=[r for r in report['cases'] if r['bundle']=='measured' and r['source'] and r['source']['positive'] and r['near']==r['full']==r['frames']]
    lines+=['', f"完整状态合成序列中，{len(report['continuousMetricRingFailures'])} 个出现米制抑制/锁存，；连续场景状态以本报告首行验收结论为准。分割误差和路径结果见逐阶段记录。", '',
        f'实拍静态预览包含6个正例原姿态、4个反例原姿态，以及9个派生镜像。{recognized}/{len(positive)}个正例/派生姿态识别出预期环岛方向，{rejected}/{len(negative)}个反例/派生姿态末帧未进入环岛。它们不是19张独立实拍原图。当前影子映射接纳其中{len(admitted)}个正例/派生姿态；跨安装条件的映射结果不能作为物理准确度。', '',
        '低速短暂far丢失可降级并恢复；另设高规划速度场景：请求1m/s、失效前规划速度约0.565m/s，突然降至0.5m/s上限时触发不可行保护。此处编码器为显式0.3m/s合成反馈，未模拟车辆追踪规划速度；这不是对真实制动距离的验证。']
    lines+=['','![本轮场景输入与真实二值输出](scene-examples.svg)','',
        '前四行是解析合成图及亮灰地面派生图；末行来自已有94×60实拍预览，最近邻放大到188×120，不能恢复原始相机细节。',
        '合成图使用显式合成映射；measured组合使用当前影子配置，在旧预览上仅用于跨安装条件的接纳诊断，不代表该预览具备同一相机标定。', '',
        'near/full检查指定采样行；普通道路要求真实双边，有当前环岛候选或环岛状态时可采用经宽度/边界检查的单边推算或完整锚点补线，入岛采用当前有效引导。来源标记保持不变。两套near/far仍不能按名字视为同一信号。短时near缺失、far缺失与长时锁存通过真实灰度条带/失明图触发；具体结果和失败保留在逐阶段记录。', '',
        '合成白区IoU比较阶段末帧第24～110行；另外每帧均与解析白区经三像素多数滤波的期望结果比对，差异必须为0。这不是实拍图准确率。这里的帧数含静态重复和派生镜像，不能写成独立图像准确率。无板端耗时、硬件动作、完整真实环岛视频或闭环绕行验收。', '',
        '[逐场景与阶段结果](cases.md) · [结构化结果及源码绑定](report.json)','']
    if report['failures']:lines+=['## 未通过断言','',*['- '+s for s in report['failures']],'']
    (out/'user-report.md').write_text('\n'.join(lines),encoding='utf-8')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=Path.cwd()/'vision-scene-integration')
    args=parser.parse_args()
    try:sys.exit(0 if run(args.output)['status']=='PASS' else 1)
    except (ValueError,OSError,subprocess.SubprocessError) as e:
        print('SCENE INTEGRATION ERROR: '+str(e),file=sys.stderr);sys.exit(2)
