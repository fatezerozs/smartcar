"""Render Markdown exclusively from the persisted JSON run."""
from common import read_json


def clean(value):
    return str(value).replace('|','\\|').replace('\n',' ')


def write_report(run_path):
    run = read_json(run_path)
    lines = ['# SmartCar test report', '',
             f"Run: `{run['runId']}` · Profile: `{run['profile']}` · Status: **{run['status']}**", '',
             f"Git: `{run['git'].get('sha','unknown')}` · branch: `{run['git'].get('branch','unknown')}` · dirty: `{run['git'].get('dirty')}`", '',
             f"Start: {run['startedAt']} · duration: {run['durationSeconds']:.3f} seconds", '',
             f"Software passed: `{run['softwarePassed']}` · release eligible within profile: `{run['releaseEligible']}`", '',
             run['scope'], '',
             '| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |',
             '| --- | --- | --- | --- | --- | --- | --- | --- |']
    if 'acceptance' in run:
        a=run['acceptance']
        lines[2]+=f" · Scenario acceptance: **{a['status']}** · Command passed: `{run['commandPassed']}`"
    for s in run['suites']:
        logs = f"[stdout](suites/{s['id']}/stdout.log) / [stderr](suites/{s['id']}/stderr.log)"
        lines.append('| '+' | '.join(map(clean,[s['id'],s['layer']+' / '+s['evidenceClass'],s['required'],s['status'],
                                               f"{s['durationSeconds']:.3f}",s['exitCode'],s['reason']]))+' | '+logs+' |')
    lines += ['', '## Evidence and remaining scope', ''] + ['- '+risk for risk in run['risks']]
    if 'acceptance' in run:
        lines+=['','## Scenario acceptance','',run['acceptance']['scope'],'',
            '| Check | Suite | Status | Reason | Evidence |','| --- | --- | --- | --- | --- |']
        for check in run['acceptance']['checks']:
            evidence=f"[report]({check['reportPath']}) / `{check['reportSha256']}`" if check['reportSha256'] else 'not available'
            lines.append('| '+' | '.join(clean(check[k]) for k in ('id','suiteId','status','reason'))+' | '+evidence+' |')
    lines += ['', 'Tool versions and machine details: [environment.json](environment.json).',
              'Macro combinations, source paths, process arguments and build variants: [run.json](run.json).',
              'No history is counted as a result of this run. No firmware flashing or hardware operation was performed.', '']
    run_path.with_name('summary.md').write_text('\n'.join(lines),encoding='utf-8')
    from user_report import write_user_report
    write_user_report(run_path)
