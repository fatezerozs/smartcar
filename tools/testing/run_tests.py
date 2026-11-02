"""Shared local/CI orchestration. PowerShell is the public Windows entrypoint."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED
from datetime import datetime, timezone
import os
from pathlib import Path
import platform
import subprocess
import sys
import time
import traceback
import uuid

from common import ROOT, aggregate_status, load_catalog, select_suites, sha256, validate_schema, write_json
from process_runner import ProcessTimeout, resource_locks
from providers import Context, Execution, SuiteProblem
from reporter import write_report


def utc():
    return datetime.now(timezone.utc).isoformat(timespec='milliseconds')


def git_state():
    def git(*args):
        return subprocess.check_output(['git','-C',str(ROOT),*args],text=True,encoding='utf-8',errors='replace',
                                       stderr=subprocess.PIPE,timeout=30).strip()
    status = git('status','--porcelain=v1','--untracked-files=normal')
    return {'sha':git('rev-parse','HEAD'),'branch':git('rev-parse','--abbrev-ref','HEAD'),
            'dirty':bool(status),'status':status,
            'workingDiffSha256':__import__('hashlib').sha256(subprocess.check_output(
                ['git','-C',str(ROOT),'diff','HEAD','--binary'],stderr=subprocess.PIPE,timeout=30)).hexdigest()}


def input_snapshot():
    files = {}
    for directory in ('demo/code','demo/user','demo/mdk/base','demo/mdk/scf','libraries','tools','tests'):
        for path in sorted((ROOT/directory).rglob('*')):
            if path.is_file() and path.suffix.lower() in ('.c','.h','.s','.inc','.lib','.a','.ps1','.psm1','.cmd','.py','.json','.gray','.scf','.sct','.uvprojx'):
                if any(part in ('output','__pycache__','Objects','Listings') for part in path.relative_to(ROOT).parts):
                    continue
                files[path.relative_to(ROOT).as_posix()] = sha256(path)
    return files


def make_result(suite, folder):
    return {'schemaVersion':'1.0','id':suite['id'],'status':'ERROR','evidenceClass':suite['evidenceClass'],
            'layer':suite['layer'],'category':suite['category'],'required':suite['required'],'reason':'',
            'durationSeconds':0,'exitCode':None,'startedAt':utc(),'finishedAt':utc(),'processes':[],
            'variants':[],'logDirectory':str(folder),'dependsOn':suite['dependsOn'],
            'artifacts':[],'details':{'scope':suite.get('scope',''),'executionMethod':suite.get('executionMethod',suite['entrypoint']['type'])}}


def execute_suite(suite, output, context, dependencies):
    folder = output / 'suites' / suite['id']
    folder.mkdir(parents=True)
    result = make_result(suite,folder)
    started = time.monotonic()
    with (folder/'stdout.log').open('wb') as stdout, (folder/'stderr.log').open('wb') as stderr:
        blocked = [r['id']+':'+r['status'] for r in dependencies if r['status']!='PASS']
        if blocked:
            result.update(status='BLOCKED',reason='Unmet dependencies: '+', '.join(blocked))
        else:
            try:
                execution = Execution(suite,folder,context,result,stdout,stderr)
                resources = list(suite['resources'])
                if context.options.compiler=='msvc' and context.options.mode=='asan' and suite['entrypoint']['type'] in ('c','compiler-environment'):
                    resources.append('msvc-asan-pdb')
                result['details']['effectiveResources'] = resources
                with resource_locks(ROOT/'.test-results/.locks',resources,suite['timeoutSeconds']):
                    execution.execute()
                result.update(status='PASS',reason='Completed within declared software evidence scope')
            except SuiteProblem as error:
                result.update(status=error.status,reason=str(error))
            except ProcessTimeout as error:
                result.update(status='TIMEOUT',reason=str(error))
            except Exception as error:
                result.update(status='ERROR',reason=type(error).__name__+': '+str(error))
                stderr.write(traceback.format_exc().encode('utf-8'))
        result['durationSeconds'] = round(time.monotonic()-started,6)
        result['finishedAt'] = utc()
        stdout.write(('['+result['status']+'] '+result['reason']+'\n').encode('utf-8'))
    result['details']['lastRawExitCode'] = result['exitCode']
    result['exitCode'] = {'PASS':0,'FAIL':1,'ERROR':2,'TIMEOUT':124,'BLOCKED':3,'SKIPPED':0,'MANUAL_PENDING':4}[result['status']]
    # Before Python 3.13, a glob ending in ** returns directories only.
    patterns = [p.replace('\\','/') for p in suite['artifacts']]
    patterns = [p+'/*' if p.endswith('/**') else p for p in patterns]
    result['artifacts'] = sorted({p.relative_to(folder).as_posix() for pattern in patterns
                                  for p in folder.glob(pattern) if p.is_file()})
    write_json(folder/'result.json',result)
    print(f"{result['status']:14} {suite['id']:27} {result['durationSeconds']:8.3f}s  {result['reason']}",flush=True)
    return result


def schedule(catalog, order, output, context, jobs=1):
    pending, completed, running = list(order), {}, {}
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        while pending or running:
            for sid in pending[:]:
                deps = catalog[sid]['dependsOn']
                if len(running)>=jobs:
                    break
                if all(dep in completed for dep in deps):
                    future = pool.submit(execute_suite,catalog[sid],output,context,[completed[d] for d in deps])
                    running[future] = sid
                    pending.remove(sid)
            if not running:
                raise RuntimeError('No runnable suite; dependency scheduling is inconsistent')
            done,_ = wait(running,return_when=FIRST_COMPLETED)
            for future in done:
                sid = running.pop(future)
                completed[sid] = future.result()
    return [completed[sid] for sid in order]


def parser():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--profile',choices=['Host','PreFlash','Diagnostic'],default='Host')
    p.add_argument('--suite',action='append',default=[])
    p.add_argument('--variant')
    p.add_argument('--list',action='store_true')
    p.add_argument('--validate',action='store_true')
    p.add_argument('--compiler',choices=['msvc','gcc'],default='msvc')
    p.add_argument('--mode',choices=['optimized','asan'],default='optimized')
    p.add_argument('--jobs',type=int,choices=range(1,5),default=1)
    p.add_argument('--vcvars')
    p.add_argument('--uv4')
    p.add_argument('--arm-bin')
    return p


def main(argv=None):
    args=parser().parse_args(argv)
    try:
        catalog = load_catalog()
        if args.validate:
            from benchmark import validate_benchmark
            for suite in catalog.values():
                if suite['entrypoint']['type']=='benchmark':
                    from common import read_json
                    validate_benchmark(read_json(ROOT/suite['entrypoint']['path']))
            print(f'Manifest/schema/path/DAG validation passed: {len(catalog)} suites')
            return 0
        if args.list:
            for suite in catalog.values():
                print(f"{suite['id']:27} {suite['layer']:14} {suite['evidenceClass']:15} {','.join(suite['profiles']) or 'explicit-only'}")
            return 0
        order = select_suites(catalog,args.profile,args.suite)
        for sid in args.suite:
            # Explicit selection makes that target required for this invocation.
            catalog[sid.lower()]['required'] = True
        if args.variant:
            if len(args.suite)!=1 or catalog[args.suite[0].lower()]['entrypoint']['type']!='c':
                raise ValueError('--variant requires exactly one explicit C suite')
            entry = catalog[args.suite[0].lower()]['entrypoint']
            entry['variants'] = [v for v in entry['variants'] if v['id']==args.variant]
            if not entry['variants']:
                raise ValueError('Unknown C build variant: '+args.variant)
        run_id = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S')+'-'+uuid.uuid4().hex[:10]
        output = ROOT/'.test-results'/run_id
        output.mkdir(parents=True)
        started, monotonic = utc(), time.monotonic()
        git = git_state()
        context = Context(args)
        environment = {'platform':platform.platform(),'machine':platform.machine(),
                       'processor':platform.processor(),'logicalCpuCount':os.cpu_count(),'tools':context.tools,
                       'inputFiles':input_snapshot()}
        write_json(output/'environment.json',environment)
        print('Run directory: '+str(output),flush=True)
        results = schedule(catalog,order,output,context,args.jobs)
        status = aggregate_status(results)
        passed = status=='PASS'
        profile = 'Suite' if args.suite and args.profile!='Diagnostic' else args.profile
        risks = ['Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.',
                 'Hardware acceptance and manual ISR/DMA/critical-section review remain pending.']
        final_inputs = input_snapshot()
        if final_inputs != environment['inputFiles']:
            status, passed = 'ERROR', False
            environment['inputsAtFinish'] = final_inputs
            risks.append('Source/fixture/tool inputs changed during execution; this run cannot be used for acceptance.')
        if args.compiler=='gcc':
            risks.append('GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.')
        if profile!='PreFlash':
            risks.append('This run does not claim the complete PreFlash target-build/static profile.')
        for result in results:
            if result['status']!='PASS':
                risks.append(result['id']+': '+result['status']+' — '+result['reason'])
        run = {'schemaVersion':'1.0','runId':run_id,'profile':profile,'startedAt':started,'finishedAt':utc(),
               'durationSeconds':round(time.monotonic()-monotonic,6),'git':git,
               'manifestSha256':sha256(ROOT/'tests/suites/catalog.json'),'environment':environment,'status':status,
               'softwarePassed':passed,'releaseEligible':passed and args.compiler=='msvc' and profile in ('Host','PreFlash'),
               'scope':'Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.',
               'risks':risks,'suites':results}
        validate_schema(run,'result.schema.json')
        write_json(output/'environment.json',environment)
        write_json(output/'run.json',run)
        write_report(output/'run.json')
        print('Report: '+str(output/'summary.md'),flush=True)
        return 0 if passed else 1
    except (ValueError,RuntimeError,OSError,subprocess.SubprocessError) as error:
        print('FRAMEWORK ERROR: '+str(error),file=sys.stderr)
        return 2


if __name__=='__main__':
    sys.exit(main())
