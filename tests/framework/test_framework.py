"""Infrastructure behavior tests; only synthetic inputs, no dataset or hardware."""
from __future__ import annotations

import copy
import contextlib
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/testing'))
from common import aggregate_status, inside, load_catalog, read_json, select_suites, validate_schema, write_json
from benchmark import compare, statistic, summarize, validate_benchmark
from process_runner import ProcessTimeout, resource_locks, run_process
from providers import Context, Execution, SuiteProblem
from reporter import write_report
from user_report import write_user_report
from run_tests import execute_suite, make_result, schedule, input_snapshot, main as plan_main
from acceptance import collect_acceptance, decision


class Scratch(unittest.TestCase):
    def setUp(self):
        parent=ROOT/'.build/framework-tests'
        parent.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.folder=Path(self.temp.name)


class UserReportTests(Scratch):
    def test_scene_contract_pass_does_not_hide_operational_failure(self):
        run=self.report_fixture();run['suites'][0]['id']='vision-scene-integration'
        folder=self.folder/'suites/vision-scene-integration/artifacts/work/vision-scene-integration';folder.mkdir(parents=True)
        write_json(folder/'report.json',dict(cases=[{}],operationalAcceptance='NOT_READY',continuousMetricRingFailures=['ring-left']))
        path=self.folder/'run.json';write_json(path,run)
        report=write_user_report(path).read_text('utf-8')
        self.assertIn('连续米制过环岛尚未通过验收',report)
        self.assertIn('1 个完整状态合成序列',report)

    def report_fixture(self,status='PASS'):
        suites=[{'id':'vision','status':status,'layer':'unit','reason':'fixture failure'}]
        return {'runId':'fixture','status':status,'startedAt':'2026-11-09T11:00:00+00:00','durationSeconds':1.25,
            'git':{'sha':'a'*40,'dirty':True},'environment':{'tools':{'compilerFamily':'gcc','mode':'optimized'}},'suites':suites}

    def test_failed_run_remains_failed_and_no_metrics_are_invented(self):
        run=self.report_fixture('FAIL');path=self.folder/'run.json';write_json(path,run)
        report=write_user_report(path).read_text('utf-8')
        self.assertIn('结果：失败',report);self.assertIn('fixture failure',report)
        self.assertIn('2026-11-09 19:00:00',report)
        self.assertNotIn('图像均值',report);self.assertNotIn('已修复',report)
        import xml.etree.ElementTree as ET
        ET.parse(self.folder/'report-images/results.svg')

    def test_skip_is_visible_and_all_pass_does_not_claim_vehicle_acceptance(self):
        run=self.report_fixture();run['suites'].append({'id':'unmeasured','status':'SKIPPED','layer':'performance','reason':'no labels'})
        path=self.folder/'run.json';write_json(path,run);report=write_user_report(path).read_text('utf-8')
        self.assertIn('跳过',report);self.assertIn('no labels',report)
        self.assertIn('未执行的测试、板端时序和实车行驶不能视为通过',report)

    def test_native_images_only_come_from_current_run_and_missing_reference_is_not_zero(self):
        run=self.report_fixture();run['suites'][0]['id']='performance-vision-images'
        folder=self.folder/'suites/performance-vision-images/artifacts/work/vision-image-performance';folder.mkdir(parents=True)
        data={'framesVerified':1,'qualityRowsPerReplay':28,'routeRecognitionEnabled':False,
              'timing':{'vision_ms':{'mean':.2,'p95':.3}},'audit':{'near':{'accepted':1},'full':{'accepted':0},
              'visuallyReviewedClearSceneCases':[{'index':0,'row54Flags':4,'rawValid':1}]}}
        write_json(folder/'report.json',data)
        (folder/'original.gray').write_bytes(bytes([127])*22560);(folder/'masks.gray').write_bytes(bytes([255])*22560)
        path=self.folder/'run.json';write_json(path,run);report=write_user_report(path).read_text('utf-8')
        self.assertIn('未测得',report);self.assertIn('仍有需处理的案例',report)
        self.assertIn('vision-examples.svg',report);self.assertIn('不判断性能提高或退化',report)
        import xml.etree.ElementTree as ET
        tree=ET.parse(self.folder/'report-images/vision-examples.svg')
        self.assertEqual(len(tree.findall('.//{http://www.w3.org/2000/svg}image')),2)
        run['environment']['inputsAtFinish']={};write_json(path,run)
        changed=write_user_report(path).read_text('utf-8')
        self.assertIn('不能用于验收',changed);self.assertNotIn('图像均值',changed)


