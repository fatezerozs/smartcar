"""Synthetic temporary intake tests, not operator measurements or physical data."""
import csv
import json
import math
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.speed_planning.calibration_inputs import GEOMETRY, HEADERS, analyze


class ManualIntake(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.session = {
            'schemaVersion': 'smartcar.phase2-manual/1', 'sessionId': 'synthetic-test',
            'status': 'COLLECTING', 'productionApproved': False,
            'operator': 'synthetic-fixture', 'vehicleId': 'synthetic-not-real', 'conditionId': 'c1',
            'instrument': {'id': 'tool1', 'name': 'synthetic-tool', 'distanceResolutionMm': 0.1,
                           'angleResolutionDeg': 0.1},
            'conditions': {'surface': 'synthetic', 'batteryAndPayload': 'synthetic',
                           'tireState': 'synthetic', 'otherTestReleasedVehicle': True,
                           'actuatorPowerIsolated': True},
        }
        for kind in HEADERS:
            self.session[kind + 'Csv'] = kind + '.csv'
            self.write_rows(kind, [])
        self.path = self.folder / 'session.json'
        self.encoder = self.folder / 'encoder.json'
        self.encoder.write_text(json.dumps({
            'schema': 'smartcar.encoder-production/1', 'production_configuration_written': True,
            'left': {'counts_per_wheel_revolution': 2000},
            'right': {'counts_per_wheel_revolution': 2100},
        }), encoding='utf-8')

    def row(self, trial, **values):
        return dict(trial_id=trial, condition_id='c1', instrument_id='tool1',
                    measured_at='2026-11-08T20:00:00+08:00', accepted='true', notes='', **values)

    def write_rows(self, kind, rows):
        with (self.folder / (kind + '.csv')).open('w', encoding='utf-8', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=HEADERS[kind].split(','))
            writer.writeheader()
            writer.writerows(rows)

    def result(self):
        self.path.write_text(json.dumps(self.session), encoding='utf-8')
        return analyze(self.path, self.encoder)

    def test_empty_cannot_pass(self):
        result = self.result()
        self.assertEqual(result['status'], 'INCOMPLETE')
        self.assertEqual(result['rolling'], {})
        self.assertFalse(result['phase2Complete'])

    def test_known_conversion_and_direction_separation(self):
        self.write_rows('rolling', [
            self.row('lf', wheel='left', direction='forward', turns=5, distance_mm=1000),
            self.row('lr', wheel='left', direction='reverse', turns=5, distance_mm=950),
            self.row('rf', wheel='right', direction='forward', turns=5, distance_mm=1050),
        ])
        result = self.result()['rolling']
        self.assertEqual(result['left:forward']['circumferenceMm']['mean'], 200)
        self.assertEqual(result['left:forward']['countsPerMetre']['mean'], 10000)
        self.assertAlmostEqual(result['left:reverse']['countsPerMetre']['mean'], 2000 / .19)
        self.assertEqual(result['right:forward']['countsPerMetre']['mean'], 10000)
        self.assertNotIn('right:reverse', result)

    def test_three_repeats_are_not_physical_approval(self):
        geometry = []
        for quantity in GEOMETRY:
            for j in range(3):
                geometry.append(self.row(quantity + str(j), quantity=quantity,
                                value_mm=0 if 'offset' in quantity else 100))
        self.write_rows('geometry', geometry)
        rows = []
        for wheel in ('left', 'right'):
            for direction in ('forward', 'reverse'):
                for j, distance in enumerate((995, 1000, 1005)):
                    rows.append(self.row(wheel + direction + str(j), wheel=wheel,
                                direction=direction, turns=5, distance_mm=distance))
        self.write_rows('rolling', rows)
        result = self.result()
        self.assertEqual(result['status'], 'STATIC_COMPLETE_PENDING_REVIEW')
        self.assertAlmostEqual(result['rolling']['left:forward']['circumferenceMm']['sampleStandardDeviation'], 1)
        self.assertFalse(result['productionApproved'])
        self.assertFalse(result['phase2Complete'])

    def test_busy_vehicle_unknown_resolution_and_mixed_conditions_reject(self):
        self.write_rows('rolling', [self.row('r', wheel='left', direction='forward', turns=5, distance_mm=1000)])
        for field in ('otherTestReleasedVehicle', 'actuatorPowerIsolated'):
            self.session['conditions'][field] = False
            with self.assertRaises(ValueError): self.result()
            self.session['conditions'][field] = True
        self.session['instrument']['distanceResolutionMm'] = None
        with self.assertRaises(ValueError): self.result()
        self.session['instrument']['distanceResolutionMm'] = .1
        self.session['conditionId'] = 'different-load'
        with self.assertRaises(ValueError): self.result()

    def test_missing_values_nonfinite_fractional_turns_and_duplicates_reject(self):
        for turns, distance in ((5, ''), (2.5, 500), (5, 'nan'), (5, 0)):
            self.write_rows('rolling', [self.row('r', wheel='left', direction='forward',
                            turns=turns, distance_mm=distance)])
            with self.assertRaises(ValueError): self.result()
        row = self.row('r', wheel='left', direction='forward', turns=5, distance_mm=1000)
        self.write_rows('rolling', [row, row])
        with self.assertRaises(ValueError): self.result()

    def test_pending_and_rejected_rows_remain_visible(self):
        a = self.row('pending', wheel='left', direction='forward', turns=5, distance_mm=1000)
        a['accepted'] = ''
        b = self.row('slip', wheel='left', direction='forward', turns=5, distance_mm=1300)
        b.update(accepted='false', notes='synthetic slip fixture')
        self.write_rows('rolling', [a, b])
        result = self.result()
        self.assertEqual(result['rolling'], {})
        self.assertEqual(result['pendingTrials'][0]['trial_id'], 'pending')
        self.assertEqual(result['excludedTrials'][0]['trial_id'], 'slip')

    def test_triangle_sign_baseline_and_no_bicycle_angle_promotion(self):
        row = self.row('s1', pulse_us=1500, sweep_direction='increasing_pulse',
                       left_dx_right_mm=10, left_dy_forward_mm=100,
                       right_dx_right_mm=-20, right_dy_forward_mm=100)
        self.write_rows('steeringTriangles', [row])
        result = self.result()['steeringAngleCandidates'][0]
        self.assertAlmostEqual(result['leftAngleRightPositiveDeg'], math.degrees(math.atan(.1)))
        self.assertAlmostEqual(result['rightAngleRightPositiveDeg'], -math.degrees(math.atan(.2)))
        self.assertIsNone(result['effectiveBicycleAngleRad'])
        self.assertIsNone(result['uncertaintyDeg'])
        row['left_dy_forward_mm'] = 0
        self.write_rows('steeringTriangles', [row])
        with self.assertRaises(ValueError): self.result()

    def test_path_escape_and_unmeasured_data_reject(self):
        self.session['rollingCsv'] = '../outside.csv'
        with self.assertRaises(ValueError): self.result()
        self.session['rollingCsv'] = 'rolling.csv'
        self.session['status'] = 'NOT_MEASURED'
        self.write_rows('rolling', [self.row('r', wheel='left', direction='forward', turns=5, distance_mm=1000)])
        with self.assertRaises(ValueError): self.result()


if __name__ == '__main__':
    unittest.main(verbosity=2)
