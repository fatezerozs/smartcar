import contextlib,io,tempfile,unittest
from pathlib import Path
from review_b2_metrics import review

class MetricsReviewTests(unittest.TestCase):
 def parse(self,text):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'HOST_SYNTHETIC_ONLY.txt';p.write_text(text)
   with contextlib.redirect_stdout(io.StringIO()):return review(p)
 def fixture(self):
  lines=['B2METRIC session=2:/R0000002 csi_width_cycles=240 csi_height=94 csi_cr1=000000FF csi_cr18=00000001 sd_hz=24750000 sd_bits=1',
   'B2METRIC runtime_total_us=10000 idle_attributed_us=5000 nonidle_permille=500 csi_irq_count=100 csi_irq_max_us=3 sd_irq_count=50 sd_irq_max_us=7']
  lines += ['B2METRIC task='+n+' runtime_us=100 stack_free_words=20' for n in ('Capture','Writer','Monitor','IDLE')]
  lines += ['B2METRIC io='+n+' count=5 sample_overflow=0 errors=0 bytes=10 total_us=50 p50_us=5 p95_us=10 p99_us=10 max_us=20' for n in ('write','sync','close')]
  return '\n'.join(lines+['B2METRIC report_stack_free_words=800','B2METRIC END session=2:/R0000002'])
 def test_measurements_do_not_alone_grant_acceptance(self):
  r=self.parse(self.fixture());self.assertTrue(r['measurements_present_and_valid']);self.assertFalse(r['b2_full_accepted'])
 def test_old_status_log_does_not_invent_metrics(self):
  r=self.parse('[B2] state=4 error=0 written=100 synced=100');self.assertFalse(r['measurements_present_and_valid'])
 def test_overflow_missing_io_and_invalid_geometry_fail(self):
  for text in (self.fixture().replace('sample_overflow=0','sample_overflow=1'),self.fixture().replace('io=close','io=other'),self.fixture().replace('csi_height=94','csi_height=1'),self.fixture().replace('B2METRIC END','B2METRIC TRUNCATED')):
   self.assertFalse(self.parse(text)['measurements_present_and_valid'])
