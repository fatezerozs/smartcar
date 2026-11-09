"""Measured-data transforms and the actual private shadow candidate, no device."""
import copy
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.prepare_phase3_shadow import VERSION,prepare
from tools.speed_planning.phase3_profile import PROFILE,build_profile,header,project,validate_profile
from tools.speed_planning.replay import compile_command
from tools.speed_planning.visual_trace import decode_frame


class ShadowPreparation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        parent=ROOT/'.build/phase3-shadow-tests';parent.mkdir(parents=True,exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(dir=parent);cls.addClassCleanup(cls.temp.cleanup)
        cls.folder=Path(cls.temp.name);cls.profile=json.loads(PROFILE.read_text('utf-8'))
        cls.manifest=prepare(cls.folder/'candidate');cls.code=Path(cls.manifest['output'])/'workspace/demo/code'

    def test_profile_matches_sources_and_retains_unknowns(self):
        self.assertEqual(self.profile,build_profile())
        self.assertFalse(self.profile['productionApproved']);self.assertFalse(self.profile['activeMetricControlAllowed'])
        self.assertTrue(all(v is None for v in self.profile['unresolved'].values()))
        self.assertIn('SP_SIMULATION',header(self.profile))
        bad=copy.deepcopy(self.profile);bad['runtimeConfig']['limits']['latency_s']=.004
        with self.assertRaises(ValueError):validate_profile(bad)
        bad=copy.deepcopy(self.profile);bad['runtimeConfig']['ticks_per_metre_left']=12358
        with self.assertRaises(ValueError):validate_profile(bad)

    def test_homography_uses_exact_preview_sampling_and_metres(self):
        source=json.loads((ROOT/'docs/changes/phase2-calibration-20261108/ground-homography-origin-corrected.json').read_text('utf-8'))
        original=source['homographyImageToRearGroundMm'];converted=self.profile['measured']['groundMapping']['runtimeHomographyMetres']
        for p in source['points']:
            u,v=p['measuredImageUvMean'];a=project(original,u,v);b=project(converted,u*2,v*2)
            for x,y in zip(a,b):self.assertAlmostEqual(x*.001,y,places=12)
        self.assertEqual(self.profile['runtimeConfig']['compensate'],0)

    def test_projects_share_one_image_and_only_download_opt_in_differs(self):
        a=ET.parse(self.manifest['project']);b=ET.parse(self.manifest['downloadProject'])
        self.assertEqual(a.findtext('.//UpdateFlashBeforeDebugging'),'0');self.assertEqual(b.findtext('.//UpdateFlashBeforeDebugging'),'1')
        b.find('.//UpdateFlashBeforeDebugging').text='0';self.assertEqual(ET.tostring(a.getroot()),ET.tostring(b.getroot()))
        defines=a.findtext('.//Cads/VariousControls/Define');self.assertIn('VISION_PLANNER_ACTIVE=0',defines)
        self.assertNotIn('VISION_PLANNER_PROFILE_REVIEWED=1',defines)
        self.assertTrue(all(n.text=='0' for n in a.iter() if n.tag.startswith('RunUserProg')))
        self.assertEqual((self.code/'vision_speed.c').read_bytes(),(ROOT/'demo/code/vision_speed.c').read_bytes())
        self.assertEqual((self.code/'vision_actuator.c').read_bytes(),(ROOT/'demo/code/vision_actuator.c').read_bytes())
        self.assertIn('build='+VERSION,(self.code/'vision_field.c').read_text('utf-8'))
        self.assertFalse(self.manifest['autoLoadOrArm']);self.assertFalse(self.manifest['flashed'])

    def test_native_candidate_keeps_legacy_pwm_and_retains_stop_only_trace(self):
        family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
        compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family=='msvc' else 'gcc')
        catalog=json.loads((ROOT/'tests/suites/catalog.json').read_text('utf-8'))
        suite=next(s for s in catalog['suites'] if s['id']=='planner-runtime')
        variant=next(v for v in suite['entrypoint']['variants'] if v['id']=='shadow')
        candidate=Path(self.manifest['output'])/'workspace'
        sources=[ROOT/'tests/planning/test_shadow_candidate.c']+[candidate/s for s in variant['sources'][1:]]+[self.code/'vision_planner_shadow_diag.c']
        exe=self.folder/'candidate-test.exe'
        cmd=compile_command(compiler,family,mode,sources,[ROOT/'tests/vision/stubs',self.code],exe)
        defines=variant['defines']+['VISION_HOST_SHADOW_DIAG=1']
        cmd[1:1]=[('/D' if family=='msvc' else '-D')+s for s in defines]
        built=subprocess.run(cmd,capture_output=True,text=True,timeout=90);self.assertEqual(built.returncode,0,built.stderr)
        result=subprocess.run([exe],capture_output=True,text=True,timeout=30)
        print(result.stdout,flush=True);self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_original_750_images_expose_candidate_geometry_coverage(self):
        folder=ROOT/'docs/changes/phase2-calibration-20261108/evidence/visual-continuous/session04'
        saved=json.loads((folder/'session.json').read_text('utf-8'));raw=self.folder/'original.gray'
        with raw.open('wb') as file:
            for i,metadata in enumerate(saved['frames']):
                blob=(folder/metadata['file']).read_bytes();decoded=decode_frame(blob,saved['frozen']['gen'],i)
                self.assertEqual(hashlib.sha256(blob).hexdigest(),metadata['sha256'])
                self.assertTrue(all(decoded[k]==metadata[k] for k in decoded));file.write(blob[96:])
        family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
        compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family=='msvc' else 'gcc')
        source_names=['demo/code/vision_midterm.c','demo/code/vision_metric_io.c','demo/code/vision_planner.c',
                      'demo/code/vision_planner_shadow.c','tools/speed_planning/vision_adapter.c','tools/speed_planning/speed_planner.c']
        sources=[ROOT/'tools/speed_planning/metric_input_audit.c']+[ROOT/n for n in source_names]
        exe=self.folder/'geometry.exe';destination=self.folder/'geometry.csv'
        cmd=compile_command(compiler,family,mode,sources,[ROOT/'demo/code',self.code],exe)
        flags=['VISION_MIDTERM_TRAINING=1','VISION_PLANNER_RUNTIME=1','VISION_ROUTE_SWITCH=1',
               'VISION_ENCODER_ENABLE=1','VISION_MIDTERM_ROUTE_TRIAL=1','VISION_MIDTERM_AUTONOMOUS=0']
        cmd[1:1]=[('/D' if family=='msvc' else '-D')+s for s in flags]
        built=subprocess.run(cmd,capture_output=True,text=True,timeout=60);self.assertEqual(built.returncode,0,built.stderr)
        result=subprocess.run([exe,raw,destination],capture_output=True,text=True,timeout=60)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);print(result.stdout,flush=True)
        with destination.open() as file:rows=list(csv.DictReader(file))
        self.assertEqual(len(rows),750)
        near=sum(int(r['near_accepted']) for r in rows);full=sum(int(r['full_accepted']) for r in rows)
        self.assertEqual((near,full),(470,376)) # Frozen scene/profile; not recognition ground truth.
        self.assertEqual([int(r['index']) for r in rows if r['arm_event']=='1'],[0])
        # Uniform 20ms timestamps and explicit stationary synthetic encoder:
        # accepted real geometry must survive the legal maximum sample period.
        self.assertEqual(rows[1]['planner_status'],'2') # VPS_READY at the exact 20ms boundary.
        self.assertTrue(any(int(r['target_right'])>0 and int(r['target_left'])>0 for r in rows))
        # Later real geometry outages are retained. A latched supervisor must
        # not silently rearm when subsequent images look usable again.
        first_fault=next(i for i,r in enumerate(rows) if r['planner_status']=='4')
        self.assertGreater(first_fault,1)
        self.assertTrue(all(r['planner_status']=='4' for r in rows[first_fault:]))


if __name__=='__main__':unittest.main(verbosity=2)
