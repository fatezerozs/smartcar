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
from run_tests import execute_suite, make_result, schedule, main as plan_main


class Scratch(unittest.TestCase):
    def setUp(self):
        parent=ROOT/'.build/framework-tests'
        parent.mkdir(parents=True,exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.folder=Path(self.temp.name)


class ContractTests(Scratch):
    def test_catalog_and_profiles_have_explicit_closures(self):
        catalog=load_catalog()
        selected=select_suites(catalog,'Host',['AUTO-TRACK'])
        self.assertLess(selected.index('speed'),selected.index('auto-track'))
        self.assertEqual(selected[-1],'auto-track')
        self.assertNotIn('hardware-acceptance',select_suites(catalog,'PreFlash'))
        self.assertNotIn('keil-base',select_suites(catalog,'Host'))
        self.assertIn('performance-host',select_suites(catalog,'Host'))

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
