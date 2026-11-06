import json
import test_recorder as legacy
from verify_b2_session import verify

class DownloadValidationTests(legacy.RecorderTests):
 def test_download_full_pixel_roundtrip_stays_synthetic(self):
  self.complete()
  self.assertFalse(verify(self.source,self.base/'pc'))
  result=json.loads((self.base/'pc/pixel-audit.json').read_text())
  self.assertEqual(result['frames'],3)
  self.assertTrue(result['all_source_pixels_match_archive'])
  self.assertTrue(result['complete_observed_window'])
  self.assertFalse(result['b2_full_acceptance'])

 def test_download_unlisted_entry_rejected(self):
  self.complete();(self.source/'other.bin').write_bytes(b'preserve')
  with self.assertRaisesRegex(ValueError,'entries differ'):verify(self.source,self.base/'pc')
  self.assertEqual((self.source/'other.bin').read_bytes(),b'preserve')

 def test_download_crc_damage_never_complete(self):
  self.complete();frames,_=self.scan();b=bytearray(self.path.read_bytes())
  b[frames[1][0]['payload_offset']]^=1;self.path.write_bytes(b)
  self.assertFalse(verify(self.source,self.base/'pc'))
  q=json.loads((self.base/'pc/continuous/continuous_report.json').read_text())
  self.assertEqual(q['status'],'incomplete');self.assertEqual(q['frames'],2)