class ContractTests(Scratch):
    def test_scene_not_ready_blocks_release_even_when_host_tests_pass(self):
        suite={'id':'vision-scene-integration','layer':'integration','category':'functional',
               'evidenceClass':'host_simulated','required':True,'dependsOn':[],'entrypoint':{'type':'python'},
               'acceptanceChecks':[{'id':'continuous-metric-ring','name':'连续米制环岛场景',
                   'reportPath':'artifacts/work/vision-scene-integration/report.json',
                   'statusField':'operationalAcceptance','blockingCasesField':'continuousMetricRingFailures',
                   'scope':'Synthetic scene replay only; not physical acceptance.'}]}
        def scheduled(catalog,order,output,context,jobs):
            folder=output/'suites'/suite['id'];folder.mkdir(parents=True)
            write_json(folder/suite['acceptanceChecks'][0]['reportPath'],
                       {'status':'PASS','cases':[{}],'operationalAcceptance':'NOT_READY','continuousMetricRingFailures':['ring-left']})
            result=make_result(suite,folder);result.update(status='PASS',exitCode=0,reason='contracts passed')
            return [result]
        context=SimpleNamespace(tools={'compilerFamily':'msvc','mode':'optimized'})
        output=io.StringIO()
        with patch('run_tests.load_catalog',return_value={suite['id']:suite}), \
             patch('run_tests.select_suites',return_value=[suite['id']]), \
             patch('run_tests.Context',return_value=context), \
             patch('run_tests.schedule',side_effect=scheduled), \
             patch('run_tests.input_snapshot',return_value={}), \
             patch('run_tests.git_state',return_value={}), contextlib.redirect_stdout(output):
            code=plan_main(['--profile','Host'])
        path=next(line.removeprefix('Report: ') for line in output.getvalue().splitlines() if line.startswith('Report: '))
        run=read_json(Path(path).with_name('run.json'))
        self.assertEqual(code,0) # Software regression remains independently useful.
        self.assertTrue(run['softwarePassed'])
        self.assertFalse(run['releaseEligible'])
        self.assertEqual(run['acceptance']['status'],'NOT_READY')
        self.assertIn('场景验收：未就绪',Path(path).with_name('user-report.md').read_text('utf-8'))
        output=io.StringIO()
        with patch('run_tests.load_catalog',return_value={suite['id']:suite}), \
             patch('run_tests.select_suites',return_value=[suite['id']]), \
             patch('run_tests.Context',return_value=context), \
             patch('run_tests.schedule',side_effect=scheduled), \
             patch('run_tests.input_snapshot',return_value={}), \
             patch('run_tests.git_state',return_value={}), contextlib.redirect_stdout(output):
            code=plan_main(['--profile','Host','--require-acceptance'])
        path=next(line.removeprefix('Report: ') for line in output.getvalue().splitlines() if line.startswith('Report: '))
        strict=read_json(Path(path).with_name('run.json'))
        self.assertEqual(code,1)
        self.assertTrue(strict['softwarePassed'])
        self.assertFalse(strict['commandPassed'])
        self.assertTrue(strict['acceptanceRequired'])
        self.assertFalse(strict['releaseEligible'])
        self.assertEqual(strict['acceptance']['status'],'NOT_READY')
        contradictory=copy.deepcopy(strict);contradictory['releaseEligible']=True
        with self.assertRaises(ValueError):validate_schema(contradictory,'result.schema.json')
        # Reporting reuses the persisted decision; a later edit to the child
        # artifact cannot silently rewrite this run's acceptance to PASS.
        child=Path(path).parent/'suites'/suite['id']/suite['acceptanceChecks'][0]['reportPath']
        write_json(child,{'status':'PASS','operationalAcceptance':'PASS','continuousMetricRingFailures':[]})
        write_report(Path(path).with_name('run.json'))
        self.assertIn('场景验收：未就绪',Path(path).with_name('user-report.md').read_text('utf-8'))

    def test_catalog_and_profiles_have_explicit_closures(self):
        catalog=load_catalog()
        selected=select_suites(catalog,'Host',['AUTO-TRACK'])
        self.assertLess(selected.index('speed'),selected.index('auto-track'))
        self.assertEqual(selected[-1],'auto-track')
        self.assertNotIn('hardware-acceptance',select_suites(catalog,'PreFlash'))
        self.assertNotIn('keil-base',select_suites(catalog,'Host'))
        self.assertIn('performance-host',select_suites(catalog,'Host'))
        self.assertIn('recorder',select_suites(catalog,'Host'))
        self.assertIn('recorder',select_suites(catalog,'PreFlash'))
        self.assertEqual(catalog['recorder']['dependsOn'],['compiler-environment'])
        self.assertTrue(catalog['recorder']['required'])
        closure=select_suites(catalog,'Host',['recorder'])
        self.assertEqual(closure,['environment','compiler-environment','recorder'])

    def test_recorder_sources_are_in_acceptance_snapshot(self):
        snapshot=input_snapshot()
        for path in ('demo/recorder/recorder_core.c','demo/recorder/recorder_storage.c',
                     'demo/recorder/include/recorder_runtime.h','tests/recorder/test_recorder.py'):
            self.assertIn(path,snapshot)

    def bad_catalog(self, mutate):
        catalog=read_json(ROOT/'tests/suites/catalog.json')
        mutate(catalog)
        path=self.folder/'catalog.json'
        write_json(path,catalog)
        with self.assertRaises(ValueError): load_catalog(path)

    def test_duplicate_suite_rejected(self):
        self.bad_catalog(lambda c:c['suites'].append(c['suites'][0]))

    def test_unknown_dependency_rejected(self):
        self.bad_catalog(lambda c:c['suites'][0]['dependsOn'].append('absent'))

    def test_dependency_cycle_rejected(self):
        self.bad_catalog(lambda c:c['suites'][0]['dependsOn'].append('vision'))

    def test_schema_rejects_wrong_timeout_type(self):
        self.bad_catalog(lambda c:c['suites'][0].update(timeoutSeconds='300'))

    def test_path_traversal_and_absolute_paths_rejected(self):
        for name in ('../outside','C:/Windows/cmd.exe','/outside'):
            with self.assertRaises(ValueError): inside(ROOT,name,must_exist=False)
        self.bad_catalog(lambda c:c['suites'][1].update(workingDirectory='../outside'))

    def test_hardware_cannot_enter_an_automatic_profile(self):
        self.bad_catalog(lambda c:next(s for s in c['suites'] if s['id']=='hardware-acceptance')['profiles'].append('Host'))

    def test_duplicate_variant_rejected(self):
        def mutate(c):
            variants=next(s for s in c['suites'] if s['id']=='drive')['entrypoint']['variants']
            variants.append(variants[0])
        self.bad_catalog(mutate)

    def test_acceptance_paths_and_duplicate_ids_rejected(self):
        def traversal(c):
            next(s for s in c['suites'] if s['id']=='vision-scene-integration')['acceptanceChecks'][0]['reportPath']='../old/report.json'
        self.bad_catalog(traversal)
        def duplicate(c):
            source=next(s for s in c['suites'] if s['id']=='vision-scene-integration')
            c['suites'][0]['acceptanceChecks']=copy.deepcopy(source['acceptanceChecks'])
        self.bad_catalog(duplicate)

    def test_json_duplicate_keys_and_nonfinite_values_rejected(self):
        path=self.folder/'bad.json'
        for text in ('{"x":1,"x":2}','{"x":NaN}'):
            path.write_text(text,encoding='utf-8')
            with self.assertRaises(ValueError): read_json(path)

    def test_optional_skip_and_required_blocking_are_distinct(self):
        results=[{'required':True,'status':'PASS'},{'required':False,'status':'SKIPPED'}]
        self.assertEqual(aggregate_status(results),'PASS')
        results[0]['status']='BLOCKED'
        self.assertEqual(aggregate_status(results),'BLOCKED')
        results[0]['status']='SKIPPED'
        self.assertNotEqual(aggregate_status(results),'PASS')

    def test_unknown_suite_never_silently_runs_host(self):
        with self.assertRaises(ValueError): select_suites({},'Host',['missing'])

    def test_explicit_manual_selection_never_reports_software_pass(self):
        output=io.StringIO()
        with contextlib.redirect_stdout(output):
            code=plan_main(['--suite','hardware-acceptance'])
        report=next(line.removeprefix('Report: ') for line in output.getvalue().splitlines() if line.startswith('Report: '))
        run=read_json(Path(report).with_name('run.json'))
        self.assertEqual(code,1)
        self.assertEqual(run['status'],'MANUAL_PENDING')
        self.assertFalse(run['softwarePassed'])
        self.assertFalse(run['releaseEligible'])


