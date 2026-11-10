"""Fail-closed measurement/accuracy bookkeeping, independent of timing values."""
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from tools.testing.vision_image_benchmark import complete_grid,quality_samples,loss_episodes

class MeasurementContract(unittest.TestCase):
    def test_missing_and_duplicate_frames_cannot_improve_timing(self):
        rows=[{'repetition':str(r),'index':str(i),'vision_ms':'.2','metric_ms':'.001'} for r in range(2) for i in range(3)]
        complete_grid(rows,3,2)
        for bad in (rows[:-1],rows+[rows[0]],list(reversed(rows))):
            with self.assertRaises(ValueError):complete_grid(bad,3,2)
        bad=copy.deepcopy(rows);bad[0]['vision_ms']='nan'
        with self.assertRaises(ValueError):complete_grid(bad,3,2)

    def test_missing_boundary_stays_in_accuracy_denominator(self):
        labels={'height':2,'width':188,'frames':[{'index':0,'sha256':'pinned'}],
                'reviewRows':[0,1],'referenceCentresLeftRight':[[10,30],[11,31]],'tolerancePixels':3}
        rows=[{'flags':'3','left':'10','right':'31'},{'flags':'0','left':'255','right':'255'}]
        samples=quality_samples(labels,rows,{0:'pinned'})
        self.assertEqual(len(samples),2)
        self.assertTrue(all(s['valid'] for s in samples))
        self.assertEqual(samples[0]['metrics']['boundary-mae'],.5)
        self.assertEqual(samples[1]['metrics']['missing-pair'],1)
        self.assertEqual(samples[1]['metrics']['within-reference-band'],0)
        self.assertEqual(samples[1]['metrics']['boundary-max-error'],188)
        with self.assertRaises(ValueError):quality_samples(labels,rows,{0:'changed'})

    def test_loss_at_end_and_independent_full_recovery(self):
        rows=[{'near_accepted':n,'full_accepted':f} for n,f in [('1','0'),('0','1'),('0','0')]]
        self.assertEqual(loss_episodes(rows,'near_accepted'),[{'firstIndex':1,'lastIndex':2,'frames':2,'modelDurationMs':40}])
        self.assertEqual([e['firstIndex'] for e in loss_episodes(rows,'full_accepted')],[0,2])

if __name__=='__main__':unittest.main(verbosity=2)
