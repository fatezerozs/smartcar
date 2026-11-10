"""A concise Chinese user report derived only from this persisted run/artifacts."""
from collections import Counter
from datetime import datetime,timedelta,timezone
import base64
from html import escape
from pathlib import Path
import struct
import zlib

from common import read_json

LABELS={'PASS':'通过','FAIL':'失败','ERROR':'错误','BLOCKED':'阻塞','TIMEOUT':'超时',
        'SKIPPED':'跳过','MANUAL_PENDING':'待人工验证'}
COLORS={'PASS':'#26734d','FAIL':'#b93832','ERROR':'#b93832','BLOCKED':'#9a6718',
        'TIMEOUT':'#9a6718','SKIPPED':'#68717a','MANUAL_PENDING':'#68717a'}
ACCEPTANCE_LABELS={'PASS':'通过','NOT_READY':'未就绪','NOT_EVALUATED':'未执行','ERROR':'证据错误'}
NAMES={'environment':'运行环境','compiler-environment':'编译环境','vision':'基础视觉识别',
       'vision-contract':'图像预处理一致性','vision-image-regression':'原图边界回归',
       'vision-ground-merge-regression':'灰地面误合并故障用例',
       'performance-vision-images':'图像耗时与局部边界质量','vision-benchmark-contract':'图像测量数据完整性',
       'phase3-shadow-preparation':'影子配置与候选构建','framework':'测试框架',
       'control':'视觉控制','turn-route':'弯道控制','roundabout':'环岛识别',
       'roundabout-projection':'环岛投影','tangent-preview':'路径预览','static-safety':'静态检查',
       'hardware-acceptance':'硬件验收'}
NAMES['vision-scene-integration']='图像到near/full与速度规划器集成'

def safe(value):return str(value).replace('|','\\|').replace('\n',' ')

def status_chart(path,counts):
    items=[(k,v) for k,v in counts.items() if v]
    height=55+len(items)*34
    parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="640" height="{height}" viewBox="0 0 640 {height}">',
           '<rect width="100%" height="100%" fill="#fafaf8"/>',
           '<g font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" fill="#25332d">',
           '<text x="18" y="27">本轮测试项结果（包含环境检查）</text>']
    maximum=max(counts.values(),default=1) or 1
    for i,(key,value) in enumerate(items):
        y=46+i*34;width=400*value/maximum
        parts += [f'<text x="18" y="{y+17}">{escape(LABELS.get(key,key))}</text>',
                  f'<rect x="120" y="{y}" width="{width:.2f}" height="23" rx="3" fill="{COLORS.get(key,"#68717a")}"/>',
                  f'<text x="{132+width:.2f}" y="{y+17}">{value}</text>']
    path.write_text(''.join(parts)+'</g></svg>',encoding='utf-8')

def png_gray(width,height,pixels):
    def chunk(name,data):return struct.pack('!I',len(data))+name+data+struct.pack('!I',zlib.crc32(name+data)&0xffffffff)
    scan=b''.join(b'\0'+pixels[y*width:(y+1)*width] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',width,height,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')

def vision_picture(folder,images,report):
    """Embed only this run's original bytes and native masks, with frame labels."""
    raw_path=folder/'original.gray';mask_path=folder/'masks.gray'
    if not raw_path.is_file() or not mask_path.is_file():return None
    raw=raw_path.read_bytes();masks=mask_path.read_bytes();size=188*120
    if len(raw)!=report['framesVerified']*size or len(masks)!=len(raw):return None
    indices=[c['index'] for c in report['audit'].get('visuallyReviewedClearSceneCases',[])][:2]
    if not indices or any(type(i) is not int or not 0<=i<report['framesVerified'] for i in indices):return None
    height=64+len(indices)*280
    parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="820" height="{height}" viewBox="0 0 820 {height}">',
           '<rect width="100%" height="100%" fill="#fafaf8"/>',
           '<g font-family="Segoe UI,Microsoft YaHei,sans-serif" font-size="15" fill="#25332d">',
           '<text x="15" y="24">本轮输入原图 / 当前算法实际二值图</text>']
    for n,index in enumerate(indices):
        y=48+n*280
        parts.append(f'<text x="15" y="{y}">第 {index} 帧（编号从0开始）</text>')
        for col,source in enumerate((raw,masks)):
            blob=png_gray(188,120,source[index*size:(index+1)*size])
            uri='data:image/png;base64,'+base64.b64encode(blob).decode()
            parts.append(f'<image x="{15+col*402}" y="{y+12}" width="376" height="240" href="{uri}" style="image-rendering:pixelated"/>')
    name='vision-examples.svg';(images/name).write_text(''.join(parts)+'</g></svg>',encoding='utf-8')
    return name