class AcceptanceTests(Scratch):
    def setUp(self):
        super().setUp()
        self.spec={'id':'ring','name':'环岛','scope':'synthetic only','reportPath':'artifacts/report.json',
                   'statusField':'operationalAcceptance','blockingCasesField':'continuousMetricRingFailures'}
        self.catalog={'scene':{'id':'scene','acceptanceChecks':[self.spec]}}
        self.result={'id':'scene','status':'PASS'}
        self.source=self.folder/'suites/scene/artifacts/report.json'

    def collect(self):return collect_acceptance(self.catalog,[self.result],self.folder)

    def test_declared_pass_is_scope_limited_and_allows_only_eligible_profile(self):
        write_json(self.source,{'status':'PASS','operationalAcceptance':'PASS','continuousMetricRingFailures':[]})
        a=self.collect();self.assertEqual(a['status'],'PASS')
        self.assertEqual(len(a['checks'][0]['reportSha256']),64)
        for compiler,profile,expected in [('msvc','Host',True),('msvc','PreFlash',True),('gcc','Host',False),('msvc','Suite',False),('msvc','Diagnostic',False)]:
            with self.subTest(compiler=compiler,profile=profile):
                d=decision(True,a,compiler,profile,True)
                self.assertTrue(d['commandPassed']);self.assertEqual(d['releaseEligible'],expected)
        self.assertFalse(decision(False,a,'msvc','Host')['releaseEligible'])

    def test_not_selected_skipped_or_missing_declarations_cannot_inherit_pass(self):
        write_json(self.source,{'status':'PASS','operationalAcceptance':'PASS','continuousMetricRingFailures':[]})
        for results,catalog in [([],self.catalog),([{'id':'scene','status':'SKIPPED'}],self.catalog),([self.result],{})]:
            with self.subTest(results=results,catalog=catalog):
                a=collect_acceptance(catalog,results,self.folder)
                self.assertEqual(a['status'],'NOT_EVALUATED')
                self.assertFalse(decision(True,a,'msvc','Host',True)['commandPassed'])
                self.assertFalse(decision(True,a,'msvc','Host')['releaseEligible'])

    def test_missing_invalid_unknown_and_contradictory_evidence_is_error(self):
        bad=[None,'{broken',{'status':'PASS','operationalAcceptance':'UNKNOWN','continuousMetricRingFailures':[]},
             {'status':'FAIL','operationalAcceptance':'PASS','continuousMetricRingFailures':[]},
             {'status':'PASS','operationalAcceptance':'PASS','continuousMetricRingFailures':['ring-left']},
             {'status':'PASS','operationalAcceptance':'NOT_READY','continuousMetricRingFailures':[]},
             {'status':'PASS','operationalAcceptance':'NOT_READY','continuousMetricRingFailures':'ring-left'}]
        for data in bad:
            with self.subTest(data=data):
                if isinstance(data,dict):write_json(self.source,data)
                elif isinstance(data,str):
                    self.source.parent.mkdir(parents=True,exist_ok=True)
                    self.source.write_text(data,encoding='utf-8')
                a=self.collect();self.assertEqual(a['status'],'ERROR')
                self.assertFalse(decision(True,a,'msvc','Host')['commandPassed'])
                self.assertFalse(decision(True,a,'msvc','Host')['releaseEligible'])

    def test_input_changes_and_unexecuted_sibling_prevent_acceptance_pass(self):
        write_json(self.source,{'status':'PASS','operationalAcceptance':'PASS','continuousMetricRingFailures':[]})
        a=collect_acceptance(self.catalog,[self.result],self.folder,inputs_unchanged=False)
        self.assertEqual(a['status'],'ERROR')
        sibling={'id':'other','acceptanceChecks':[dict(self.spec,id='other-check')]}
        a=collect_acceptance(dict(self.catalog,other=sibling),[self.result],self.folder)
        self.assertEqual(a['status'],'NOT_EVALUATED')
        self.assertEqual(a['checks'][0]['status'],'PASS')


