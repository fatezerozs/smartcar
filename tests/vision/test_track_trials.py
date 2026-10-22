import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / 'tools/evaluate_track_trials.py'
spec = importlib.util.spec_from_file_location('track_trials', SCRIPT)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class TrackTrialTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        # Synthetic test evidence only. Not a real drive or a video validation.
        self.evidence = self.root / 'synthetic-review.txt'
        self.evidence.write_bytes(b'synthetic fixture: no real vehicle')
        self.course = {'id': 'SYNTHETIC_TEST_ONLY', 'start_rule': 'reviewed start',
                       'finish_rule': 'reviewed full completion', 'direction': 'same direction',
                       'conditions': 'same synthetic setup', 'include_stops': False,
                       'required_passages': {'section': 1, 'circle': 1}}
        self.trial = {
            'id': 'test-1', 'course_fingerprint': module.course_fingerprint(self.course),
            'firmware': 'SYNTHETIC', 'firmware_sha256': 'a' * 64,
            'start_ms': 1000, 'finish_ms': 52000, 'timing_uncertainty_ms': 80,
            'evidence': {'file': self.evidence.name,
                         'sha256': hashlib.sha256(self.evidence.read_bytes()).hexdigest()},
            'review': {'reviewer': 'unit test fixture', 'continuous_recording': True,
                       'on_track': True, 'route_complete': True, 'stops_reviewed': True,
                       'stops': [], 'passages': [
                           {'name': 'section', 'from_ms': 2000, 'to_ms': 22000},
                           {'name': 'circle', 'from_ms': 23000, 'to_ms': 51000}]}}

    def result(self, trial=None, course=None):
        return module.evaluate(course or self.course, [trial or self.trial], self.root)

    def test_clean_time_and_no_automatic_completion_claim(self):
        report = self.result()
        row = report['results'][0]
        self.assertEqual(row['status'], 'eligible_by_operator_review')
        self.assertEqual((row['elapsed_ms'], row['stopped_ms'], row['moving_ms']), (51000, 0, 51000))
        self.assertEqual(report['fastest_reported_trial'], 'test-1')
        self.assertFalse(report['automatic_course_completion_verified'])

    def test_stop_excluded_but_remains_fault_and_not_promoted(self):
        self.trial['review']['stops'] = [{'from_ms': 5000, 'to_ms': 10000,
                                         'reason': 'vision_loss'}]
        report = self.result()
        row = report['results'][0]
        self.assertEqual(row['score_ms'], 46000)
        self.assertEqual(row['stopped_ms'], 5000)
        self.assertEqual(row['fault_stop_count'], 1)
        self.assertEqual(row['status'], 'timed_with_faults')
        self.assertEqual(report['ranked_eligible_trials'], [])
        self.assertEqual(report['builds'][0]['fault_stop_count'], 1)

    def test_unreviewed_stops_are_not_assumed_zero(self):
        del self.trial['review']['stops_reviewed']
        report = self.result()
        self.assertEqual(report['results'][0]['status'], 'pending')
        self.assertNotIn('moving_ms', report['results'][0])

    def test_off_track_or_incomplete_is_invalid_even_if_quicker(self):
        for key in ('on_track', 'route_complete', 'continuous_recording'):
            trial = copy.deepcopy(self.trial)
            trial['review'][key] = False
            report = self.result(trial)
            self.assertEqual(report['results'][0]['status'], 'invalid')
            self.assertIsNone(report['fastest_reported_trial'])

    def test_missing_review_and_coverage_are_pending(self):
        for key in ('on_track', 'route_complete', 'continuous_recording'):
            trial = copy.deepcopy(self.trial)
            trial['review'].pop(key)
            self.assertEqual(self.result(trial)['results'][0]['status'], 'pending')
        self.trial['review']['passages'].pop()
        self.assertEqual(self.result()['results'][0]['status'], 'pending')

    def test_duplicate_or_outside_coverage_cannot_count_as_another_orbit(self):
        self.course['required_passages']['circle'] = 2
        self.trial['course_fingerprint'] = module.course_fingerprint(self.course)
        self.trial['review']['passages'].append(copy.deepcopy(self.trial['review']['passages'][-1]))
        self.assertEqual(self.result()['results'][0]['status'], 'pending')
        self.trial['review']['passages'][-1] = {'name': 'circle', 'from_ms': 53000, 'to_ms': 60000}
        self.assertEqual(self.result()['results'][0]['status'], 'pending')

    def test_overlapping_stops_never_double_subtract(self):
        self.trial['review']['stops'] = [
            {'from_ms': 5000, 'to_ms': 10000, 'reason': 'vision'},
            {'from_ms': 8000, 'to_ms': 12000, 'reason': 'duplicate annotation'}]
        row = self.result()['results'][0]
        self.assertEqual(row['stopped_ms'], 7000)
        self.assertEqual(row['status'], 'pending')

    def test_course_change_or_missing_evidence_prevents_ranking(self):
        self.course['direction'] = 'opposite direction'
        self.assertIsNone(self.result()['fastest_reported_trial'])
        self.trial['course_fingerprint'] = module.course_fingerprint(self.course)
        self.evidence.write_bytes(b'changed evidence')
        self.assertIsNone(self.result()['fastest_reported_trial'])
        self.evidence.unlink()
        self.assertIsNone(self.result()['fastest_reported_trial'])

    def test_no_trace_only_or_software_run_span_acceptance(self):
        report = self.result({'id': 'trace-only', 'TRACE_END': True, 'duration_ms': 10})
        self.assertIsNone(report['fastest_reported_trial'])

    def test_timing_types_and_duplicate_ids(self):
        for value in (True, -1, None, '1000', 52000):
            trial = copy.deepcopy(self.trial)
            trial['start_ms'] = value
            self.assertIsNone(self.result(trial)['fastest_reported_trial'])
        with self.assertRaises(ValueError):
            module.evaluate(self.course, [self.trial, self.trial], self.root)

    def test_uncertainty_does_not_claim_a_small_gain(self):
        other = copy.deepcopy(self.trial)
        other['id'] = 'test-2'
        other['finish_ms'] -= 20
        report = module.evaluate(self.course, [self.trial, other], self.root)
        self.assertEqual(report['fastest_reported_trial'], 'test-2')
        self.assertEqual(report['results'][1]['definitely_faster_than'], [])
        other['finish_ms'] -= 1000
        other['review']['passages'][-1]['to_ms'] = other['finish_ms'] - 500
        report = module.evaluate(self.course, [self.trial, other], self.root)
        self.assertEqual(report['results'][1]['definitely_faster_than'], ['test-1'])

    def test_unconfirmed_template_is_rejected(self):
        with self.assertRaises(ValueError):
            module.evaluate(module.template()['course'], [], self.root)

    def test_cli_cannot_overwrite_source_or_existing_report(self):
        manifest = self.root / 'trials.json'
        manifest.write_text(json.dumps({'course': self.course, 'trials': [self.trial]}), encoding='utf-8')
        for path in (manifest, self.evidence):
            before = path.read_bytes()
            run = subprocess.run([sys.executable, str(SCRIPT), str(manifest), '--output', str(path)],
                                 capture_output=True)
            self.assertNotEqual(run.returncode, 0)
            self.assertEqual(path.read_bytes(), before)
        output = self.root / 'report.json'
        first = subprocess.run([sys.executable, str(SCRIPT), str(manifest), '--output', str(output)],
                               capture_output=True)
        self.assertEqual(first.returncode, 0, first.stderr)
        before = output.read_bytes()
        second = subprocess.run([sys.executable, str(SCRIPT), str(manifest), '--output', str(output)],
                                capture_output=True)
        self.assertNotEqual(second.returncode, 0)
        self.assertEqual(output.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
