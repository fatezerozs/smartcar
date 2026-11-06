"""Host-only independent Python oracle, import/episode tests and C replay."""
import csv
import hashlib
import json
import math
import os
import copy
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from speed_planning.sessions import (load_session, episodes, budget_report, safety_and_tracking, SCHEMA)
from speed_planning.vehicle_model import (load_model, nominal_circumference, turn_geometry,
    legacy_servo_command_us, counts_to_speed, angle_from_pulse, report, Uncalibrated)
from speed_planning.replay import run_replay
from speed_planning.reference import load_reference,derived,write_c_header
from speed_planning.data_inputs import (audit,load_encoder,verify_manifest,inspect_trace,DELIVERY,ENCODER_PROFILE)


def sample(n, near=True, far=True, distance=None):
    return dict(frameSeq=n,captureMs=n*20,nearValid=near,farValid=far,
                odometerM=n*.01 if distance is None else distance,speedMeasuredMps=.5,
                speedReferenceMps=.5,leftTargetTicksS=500 if near else 0,
                rightTargetTicksS=500 if near else 0,leftPwmPermille=100 if near else 0,
                rightPwmPermille=100 if near else 0,
                mode='NORMAL_TRACK' if far else 'DEGRADED_FAR_LOSS',outputGeneration=1)


def write_session(path, ident, rows, **changes):
    header=dict(schemaVersion=SCHEMA,sessionId=ident,datasetVersion='SIMULATION-1',units='SI',
                provenance='simulation',imageSha256=hashlib.sha256(ident.encode()).hexdigest())
    header.update(changes)
    path.write_text('\n'.join(json.dumps(row) for row in [header,*rows])+'\n',encoding='utf-8')
    return load_session(path)


