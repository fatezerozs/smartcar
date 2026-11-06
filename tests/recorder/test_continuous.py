"""B2 software preparation: native C producer -> existing archive -> continuous audit."""
import json,struct,zlib,hashlib
from pathlib import Path
import test_recorder as legacy
from vision_data.archive import import_session
from vision_data.common import encoded
from vision_data.continuous import build_report

class ContinuousTests(legacy.RecorderTests):
    def _import(self,ending=None):
        with self.path.open('ab') as f:f.write(self.control_record(b'END1',self.ending() if ending is None else ending))
        (self.source/'session.json').write_bytes(encoded(dict(self.meta,segments=['000000.scraw'])))
        return import_session(self.source,self.root)

    def test_b2_complete_keeps_all_raw_frames_and_sampling_is_separate(self):
        result=self._import();before=hashlib.sha256(self.path.read_bytes()).hexdigest()
        q=build_report(self.root,self.meta['session_id'],self.base/'qc',every=2)
        self.assertEqual(result['status'],'complete');self.assertEqual(q['status'],'complete')
        self.assertEqual(q['frames'],3);self.assertEqual(q['sampling']['frames'],2)
        self.assertEqual(q['observed_valid_record_fps'],50)
        self.assertFalse(q['b2_hardware_accepted']);self.assertTrue(q['synthetic'])
        self.assertFalse(q['telemetry_available']);self.assertIsNone(q['actual_exposure_readback'])
        self.assertEqual(hashlib.sha256(self.path.read_bytes()).hexdigest(),before)
        self.assertEqual(len((self.base/'qc/frame_timeline.csv').read_text().splitlines()),4)
        tasks=json.loads((self.base/'qc/label_studio/tasks.json').read_text())
        self.assertEqual(len(tasks),2);self.assertEqual(tasks[0]['meta']['width'],188)

    def test_b2_crc_valid_ending_wrong_first_not_complete(self):
        end=self.ending();end['first_source_sequence']+=1
        result=self._import(end);self.assertEqual(result['status'],'incomplete')
        report=json.loads((Path(result['path'])/'integrity.json').read_text())
        self.assertTrue(any(i['kind']=='source_boundary_mismatch' for i in report['issues']))

    def test_b2_crc_valid_ending_wrong_last_not_complete(self):
        end=self.ending();end['last_source_sequence']+=1
        result=self._import(end);self.assertEqual(result['status'],'incomplete')

    def test_b2_missing_middle_has_exact_range_not_zero_filled(self):
        frames,_=self.scan();raw=self.path.read_bytes()
        middle=frames[1][0]['payload_offset']-40;next_start=frames[2][0]['payload_offset']-40
        self.path.write_bytes(raw[:middle]+raw[next_start:])
        end=self.ending();end['accepted']=end['recorded_frames']=2;end['queue_full']=1;end['status']='incomplete'
        self._import(end);q=build_report(self.root,self.meta['session_id'],self.base/'qc')
        self.assertEqual(q['status'],'incomplete');self.assertEqual(q['frames'],2)
        self.assertEqual(q['missing_frame_ranges'],[{'first':2**32+1,'last':2**32+1,'count':1,'evidence':'validated_record_sequence'}])
        self.assertFalse(q['b2_hardware_accepted'])

    def test_b2_nonmonotonic_time_not_complete(self):
        frames,_=self.scan();b=bytearray(self.path.read_bytes());start=frames[1][0]['payload_offset']-40
        struct.pack_into('<Q',b,start+16,frames[0][0]['timestamp_us'])
        struct.pack_into('<I',b,start+36,zlib.crc32(b[start:start+36]))
        tail=frames[1][0]['payload_offset']+frames[1][0]['payload_size']
        struct.pack_into('<I',b,tail,zlib.crc32(b[start:tail]));self.path.write_bytes(b)
        self._import();q=build_report(self.root,self.meta['session_id'],self.base/'qc')
        self.assertEqual(q['status'],'incomplete');self.assertFalse(q['timestamp_strictly_increasing'])

    def test_b2_missing_final_manifest_never_claims_complete(self):
        import_session(self.source,self.root)
        q=build_report(self.root,self.meta['session_id'],self.base/'qc')
        self.assertEqual(q['status'],'incomplete');self.assertTrue(q['unknown_source_loss'])

    def test_b2_real_metadata_flag_alone_not_hardware_acceptance(self):
        # Rewrite the native header + JSON consistently, changing only provenance.
        raw=self.path.read_bytes();old_size=struct.unpack_from('<I',raw,48)[0]
        self.meta['synthetic']=False;meta=encoded(self.meta);head=bytearray(raw[:64])
        struct.pack_into('<II',head,48,len(meta),zlib.crc32(meta));struct.pack_into('<I',head,60,zlib.crc32(head[:60]))
        self.path.write_bytes(head+meta+raw[64+old_size:]);self._import()
        q=build_report(self.root,self.meta['session_id'],self.base/'qc')
        self.assertFalse(q['synthetic']);self.assertFalse(q['b2_hardware_accepted']);self.assertFalse(q['production_ready'])

    def test_b2_output_collision_refuses_overwrite(self):
        self._import();out=self.base/'qc';out.mkdir();sentinel=out/'keep';sentinel.write_bytes(b'unchanged')
        with self.assertRaisesRegex(ValueError,'already exists'):build_report(self.root,self.meta['session_id'],out)
        self.assertEqual(sentinel.read_bytes(),b'unchanged')
