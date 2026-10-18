"""Training pipeline tests use invented labels, never claim real-track training."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("fit", Path(__file__).resolve().parents[2] / "tools/fit_track_model.py")
fit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fit)


def fixture():
    return [dict(sequence="synthetic", ms=str((i * 3 + j) * 20), state=s,
                 **{k: "0.2" for k in fit.FEATURES})
            for i, s in enumerate(fit.STATES) for j in range(3)]


class FitTests(unittest.TestCase):
    def test_normalized(self):
        m = fit.fit(fixture(), minimum=2)
        self.assertAlmostEqual(sum(m["prior"]), 1)
        for row in m["transition"]:
            self.assertAlmostEqual(sum(row), 1)
        self.assertGreater(m["variance"][1][0], 0)
        self.assertIn("m->trained = 1", fit.header(m))

    def test_missing_class(self):
        with self.assertRaises(ValueError):
            fit.fit(fixture()[:-3], minimum=2)

    def test_missing_measurement(self):
        rows = fixture()
        for row in rows:
            row["heading"] = ""
        with self.assertRaises(ValueError):
            fit.fit(rows, minimum=2)

    def test_reordered(self):
        rows = fixture()
        rows[1]["ms"] = "0"
        with self.assertRaises(ValueError):
            fit.fit(rows, minimum=2)

    def test_nan(self):
        rows = fixture()
        rows[0]["heading"] = "nan"
        with self.assertRaises(ValueError):
            fit.fit(rows, minimum=2)

    def test_recording_boundary(self):
        rows = fixture()
        for row in rows:
            row["sequence"] = row["state"]
        model = fit.fit(rows, minimum=2)
        self.assertTrue(all(abs(x - 1/len(fit.STATES)) < 1e-6 for x in model["transition"][0]))


if __name__ == "__main__":
    unittest.main()