class Sessions(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.folder=Path(self.tmp.name)

    def test_complete_episode_counts_recovery_and_near_pause(self):
        rows=[sample(0),sample(1,far=False),sample(2,near=False,far=False),sample(3,far=False),sample(4),sample(5),sample(6)]
        data=write_session(self.folder/'a.jsonl','a',rows)
        e=episodes(data['samples'],3,100)
        self.assertEqual(len(e),1);self.assertEqual(e[0]['durationMs'],100)
        self.assertAlmostEqual(e[0]['distanceM'],.05);self.assertFalse(e[0]['censored'])
        self.assertFalse(safety_and_tracking(data)['nearInvalidNonzeroFrames'])

    def test_censor_gap_and_end(self):
        rows=[sample(0),sample(1,far=False),sample(8,far=False),sample(9,far=False)]
        data=write_session(self.folder/'a.jsonl','a',rows)
        e=episodes(data['samples'],3,100)
        self.assertEqual(len(e),2);self.assertTrue(all(x['censored'] for x in e))

    def test_no_frame_split_or_dataset_overlap(self):
        rows=[sample(0,far=False),sample(1),sample(2),sample(3)]
        a=write_session(self.folder/'a.jsonl','a',rows)
        b=write_session(self.folder/'b.jsonl','b',rows)
        kw=dict(recovery_frames=3,frame_period_ms=20,latency_ms=30,distance_margin_m=.01,max_gap_ms=100)
        report=budget_report([a],[b],**kw)
        self.assertEqual(report['method'],'temporary-maximum-plus-margin')
        self.assertEqual(report['timeBudgetMs'],100);self.assertEqual(report['coverage'],1)
        self.assertFalse(report['productionApproved']);self.assertIsNone(report['falseStopRate'])
        with self.assertRaisesRegex(ValueError,'overlap'):budget_report([a],[a],**kw)
        b['header']['imageSha256']=a['header']['imageSha256']
        with self.assertRaisesRegex(ValueError,'overlap'):budget_report([a],[b],**kw)

    def test_version_units_duplicates_nonfinite(self):
        p=self.folder/'x.jsonl'
        for changes in ({'schemaVersion':'future'},{'units':'pixels'},{'provenance':'trusted'}):
            with self.subTest(changes=changes),self.assertRaises(ValueError):write_session(p,'x',[sample(0)],**changes)
        write_session(p,'x',[sample(0)])
        p.write_text(p.read_text(encoding='utf-8').replace('"frameSeq": 0','"frameSeq": 0, "frameSeq": 1'),encoding='utf-8')
        with self.assertRaisesRegex(ValueError,'Duplicate'):load_session(p)
        row=sample(0);row['odometerM']=float('nan')
        with self.assertRaises(ValueError):write_session(p,'x',[row])

    def test_order_and_pwm_constraints(self):
        for rows in ([sample(1),sample(1)],[sample(1),sample(0)]):
            with self.assertRaises(ValueError):write_session(self.folder/'x.jsonl','x',rows)
        row=sample(1);row['leftPwmPermille']=1001
        with self.assertRaises(ValueError):write_session(self.folder/'x.jsonl','x',[row])

    def test_near_output_violation_reported(self):
        row=sample(1,near=False,far=False);row['leftPwmPermille']=50
        data=write_session(self.folder/'x.jsonl','x',[row])
        self.assertEqual(safety_and_tracking(data)['nearInvalidNonzeroFrames'],[1])

    def test_no_data_means_no_budget(self):
        with self.assertRaises(ValueError):budget_report([],[],recovery_frames=3,frame_period_ms=20,
            latency_ms=30,distance_margin_m=.01,max_gap_ms=100)
        a=write_session(self.folder/'a.jsonl','a',[sample(0),sample(1)])
        b=write_session(self.folder/'b.jsonl','b',[sample(0),sample(1)])
        with self.assertRaisesRegex(ValueError,'No complete'):budget_report([a],[b],recovery_frames=3,
            frame_period_ms=20,latency_ms=30,distance_margin_m=.01,max_gap_ms=100)

    def test_empirical_p99_is_explicit_policy(self):
        rows=[]
        for n in range(101):rows.extend([sample(2*n,far=False),sample(2*n+1)])
        a=write_session(self.folder/'a.jsonl','a',rows);b=write_session(self.folder/'b.jsonl','b',rows)
        r=budget_report([a],[b],recovery_frames=1,frame_period_ms=20,latency_ms=30,distance_margin_m=.01,max_gap_ms=100)
        self.assertEqual(r['sampleCount'],101);self.assertIn('P99',r['method']);self.assertFalse(r['productionApproved'])


class VehicleModel(unittest.TestCase):
    def setUp(self):
        self.model=load_model()

    def test_dimensions_keep_provenance(self):
        self.assertEqual(self.model['geometry']['wheelbaseM']['value'],.205)
        self.assertEqual(self.model['geometry']['rearTrackM']['value'],.152)
        self.assertIn('estimate',self.model['geometry']['wheelbaseM']['status'])
        self.assertIn('nominal',self.model['geometry']['tireDiameterM']['status'])
        self.assertAlmostEqual(nominal_circumference(self.model),.20106192982974677)
        self.assertIsNone(self.model['measuredRollingCircumferenceLeftM'])

    def test_bicycle_and_rear_axle_mirror(self):
        right=turn_geometry(self.model,1,1);left=turn_geometry(self.model,1,-1)
        self.assertAlmostEqual(right['rearLeftMps'],1.076)
        self.assertAlmostEqual(right['rearRightMps'],.924)
        self.assertEqual(right['rearLeftMps'],left['rearRightMps'])
        self.assertAlmostEqual(math.tan(right['requiredEffectiveSteeringRad']),.205)
        self.assertGreater(right['requiredFrontRightRad'],right['requiredFrontLeftRad'])
        self.assertEqual(turn_geometry(self.model,1,0)['rearLeftMps'],1)
        with self.assertRaises(ValueError):turn_geometry(self.model,1,20)
        with self.assertRaises(ValueError):turn_geometry(self.model,-1,0)

    def test_nominal_encoder_label_cannot_authorize_metres(self):
        self.assertEqual(self.model['encoder']['nominalResolutionLabel'],1024)
        for wheel in ('left','right'):
            with self.assertRaises(Uncalibrated):counts_to_speed(self.model,wheel,40,.01)

    def test_count_direction_and_sample_units_with_synthetic_calibration(self):
        model=copy.deepcopy(self.model)
        for wheel in ('left','right'):
            model['encoder'][wheel]['countsPerWheelRev']=1000 # synthetic
            model['measuredRollingCircumference'+wheel.capitalize()+'M']=.2 # synthetic
        self.assertAlmostEqual(counts_to_speed(model,'left',-40,.01),.8)
        self.assertAlmostEqual(counts_to_speed(model,'right',40,.01),.8)
        self.assertAlmostEqual(counts_to_speed(model,'left',-80,.02),.8)
        with self.assertRaises(ValueError):counts_to_speed(model,'left',1,0)

    def test_servo_command_is_monotonic_but_not_angle(self):
        model=self.model
        self.assertEqual(legacy_servo_command_us(model,0),1550)
        self.assertEqual(legacy_servo_command_us(model,1),1430)
        self.assertEqual(legacy_servo_command_us(model,-1),1670)
        self.assertAlmostEqual(legacy_servo_command_us(model,.5),1505)
        pulses=[legacy_servo_command_us(model,n/100) for n in range(-100,101)]
        self.assertTrue(all(a>b for a,b in zip(pulses,pulses[1:])))
        with self.assertRaises(Uncalibrated):angle_from_pulse(model,1550)

    def test_servo_profile_matches_current_source(self):
        text=(ROOT/'demo/code/midterm_route_profile.h').read_text(encoding='utf-8-sig')
        for direction in ('RIGHT','CENTER','LEFT'):
            value=int(re.search(r'#define MT_ROUTE_SERVO_'+direction+r'_US (\d+)U',text).group(1))
            self.assertEqual(value,self.model['servo']['pulse'+direction.title()+'Us'])

    def test_readiness_preserves_missing_calibration(self):
        result=report(self.model)
        self.assertFalse(result['productionApproved'])
        self.assertFalse(result['servoAngleMapAvailable'])
        self.assertFalse(result['encoderConversion']['left']['available'])
        self.assertIn('brakingDecelerationMps2',result['dynamicCalibrationMissing'])
        self.assertIsNone(result['exampleRightTurn']['servoPulseFromAngleUs'])

    def test_preserved_image_hashes(self):
        folder=ROOT/'docs/reference/c1-model-sources'
        manifest=json.loads((folder/'sources.json').read_text(encoding='utf-8'))
        self.assertEqual(len(manifest['items']),3)
        for item in manifest['items']:
            self.assertEqual(hashlib.sha256((folder/item['file']).read_bytes()).hexdigest(),item['sha256'])


class EngineeringReferences(unittest.TestCase):
    def test_explicit_assumptions_and_braking_reserve(self):
        reference=load_reference();model=load_model();p=reference['parameters'];d=derived(reference,model)
        self.assertFalse(reference['productionApproved'])
        self.assertNotIn('encoderWheelCprHypothesis',p)
        encoder=load_encoder()
        self.assertAlmostEqual(d['referenceTicksPerMetreLeft']*nominal_circumference(model),encoder['left']['counts_per_wheel_revolution'])
        self.assertAlmostEqual(d['referenceTicksPerMetreRight']*nominal_circumference(model),encoder['right']['counts_per_wheel_revolution'])
        self.assertNotEqual(d['referenceTicksPerMetreLeft'],d['referenceTicksPerMetreRight'])
        self.assertFalse(d['measuredMetricScale'])
        self.assertLess(p['planningDecelerationMps2']['value'],p['brakingDecelerationMps2']['value'])
        self.assertGreater(d['nearBrakingCapMps'],p['maximumSpeedMps']['value'])
        v=d['nearBrakingCapMps'];a=p['planningDecelerationMps2']['value'];t=p['latencyS']['value']
        self.assertAlmostEqual(v*t+v*v/(2*a),p['nearPreviewM']['value']-p['distanceMarginM']['value'])
        self.assertIsNone(model['encoder']['left']['countsPerWheelRev'])

    def test_generated_config_stays_host_and_rejects_bad_references(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);reference=load_reference();write_c_header(folder/'reference.h',reference,load_model())
            text=(folder/'reference.h').read_text(encoding='utf-8')
            self.assertIn('NOT production configuration',text)
            for value in (True,float('nan'),-1):
                changed=copy.deepcopy(reference);changed['parameters']['maximumSpeedMps']['value']=value
                path=folder/'bad.json';path.write_text(json.dumps(changed),encoding='utf-8')
                with self.assertRaises(ValueError):load_reference(path)

    def test_frozen_v02_remains_explicitly_replayable(self):
        legacy=load_reference(ROOT/'profiles/c1-host-reference-v0.2.json')
        d=derived(legacy,load_model())
        self.assertEqual(d['encoderScaleSource']['kind'],'legacy-conditional-cpr-with-nominal-circumference')
        self.assertAlmostEqual(d['referenceTicksPerMetreLeft']*nominal_circumference(load_model()),1024)
        changed=copy.deepcopy(legacy);changed['encoderProfile']='profiles/encoder-production-20261106.json'
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'ambiguous.json';path.write_text(json.dumps(changed),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'exactly one'):load_reference(path)


class CurrentDataInputs(unittest.TestCase):
    def test_delivered_binary_ledger_and_production_source_agree(self):
        result=audit()
        self.assertEqual(result['verifiedNativeSamples'],4500)
        self.assertEqual(result['operatorTurnTrialsRecomputed'],7)
        self.assertEqual(len(result['windows']),9)
        self.assertTrue(result['readiness']['wheelCprAdopted'])
        for key in ('measuredMetricSpeed','motionNearFarBudgetDataset','productionPlannerIntegrationAccepted'):
            self.assertFalse(result['readiness'][key])
        self.assertFalse(result['b2Snapshot']['fullImageSessionReverifiedThisRun'])

    def test_corrupt_manifest_and_escape_are_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);(folder/'data').write_bytes(b'ok')
            manifest={'schema':'smartcar.local-handoff-manifest/1','files':[
                {'path':'data','bytes':2,'sha256':hashlib.sha256(b'ok').hexdigest()}]}
            def save(): (folder/'MANIFEST.json').write_text(json.dumps(manifest),encoding='utf-8')
            save();self.assertEqual(len(verify_manifest(folder)),1)
            (folder/'data').write_bytes(b'no')
            with self.assertRaisesRegex(ValueError,'hash/size'):verify_manifest(folder)
            manifest['files'][0]['path']='../outside';save()
            with self.assertRaisesRegex(ValueError,'escapes'):verify_manifest(folder)

    def test_crc_valid_sample_disagreement_is_rejected(self):
        source=DELIVERY/'evidence/encoder/222829502'
        raw=bytearray((source/'trace.b3t').read_bytes());header=(source/'header.txt').read_text(encoding='utf-8')
        offsets=[i for i in range(0,len(raw),64) if struct.unpack_from('<I',raw,i+4)[0]==2]
        # Change a cumulative count, then recompute transport CRC: semantic
        # checks must still reject a structurally valid, CRC-valid record.
        at=offsets[1]+16+4*4
        struct.pack_into('<i',raw,at,struct.unpack_from('<i',raw,at)[0]+1)
        header=re.sub(r'crc32=\w+',f'crc32={zlib.crc32(raw):08X}',header)
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);(folder/'trace.b3t').write_bytes(raw);(folder/'header.txt').write_text(header,encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'disagreement'):inspect_trace(folder)
            raw[0]^=1;(folder/'trace.b3t').write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'CRC'):inspect_trace(folder)

    def test_source_cpr_drift_is_rejected(self):
        data=load_encoder()
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);path=root/'encoder.json';path.write_text(json.dumps(data),encoding='utf-8')
            source=root/data['source_profile'];source.parent.mkdir(parents=True)
            original=(ROOT/data['source_profile']).read_text(encoding='utf-8')
            source.write_text(original,encoding='utf-8');load_encoder(path,root)
            source.write_text(original.replace('23358U','10240U'),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'CPR drift'):load_encoder(path,root)