class ProcessTests(Scratch):
    def run_command(self, code, timeout=5):
        with (self.folder/'out.log').open('wb') as out, (self.folder/'err.log').open('wb') as err:
            return run_process([sys.executable,'-c',code],self.folder,os.environ.copy(),timeout,out,err)

    def test_timeout_kills_parent_and_grandchild_and_stops_writes(self):
        heartbeat=self.folder/'heartbeat'
        child="import pathlib,time; p=pathlib.Path("+repr(str(heartbeat))+");\nwhile True: p.write_text(str(time.time())); time.sleep(.03)"
        code='import subprocess,sys,time; subprocess.Popen([sys.executable,"-c",'+repr(child)+']); time.sleep(30)'
        result=self.run_command(code,.7)
        self.assertTrue(result['timedOut'])
        self.assertTrue(result['cleanupVerified'])
        self.assertTrue(heartbeat.is_file())
        before=heartbeat.read_text()
        time.sleep(.12)
        self.assertEqual(heartbeat.read_text(),before)

    def test_success_also_cleans_unjoined_descendants(self):
        result=self.run_command('import subprocess,sys; subprocess.Popen([sys.executable,"-c","import time; time.sleep(30)"])')
        self.assertEqual(result['exitCode'],0)
        self.assertTrue(result['cleanupVerified'])

    def test_nonzero_exit_is_preserved(self):
        self.assertEqual(self.run_command('raise SystemExit(7)')['exitCode'],7)

    def test_resource_lock_timeout_and_release(self):
        with resource_locks(self.folder,['same'],1):
            with self.assertRaises(ProcessTimeout):
                with resource_locks(self.folder,['same'],.1): pass
        with resource_locks(self.folder,['same'],.1): pass

    def execution(self, source):
        source_path=self.folder/'fixture.py'
        source_path.write_text(source,encoding='utf-8')
        suite={'id':'fixture','layer':'unit','category':'functional','evidenceClass':'host','required':True,
               'dependsOn':[],'timeoutSeconds':5,'resources':[],'environment':{},'artifacts':['artifacts/**'],
               'entrypoint':{'type':'python','path':source_path.relative_to(ROOT).as_posix()}}
        context=Context(SimpleNamespace(compiler='gcc',mode='optimized',uv4=None,arm_bin=None,vcvars=None))
        return suite,context

    def test_failures_are_isolated_and_dependencies_block(self):
        suite,context=self.execution('from pathlib import Path; Path("private.txt").write_text("evidence"); raise SystemExit(3)')
        first=execute_suite(suite,self.folder/'run',context,[])
        self.assertEqual(first['status'],'FAIL')
        self.assertEqual(first['exitCode'],1)
        self.assertEqual(first['details']['lastRawExitCode'],3)
        second=dict(suite,id='dependent',dependsOn=['fixture'])
        blocked=execute_suite(second,self.folder/'run',context,[first])
        self.assertEqual(blocked['status'],'BLOCKED')
        self.assertEqual(blocked['processes'],[])
        self.assertTrue((self.folder/'run/suites/fixture/artifacts/work/private.txt').is_file())
        self.assertFalse((ROOT/'private.txt').exists())
        self.assertIn('artifacts/work/private.txt',first['artifacts'])
        validate_schema(first,'result.schema.json')

    def test_suite_timeout_is_distinct_and_normalized(self):
        suite,context=self.execution('import time; time.sleep(30)')
        suite['timeoutSeconds']=1
        result=execute_suite(suite,self.folder/'run',context,[])
        self.assertEqual(result['status'],'TIMEOUT')
        self.assertEqual(result['exitCode'],124)
        self.assertTrue(result['processes'][0]['cleanupVerified'])

    def test_python_compiler_bridge_preserves_selected_environment(self):
        suite,context=self.execution('pass')
        suite['dependsOn']=['compiler-environment'];suite['environment']={'FIXTURE_VALUE':'kept'}
        context.options.compiler='msvc';context.options.mode='asan'
        context.compiler='C:/fixture/cl.exe';context.compiler_env={'Path':'compiler-bin','LIB':'fixture-lib'}
        work=self.folder/'bridge';work.mkdir();result=make_result(suite,work)
        with (work/'stdout.log').open('wb') as out,(work/'stderr.log').open('wb') as err:
            execution=Execution(suite,work,context,result,out,err)
            env=execution.script_environment()
            self.assertEqual(env['Path'],'compiler-bin');self.assertEqual(env['LIB'],'fixture-lib')
            self.assertEqual(env['SMARTCAR_TEST_COMPILER'],'msvc')
            self.assertEqual(env['SMARTCAR_TEST_COMPILER_PATH'],context.compiler)
            self.assertEqual(env['SMARTCAR_TEST_MODE'],'asan');self.assertEqual(env['FIXTURE_VALUE'],'kept')
            self.assertEqual(env['TMP'],str(execution.work))
            self.assertEqual(context.compiler_env,{'Path':'compiler-bin','LIB':'fixture-lib'})
            context.compiler=None
            with self.assertRaises(SuiteProblem) as raised:execution.script_environment()
            self.assertEqual(raised.exception.status,'BLOCKED')

    def test_python_without_compiler_dependency_keeps_existing_behavior(self):
        suite,context=self.execution('pass');work=self.folder/'plain';work.mkdir()
        with (work/'stdout.log').open('wb') as out,(work/'stderr.log').open('wb') as err:
            execution=Execution(suite,work,context,make_result(suite,work),out,err)
            self.assertIs(execution.script_environment(),execution.env)
            self.assertNotIn('compilerFamily',execution.result['details'])

    def test_unrelated_compile_error_is_not_expected_rejection(self):
        suite,context=self.execution('pass')
        work=self.folder/'execution'
        work.mkdir()
        result=make_result(suite,work)
        with (work/'stdout.log').open('wb') as out,(work/'stderr.log').open('wb') as err:
            execution=Execution(suite,work,context,result,out,err)
            with self.assertRaises(SuiteProblem) as raised:
                execution.process([sys.executable,'-c','print("fatal error: missing header"); raise SystemExit(1)'],
                                  stage='compile',expect_failure='Route training requires explicit')
            self.assertEqual(raised.exception.status,'FAIL')
            execution.process([sys.executable,'-c','print("probe.c: error: #error Route training requires explicit"); raise SystemExit(1)'],
                              stage='compile',expect_failure='Route training requires explicit')
            with self.assertRaises(SuiteProblem):
                execution.process([sys.executable,'-c','raise SystemExit(1)'],stage='compile',
                                  expect_failure='Route training requires explicit')

    def test_parallel_scheduler_obeys_dag_and_continues_independent_failure(self):
        catalog={sid:{'id':sid,'dependsOn':deps} for sid,deps in
                 [('environment',[]),('bad',['environment']),('independent',['environment']),('downstream',['bad'])]}
        visited=[]
        def fake(suite,output,context,dependencies):
            self.assertTrue(all(d['id'] in visited for d in dependencies))
            visited.append(suite['id'])
            status='BLOCKED' if any(d['status']!='PASS' for d in dependencies) else 'FAIL' if suite['id']=='bad' else 'PASS'
            return {'id':suite['id'],'status':status}
        with patch('run_tests.execute_suite',side_effect=fake):
            results=schedule(catalog,list(catalog),self.folder,None,jobs=3)
        self.assertEqual({r['id']:r['status'] for r in results},
                         {'environment':'PASS','bad':'FAIL','independent':'PASS','downstream':'BLOCKED'})


