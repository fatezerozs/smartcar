import struct,sys,unittest,zlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[3]/'tools'))
from verify_b3_static import decode
def fixture():
 records=[]
 def add(kind,ms,v):records.append(struct.pack('<4I12i',len(records)+1,kind,ms,0,*(v+[0]*(12-len(v)))))
 for ch,pin in enumerate((85,95,185)):add(8,100,[pin,0,0,0,0,0,0,0,ch,10000,1])
 add(1,110,[65535,1]);add(2,110,[1,110,0,0,0,0,0,0,0,0,0,4]);add(3,111,[1,110,0,0,0,0,0,0,0,600,300,0]);add(5,120,[1,1,0,0,1,2]);add(6,120,[1,119,1,1]);raw=b''.join(records)
 h=f'B3TRACE DATA format=B3T1 run=1 count={len(records)} bytes={len(raw)} crc32={zlib.crc32(raw):08X} lost=0 reason=1 hz=600000000 pi_gap_max_ms=1 pi_max_cycles=600 mask_max_cycles=300 emit_max_cycles=60 m1=85 m2=95 servo=185 static=1 synthetic=1 source=HOST_SYNTHETIC_ONLY'
 return h,raw
class StaticDecoderTests(unittest.TestCase):
 def test_complete_is_not_motion_acceptance(self):
  r,rows=decode(*fixture());self.assertEqual(r['status'],'complete');self.assertTrue(r['synthetic']);self.assertFalse(r['b3_motion_accepted']);self.assertFalse(r['active_pi_observed']);self.assertTrue(r['all_three_actuator_snapshots_present']);self.assertEqual(rows[-1]['capture_age_ms'],1)
 def test_crc_and_short_read(self):
  h,b=fixture()
  for bad in (b[:-1],bytes([b[0]^1])+b[1:]):
   with self.assertRaises(ValueError):decode(h,bad)
 def test_lost_or_manual_freeze_is_incomplete(self):
  h,b=fixture()
  for bad in (h.replace('lost=0','lost=1'),h.replace('reason=1','reason=3')):self.assertEqual(decode(bad,b)[0]['status'],'incomplete')
 def test_nonzero_applied_pwm_and_unknown_kind_reject(self):
  h,b=fixture()
  for offset,value in ((24,1),(4,99)):
   bad=bytearray(b);struct.pack_into('<I',bad,offset,value);new=h.replace(f'{zlib.crc32(b):08X}',f'{zlib.crc32(bad):08X}')
   with self.assertRaises(ValueError):decode(new,bad)
 def test_missing_raw_reference_is_not_complete(self):
  h,b=fixture();bad=bytearray(b);struct.pack_into('<i',bad,4*64+16+11*4,999);h=h.replace(f'{zlib.crc32(b):08X}',f'{zlib.crc32(bad):08X}');self.assertEqual(decode(h,bad)[0]['status'],'incomplete')
 def test_duplicate_fields_and_unknown_version(self):
  h,b=fixture()
  for bad in (h+' count=9',h.replace('B3T1','B3T2')):
   with self.assertRaises(ValueError):decode(bad,b)
 def test_timestamp_wrap_and_backward_time(self):
  h,_=fixture()
  for times,valid in [((0xfffffff0,16),True),((100,99),False)]:
   raw=b''.join(struct.pack('<4I12i',i+1,1,ms,0,*([0]*12)) for i,ms in enumerate(times))
   header=rewrite_header(h,raw)
   if valid:
    rows=decode(header,raw)[1];self.assertEqual(rows[1]['timestamp_ms_extended']-rows[0]['timestamp_ms_extended'],32)
   else:
    with self.assertRaises(ValueError):decode(header,raw)
 def test_active_pi_is_not_motion_authority(self):
  h,b=fixture();raw=bytearray(b);struct.pack_into('<I',raw,5*64+4,9);struct.pack_into('<i',raw,5*64+16+2*4,10);struct.pack_into('<i',raw,5*64+16+11*4,1)
  r,_=decode(rewrite_header(h,raw),raw);self.assertTrue(r['active_pi_observed']);self.assertFalse(r['b3_motion_accepted'])
def rewrite_header(h,b):
 import re
 h=re.sub(r'count=\d+',f'count={len(b)//64}',h);h=re.sub(r'bytes=\d+',f'bytes={len(b)}',h);return re.sub(r'crc32=[0-9A-F]+',f'crc32={zlib.crc32(b):08X}',h)
if __name__=='__main__':unittest.main()