def independent_spatial(case):
    k=[0 if case==0 else .2 if case==1 else 2 if case==2 else 2*math.sin(j*.6) if case==3 else
       (3 if j>=8 else 0) if case==4 else .2+.001*math.sin(j*2) for j in range(16)]
    speed=[min(3,math.sqrt(2/(abs(curve)+.0001)),
               math.sqrt(.04**2+4*max((15-j)*.25-.02,0))-.04) for j,curve in enumerate(k)]
    for j in range(14,-1,-1):speed[j]=min(speed[j],math.sqrt(speed[j+1]**2+1))
    speed[0]=min(2,speed[0])
    for j in range(1,16):
        initial=min(2,speed[0]) if j==1 else speed[j-1]
        speed[j]=min(speed[j],math.sqrt(initial**2+.5))
    for j in range(14,-1,-1):speed[j]=min(speed[j],math.sqrt(speed[j+1]**2+1))
    return speed


def compile_and_replay(out):
    compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('gcc')
    if not compiler:raise RuntimeError('Explicit existing compiler required; no installation')
    family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
    exe=out/'planning.exe'
    sources=[ROOT/'tests/planning/test_speed_planner.c',ROOT/'tools/speed_planning/speed_planner.c',ROOT/'tools/speed_planning/vision_adapter.c']
    includes=[ROOT/'tools/speed_planning',ROOT/'demo/code']
    if family=='msvc':
        flags=['/nologo','/utf-8','/std:c11','/W4','/WX']+(['/Od','/Zi','/fsanitize=address'] if mode=='asan' else ['/O2'])
        command=[compiler,*flags,*['/I'+str(p) for p in includes],*map(str,sources),'/Fe'+str(exe),'/Fo'+str(out)+os.sep,'/Fd'+str(out)+os.sep]
    else:
        flags=['-O0','-g','-fsanitize=address'] if mode=='asan' else ['-O2']
        command=[compiler,'-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function',*flags,
                 *['-I'+str(p) for p in includes],*map(str,sources),'-lm','-o',str(exe)]
    print(subprocess.list2cmdline(command),flush=True)
    subprocess.run(command,cwd=out,check=True,timeout=90)
    subprocess.run([exe],cwd=out,check=True,timeout=30)
    rows=list(csv.DictReader((out/'planning-replay.csv').open(encoding='utf-8')))
    for case in range(6):
        expected=independent_spatial(case);actual=[float(r['speed']) for r in rows if r['track']=='spatial' and int(r['case'])==case]
        if len(actual)!=16 or max(abs(a-b) for a,b in zip(actual,expected))>2e-5:
            raise AssertionError('C/Python spatial oracle mismatch: '+str(case))
    temporal=[r for r in rows if r['track']=='temporal']
    speeds=[float(r['speed']) for r in temporal];accel=[float(r['feature']) for r in temporal]
    jerk=[(b-a)/.02 for a,b in zip([0,*accel],accel)]
    metrics={'schemaVersion':'smartcar.host-planning/1','evidence':'synthetic-simulation','productionApproved':False,
             'spatialCases':6,'spatialSamples':96,'temporalSamples':len(temporal),'periodS':.02,
             'speedUnit':'m/s','accelerationUnit':'m/s^2','jerkUnit':'m/s^3',
             'minimumReference':min(speeds),'maximumReference':max(speeds),'maxAbsAccel':max(map(abs,accel)),
             'maxAbsJerk':max(map(abs,jerk)),'trackingRmse':None,'measuredBrakeDistanceM':None,
             'oracleMaxToleranceMps':2e-5,'csvSha256':hashlib.sha256((out/'planning-replay.csv').read_bytes()).hexdigest()}
    (out/'normal-performance.json').write_text(json.dumps(metrics,indent=2)+'\n',encoding='utf-8')
    (out/'vehicle-model.json').write_text(json.dumps(report(load_model()),indent=2)+'\n',encoding='utf-8')
    print('Independent Python spatial oracle: 6 cases / 96 points PASS; metrics: '+str(out/'normal-performance.json'),flush=True)


if __name__=='__main__':
    suite=unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(cls) for cls in (Sessions,VehicleModel,EngineeringReferences,CurrentDataInputs))
    outcome=unittest.TextTestRunner(verbosity=2).run(suite)
    if not outcome.wasSuccessful():sys.exit(1)
    # Test runner supplies an isolated private working directory. Direct use is
    # placed under .build to keep generated artifacts out of source directories.
    out=(ROOT/'.build/speed-planning/offline') if Path.cwd()==ROOT else Path.cwd()/'planning'
    out.mkdir(parents=True,exist_ok=True)
    compile_and_replay(out)
    (out/'data-input-audit.json').write_text(json.dumps(audit(),ensure_ascii=False,indent=2,allow_nan=False)+'\n',encoding='utf-8')
    run_replay(out/'composed')