class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.spec=read_json(ROOT/'tests/benchmarks/manifests/vision-host.json')
        self.spec['repetitions']=3
        self.spec['dataset']['groundTruthSha256']='a'*64
        quality,speed=self.spec['metrics']
        quality.update(gate='absolute',absoluteLimit=2,safetyCritical=True)
        speed.update(gate='regression',maxRegressionFraction=.05,noiseFraction=.02)

    def measurement(self,quality=1,speed=10):
        return {'gitSha':'a'*40,'dirty':False,'inputTreeSha256':'b'*64,'compilerId':'test-compiler','environmentId':'test-host','mode':'optimized',
                'dataset':copy.deepcopy(self.spec['dataset']),'parameters':{},'warmup':self.spec['warmup'],'repetitions':3,
                'samples':[{'sampleId':'synthetic','repetition':i,'valid':True,
                            'metrics':{'boundary-mae':quality,'processing-time':speed}} for i in range(3)]}

    def baseline(self):
        value=self.measurement()
        value['approval']={'reviewer':'synthetic-test','approvedAt':'2026-11-02T00:00:00Z','gitSha':value['gitSha']}
        return value

    def test_quality_failure_cannot_be_offset_by_speed_improvement(self):
        result=compare(self.spec,self.measurement(quality=3,speed=1),self.baseline())
        self.assertEqual(result['status'],'FAIL')
        self.assertEqual([m['decision'] for m in result['metrics']],['FAIL','PASS'])

    def test_noise_interval_is_inconclusive(self):
        self.assertEqual(compare(self.spec,self.measurement(speed=10.1),self.baseline())['status'],'INCONCLUSIVE')

    def test_regression_outside_tolerance_fails(self):
        self.assertEqual(compare(self.spec,self.measurement(speed=12),self.baseline())['status'],'FAIL')

    def test_comparison_requires_approved_baseline_and_same_conditions(self):
        self.assertEqual(compare(self.spec,self.measurement())['status'],'INCONCLUSIVE')
        for key,value in [('environmentId','different-host'),('compilerId','other-compiler'),('mode','asan')]:
            baseline=self.baseline();baseline[key]=value
            with self.assertRaises(ValueError): compare(self.spec,self.measurement(),baseline)
        with self.assertRaises(ValueError): compare(self.spec,self.measurement(),self.measurement())

    def test_invalid_missing_duplicate_and_nonfinite_samples_are_rejected(self):
        for mutate in [lambda m:m['samples'].clear(),lambda m:m['samples'].append(m['samples'][0]),
                       lambda m:m['samples'][0]['metrics'].update({'boundary-mae':float('nan')}),
                       lambda m:m.update(dataset={})]:
            measurement=self.measurement();mutate(measurement)
            with self.assertRaises(ValueError): summarize(self.spec,measurement)

    def test_unlabelled_quality_cannot_become_a_gate(self):
        self.spec['dataset']['groundTruthSha256']=None
        with self.assertRaises(ValueError): validate_benchmark(self.spec)

    def test_zero_baseline_and_higher_is_better(self):
        metric=self.spec['metrics'][1]
        metric.update(direction='higher',noiseFraction=0)
        self.assertEqual(compare(self.spec,self.measurement(speed=12),self.baseline())['metrics'][1]['decision'],'PASS')
        self.assertEqual(compare(self.spec,self.measurement(speed=-1),dict(self.baseline(),samples=self.measurement(speed=0)['samples']))['status'],'FAIL')

    def test_dirty_baseline_and_changed_sample_set_are_rejected(self):
        baseline=self.baseline();baseline['dirty']=True
        with self.assertRaises(ValueError): compare(self.spec,self.measurement(),baseline)
        measurement=self.measurement();measurement['samples'][0]['sampleId']='other-sample'
        with self.assertRaises(ValueError): compare(self.spec,measurement,self.baseline())

    def test_quantiles_use_documented_interpolation(self):
        self.assertAlmostEqual(statistic([1,2,3,4,5],'p95'),4.8)
        self.assertEqual(statistic([1,2,3,4],'median'),2.5)


if __name__=='__main__':
    unittest.main()