def write_user_report(run_path):
    run_path=Path(run_path);run=read_json(run_path);root=run_path.parent
    images=root/'report-images';images.mkdir(exist_ok=True)
    counts=Counter(s['status'] for s in run['suites']);status_chart(images/'results.svg',counts)
    started=datetime.fromisoformat(run['startedAt'].replace('Z','+00:00'))
    local=started.astimezone(timezone(timedelta(hours=8))).strftime('%Y-%m-%d %H:%M:%S')
    status=LABELS.get(run['status'],run['status'])
    conclusion='本轮必需测试通过。' if run['status']=='PASS' else '本轮存在未通过或未完成的项目，见下表。'
    git=run.get('git',{});tools=run.get('environment',{}).get('tools',{})
    lines=['# 回归报告','',f'**结果：{status}。** {conclusion}',
           f"测试时间：{local}（北京时间）；总耗时 {run['durationSeconds']:.1f} 秒。",'',
           '![本轮各状态测试项数量](report-images/results.svg)','',
           '| 检查内容 | 本轮结果 |','| --- | --- |']
    if 'acceptance' in run:
        a=run['acceptance'];accepted=ACCEPTANCE_LABELS[a['status']]
        lines[2]=(f"**软件测试：{status}；场景验收：{accepted}；命令结果：{'通过' if run['commandPassed'] else '未通过'}。** "
                  '软件检查和场景验收分别记录，场景结论仅限本轮声明的检查范围。')
    if len(run['suites'])<=15:
        for s in run['suites']:
            name=NAMES.get(s['id'],s['id'])
            lines.append(f"| {safe(name)} | {LABELS.get(s['status'],s['status'])} |")
    else:
        layers={'environment':'环境','unit':'单元测试','contract':'接口与数据契约','integration':'模块组合回归','build':'构建与静态检查','performance':'性能与质量','hardware':'硬件'}
        for layer in dict.fromkeys(s['layer'] for s in run['suites']):
            group=[s for s in run['suites'] if s['layer']==layer];summary=Counter(s['status'] for s in group)
            detail='，'.join(f'{LABELS.get(k,k)} {v}' for k,v in summary.items())
            lines.append(f"| {layers.get(layer,layer)}（{len(group)}项） | {detail} |")
    problems=[s for s in run['suites'] if s['status']!='PASS']
    if problems:
        lines += ['', '## 未通过或未完成的项目','', '| 项目 | 原因 / 详细记录 |','| --- | --- |']
        for s in problems:
            lines.append(f"| {safe(NAMES.get(s['id'],s['id']))} | {safe(s['reason'])}；[输出](suites/{s['id']}/stdout.log) / [错误](suites/{s['id']}/stderr.log) |")
    changed_inputs='inputsAtFinish' in run.get('environment',{})
    if changed_inputs:lines += ['', '**本轮执行期间输入发生变化，不能用于验收；图像测量摘要不予展示。**']
    if 'acceptance' in run:
        lines+=['','## 场景验收与放行判断','',
            '| 场景检查 | 本轮验收状态 | 原因 |','| --- | --- | --- |']
        for check in run['acceptance']['checks']:
            evidence=f"[证据]({check['reportPath']})" if check['reportSha256'] else '无可用验收证据'
            lines.append(f"| {safe(check['name'])} | {ACCEPTANCE_LABELS[check['status']]} | {safe(check['reason'])}；{evidence} |")
        if not run['acceptance']['checks']:lines.append('| 未声明场景检查 | 未执行 | 本轮不能据此判断场景验收通过 |')
        lines += ['',f"本轮软件范围放行资格（releaseEligible）：{'满足' if run['releaseEligible'] else '未满足'}。该字段不授予烧录或真实电机运行权限。",
            '未执行的检查不继承历史结果；普通回归可单独验证软件，显式要求场景验收时，未就绪、未执行或证据错误都会使命令未通过。']
    scene=next((s for s in run['suites'] if s['id']=='vision-scene-integration' and s['status']=='PASS' and not changed_inputs and 'acceptance' not in run),None)
    scene_path=root/'suites/vision-scene-integration/artifacts/work/vision-scene-integration/report.json'
    if scene and scene_path.is_file():
        data=read_json(scene_path)
        lines+=['','## 场景集成与连续驾驶验收','',
            f"本轮 {len(data['cases'])} 个图像/配置组合的接口与保护契约通过。"]
        if data['operationalAcceptance']!='PASS':
            lines+=['**连续米制过环岛尚未通过验收。** '
                f"{len(data['continuousMetricRingFailures'])} 个完整状态合成序列出现米制输出抑制或锁存；视觉识别通过不能替代这项验收。"]
        lines+=['[场景图像、逐阶段数据和限制](suites/vision-scene-integration/artifacts/work/vision-scene-integration/user-report.md)']
    measured=next((s for s in run['suites'] if s['id']=='performance-vision-images' and s['status']=='PASS' and not changed_inputs),None)
    if measured:
        work=root/'suites/performance-vision-images/artifacts/work';reports=[]
        for name in ('vision-image-performance','vision-image-performance-route-active'):
            folder=work/name
            if (folder/'report.json').is_file():reports.append((folder,read_json(folder/'report.json')))
        if reports:
            lines += ['', '## 图像回归结果','',
                '耗时来自本机回放；局部边界误差来自已有参考带。近/远预览可用率与识别准确度分别报告。','',
                '| 状态 | 图像均值 / P95（ms） | 近预览可用 | 完整预览可用 | 平均 / 最大边界误差（像素） |',
                '| --- | ---: | ---: | ---: | ---: |']
            for folder,r in reports:
                quality=next(folder.glob('*quality*-result.json'),None)
                metrics={m['id']:m['value'] for m in read_json(quality)['metrics']} if quality else {}
                errors=f"{metrics['boundary-mae']:.3f} / {metrics['boundary-max-error']:.3f}" if 'boundary-mae' in metrics and 'boundary-max-error' in metrics else '未测得'
                count=r['framesVerified'];a=r['audit'];t=r['timing']['vision_ms']
                lines.append(f"| 路线识别{'启用' if r['routeRecognitionEnabled'] else '关闭'} | {t['mean']:.3f} / {t['p95']:.3f} | {a['near']['accepted']}/{count} | {a['full']['accepted']}/{count} | {errors} |")
            folder,r=reports[0]
            lines += ['',f"局部参考：每次回放 {r['qualityRowsPerReplay']} 组边界；它们在算法试验后选取，不能据此声称全图或独立测试集准确率。",
                      'P95表示本轮95%的帧耗时不超过该值，剩余帧可能更慢。',
                      '本轮没有经过核对的版本间对照或批准的耗时基线，因此不判断性能提高或退化。']
            cases=[c for c in r['audit'].get('visuallyReviewedClearSceneCases',[]) if c['row54Flags']!=3 or not c['rawValid']]
            if cases:
                lines += ['', '**仍有需处理的案例：** 在目视复查过的清晰直道样例中，下列帧未提供真实双边或整帧被拒绝；测试通过不代表这些问题已修复。','',
                          '| 帧号 | 第54行 | 整帧有效 |','| ---: | --- | --- |']
                flags={0:'无有效边界',1:'单边推算',2:'单边推算',3:'真实双边',4:'路口补线'}
                lines += [f"| {c['index']} | {flags.get(c['row54Flags'],c['row54Flags'])} | {'是' if c['rawValid'] else '否'} |" for c in cases]
            picture=vision_picture(folder,images,r)
            if picture:lines += ['',f'![本轮原始灰度与实际二值图对照](report-images/{picture})',
                                  '左：本轮使用的原始灰度图；右：本轮原生算法二值输出。这里只展示选定案例，不能代表整段图像的准确率。']
    lines += ['', '## 验证范围','',
              '结论仅适用于上表列出的项目。未执行的测试、板端时序和实车行驶不能视为通过。',
              f"版本：`{git.get('sha','unknown')[:12]}`；工作区{'含未提交改动' if git.get('dirty') else '无未提交改动'}；工具：`{tools.get('compilerFamily','未记录')}` / `{tools.get('mode','未记录')}`。",
              '', '[完整测试明细](summary.md) · [原始结果与执行参数](run.json) · [环境记录](environment.json)','']
    path=root/'user-report.md';path.write_text('\n'.join(lines),encoding='utf-8');return path
