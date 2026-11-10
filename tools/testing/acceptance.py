"""Aggregate declared scenario evidence from this run only.

Software test status stays independent. Missing coverage and unreadable or
contradictory evidence cannot grant release eligibility.
"""
from common import inside, read_json, sha256

SCOPE='Only the scenario checks declared in this catalog; not physical vehicle, flashing or hardware authorization.'
STATES=('PASS','NOT_READY','NOT_EVALUATED','ERROR')


def collect_acceptance(catalog,results,output,*,inputs_unchanged=True):
    selected={r['id']:r for r in results}
    checks=[]
    for suite in catalog.values():
        for spec in suite.get('acceptanceChecks',[]):
            report_path=f"suites/{suite['id']}/{spec['reportPath']}"
            check={'id':spec['id'],'suiteId':suite['id'],'name':spec['name'],'scope':spec['scope'],
                   'status':'NOT_EVALUATED','reason':'本轮未执行该场景检查。','blockingCases':[],
                   'reportPath':report_path,'reportSha256':None}
            result=selected.get(suite['id'])
            if not result:
                checks.append(check);continue
            if not inputs_unchanged:
                check.update(status='ERROR',reason='执行期间输入变化，本轮证据不可用于场景验收。')
            elif result['status']!='PASS':
                check['reason']='相关测试未通过或未完成：'+result['status']
            else:
                try:
                    path=inside(output,report_path)
                    digest=sha256(path);data=read_json(path)
                    if sha256(path)!=digest:raise ValueError('报告在读取期间变化')
                    if not isinstance(data,dict):raise ValueError('报告应为JSON对象')
                    status=data[spec['statusField']]
                    if status not in ('PASS','NOT_READY'):raise ValueError('未知场景验收状态')
                    if data.get('status')!='PASS':raise ValueError('场景报告的软件状态与套件通过状态不一致')
                    cases=data[spec['blockingCasesField']]
                    if not isinstance(cases,list) or any(not isinstance(c,str) or not c.strip() for c in cases):
                        raise ValueError('未通过场景清单格式错误')
                    if len(cases)!=len(set(cases)):raise ValueError('未通过场景清单重复')
                    if (status=='PASS')!= (not cases):raise ValueError('场景状态与未通过场景清单矛盾')
                    check.update(status=status,blockingCases=cases,reportSha256=digest,
                        reason='声明范围内的场景检查通过。' if status=='PASS' else '场景尚未就绪：'+'、'.join(cases))
                except (OSError,ValueError,KeyError,TypeError) as error:
                    check.update(status='ERROR',reason='场景证据不可用：'+str(error))
            checks.append(check)
    status=next((s for s in ('ERROR','NOT_READY','NOT_EVALUATED') if any(c['status']==s for c in checks)),
                'PASS' if checks else 'NOT_EVALUATED')
    return {'status':status,'scope':SCOPE,'checks':checks}


def decision(software_passed,acceptance,compiler,profile,require_acceptance=False):
    ready=acceptance['status']=='PASS'
    return {'softwarePassed':software_passed,
            'acceptanceRequired':require_acceptance,
            'commandPassed':software_passed and acceptance['status']!='ERROR' and (ready or not require_acceptance),
            'releaseEligible':software_passed and ready and compiler=='msvc' and profile in ('Host','PreFlash')}
