"""Original full-resolution images through native C vision and metric adapter.

Local visually reviewed reference bands supplement coverage counts. Neither
valid flags nor accepted metric previews are recognition-accuracy ground truth.
"""
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

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools.speed_planning.phase3_profile import PROFILE,header
from tools.speed_planning.replay import compile_command
from tools.speed_planning.visual_trace import decode_frame


class OriginalImageRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        parent=ROOT/'.build/vision-image-tests';parent.mkdir(parents=True,exist_ok=True)
        cls.temp=tempfile.TemporaryDirectory(dir=parent);cls.addClassCleanup(cls.temp.cleanup)
        cls.folder=Path(cls.temp.name)
        cls.labels=json.loads((ROOT/'tests/vision/fixtures/phase3-real-boundaries-20261109.json').read_text('utf-8'))
        cls.session=ROOT/cls.labels['sourceSession'];saved=json.loads((cls.session/'session.json').read_text('utf-8'))
        raw=cls.folder/'images.gray';cls.hashes={}
        with raw.open('wb') as file:
            for i,metadata in enumerate(saved['frames']):
                blob=(cls.session/metadata['file']).read_bytes();digest=hashlib.sha256(blob).hexdigest()
                if digest!=metadata['sha256']:raise ValueError('Original image SHA mismatch')
                decoded=decode_frame(blob,saved['frozen']['gen'],i)
                if any(metadata[k]!=v for k,v in decoded.items()):raise ValueError('Original metadata mismatch')
                file.write(blob[96:]);cls.hashes[i]=digest
        (cls.folder/'phase3_shadow_profile.h').write_text(header(json.loads(PROFILE.read_text('utf-8'))),'utf-8')
        family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc');mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
        compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl' if family=='msvc' else 'gcc')
        names=['tools/speed_planning/vision_image_probe.c','demo/code/vision_midterm.c','demo/code/vision_metric_io.c',
               'demo/code/vision_planner.c','demo/code/vision_planner_shadow.c','tools/speed_planning/vision_adapter.c',
               'tools/speed_planning/speed_planner.c']
        exe=cls.folder/'probe.exe';command=compile_command(compiler,family,mode,[ROOT/n for n in names],
                                                      [ROOT/'demo/code',cls.folder],exe)
        flags=['VISION_MIDTERM_TRAINING=1','VISION_PLANNER_RUNTIME=1','VISION_ROUTE_SWITCH=1',
               'VISION_ENCODER_ENABLE=1','VISION_MIDTERM_ROUTE_TRIAL=1','VISION_MIDTERM_AUTONOMOUS=0']
        command[1:1]=[('/D' if family=='msvc' else '-D')+f for f in flags]
        built=subprocess.run(command,capture_output=True,text=True,timeout=60)
        if built.returncode:raise RuntimeError(built.stdout+built.stderr)
        result=subprocess.run([exe,raw,cls.folder/'rows.csv',cls.folder/'masks.gray'],capture_output=True,text=True,timeout=60)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
        print(result.stdout,flush=True)
        with (cls.folder/'rows.csv').open() as file:cls.rows=list(csv.DictReader(file))

    def test_all_original_frames_and_current_metric_coverage(self):
        self.assertEqual(len(self.rows),750*120)
        frames=self.rows[::120]
        self.assertEqual(sum(int(r['near_valid']) for r in frames),470)
        self.assertEqual(sum(int(r['full_valid']) for r in frames),376)
        print('Original 750 frames: near=470/full=376; coverage, not accuracy.',flush=True)

    def test_visually_reviewed_boundary_bands(self):
        pairs=0
        for frame in self.labels['frames']:
            self.assertEqual(self.hashes[frame['index']],frame['sha256'])
            for y,(left,right) in zip(self.labels['reviewRows'],self.labels['referenceCentresLeftRight']):
                r=self.rows[frame['index']*120+y];context=f"frame={frame['index']} row={y}"
                self.assertEqual(r['flags'],'3',context)
                self.assertLessEqual(abs(int(r['left'])-left),self.labels['tolerancePixels'],context+' left')
                self.assertLessEqual(abs(int(r['right'])-right),self.labels['tolerancePixels'],context+' right')
                pairs+=1
        print(f'Local reviewed bands: {pairs} paired rows; selected after experiments, no holdout claim.',flush=True)


if __name__=='__main__':unittest.main(verbosity=2)
