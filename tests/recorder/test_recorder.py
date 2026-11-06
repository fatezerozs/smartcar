import copy
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from vision_data import scraw_v1
from vision_data.archive import import_session, verify_session, load_session
from vision_data.common import encoded


class RecorderTests(unittest.TestCase):
    @classmethod
    def compile_harness(cls, output, sources, *, mocks=False, defines=()):
        family=os.environ.get('SMARTCAR_TEST_COMPILER','gcc')
        mode=os.environ.get('SMARTCAR_TEST_MODE','optimized')
        compiler=os.environ.get('SMARTCAR_TEST_COMPILER_PATH') or shutil.which('cl.exe' if family=='msvc' else 'gcc')
        if family not in ('gcc','msvc') or mode not in ('optimized','asan') or not compiler:
            raise RuntimeError('Recorder tests need GCC or an initialized C32 compiler environment')
        includes=([ROOT/'tests/recorder/mocks'] if mocks else [])+[ROOT/'demo/recorder/include']
        if family=='msvc':
            flags=['/nologo','/utf-8','/std:c11','/W4','/WX','/D_CRT_SECURE_NO_WARNINGS']
            flags+=['/Od','/Zi','/fsanitize=address'] if mode=='asan' else ['/O2']
            flags+=['/I'+str(p) for p in includes]+['/D'+d for d in defines]
            flags+=[str(ROOT/p) for p in sources]+['/Fo'+str(cls.build)+os.sep,
                    '/Fd'+str(cls.build)+os.sep,'/Fe'+str(output)]
        else:
            flags=['-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation']
            flags+=['-O0','-g','-fsanitize=address'] if mode=='asan' else ['-O2']
            flags+=['-I'+str(p) for p in includes]+['-D'+d for d in defines]
            flags+=[str(ROOT/p) for p in sources]+['-o',str(output)]
        result=subprocess.run([compiler,*flags],cwd=cls.build,capture_output=True,text=True,timeout=60)
        if result.returncode:
            raise RuntimeError(f'{family}/{mode} harness compilation failed:\n'+result.stdout+result.stderr)

    @classmethod
    def setUpClass(cls):
        build_root=ROOT/'.build/recorder-tests'
        build_root.mkdir(parents=True,exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(dir=build_root)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.build = Path(cls.temporary.name)
        cls.exe = cls.build / 'recorder_core.exe'
        cls.compile_harness(cls.exe,['demo/recorder/recorder_core.c','tests/recorder/core_harness.c'])
        cls.storage_exe=cls.build/'recorder_storage.exe'
        cls.compile_harness(cls.storage_exe,['demo/recorder/recorder_storage.c','demo/recorder/recorder_core.c',
            'tests/recorder/storage_harness.c'],mocks=True,defines=['RECORDER_HOST_TEST=1'])
        cls.osa_exe=cls.build/'recorder_osa.exe'
        cls.compile_harness(cls.osa_exe,['demo/recorder/recorder_sd_osa.c','tests/recorder/osa_harness.c'],mocks=True)
        print('Recorder harnesses: compiler='+os.environ.get('SMARTCAR_TEST_COMPILER','gcc')+
              ' mode='+os.environ.get('SMARTCAR_TEST_MODE','optimized')+'; compiled 3 executables once',flush=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='recorder-test-',dir=ROOT.parent)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.source = self.base / 'card' / 'R0000001'
        self.source.mkdir(parents=True)
        self.root = self.base / 'archive'
        self.root.mkdir()
        self.meta = {'format':'SCRAW-1','schema_version':'1.0','session_id':'synthetic-v1-c-producer',
            'synthetic':True,'width':188,'height':120,'stride':188,'pixel_format':'GRAY8',
            'timestamp_unit':'us','timestamp_semantics':'csi_irq_gpt1_ms_x1000','timestamp_resolution_us':1000,
            'session_uuid_hex':bytes(range(16)).hex(),'firmware_build':'HOST_SYNTHETIC_ONLY'}
        self.path = self.source/'000000.scraw'
        subprocess.run([str(self.exe),str(self.path),json.dumps(self.meta,separators=(',',':'))],check=True,capture_output=True,timeout=30)

    def tearDown(self): self.temp.cleanup()

    def ending(self):
        return {'observed_capture':3,'capture_frames':None,'vision_processed_frames':2,'accepted':3,
            'recorded_frames':3,'synced_before_end':0,'base_source_sequence':2**32-1,
            'first_source_sequence':2**32,'last_source_sequence':2**32+2,'queue_full':0,'pool_full':0,
            'capture_ambiguous':0,'unknown_source_loss':False,'storage_errors':0,'status':'complete_observed_window'}

    @staticmethod
    def control_record(tag,obj):
        payload=encoded(obj)
        prefix=struct.pack('<4sIQQIII',tag,44+len(payload),0,0,len(payload),zlib.crc32(payload),1)
        head=prefix+struct.pack('<I',zlib.crc32(prefix))
        return head+payload+struct.pack('<I',zlib.crc32(head+payload))

    def complete(self):
        with self.path.open('ab') as f:f.write(self.control_record(b'END1',self.ending()))
        (self.source/'session.json').write_bytes(encoded(dict(self.meta,segments=['000000.scraw'])))

    def scan(self,path=None):
        report={'issues':[],'ending':None}
        result=list(scraw_v1.scan(path or self.path,scraw_v1.expected(dict(self.meta,segments=['000000.scraw']),0),report))
        return result,report

    def test_capture_ownership_pressure_and_crc_c_harness(self):
        result=subprocess.run([str(self.exe)],check=True,capture_output=True,text=True,timeout=30)
        self.assertIn('100000',result.stdout)

    def test_actual_sd_osa_deadlines_wake_wrap_and_quarantine(self):
        result=subprocess.run([str(self.osa_exe)],check=True,capture_output=True,text=True,timeout=30)
        self.assertIn('PASS',result.stdout)

    def test_actual_storage_task_errors_rotation_and_export_isolation(self):
        for case in (0,1,2,3,4,5,7,8,9,10):
            with self.subTest(case=case):
                result=subprocess.run([str(self.storage_exe),str(case)],check=True,capture_output=True,text=True,timeout=30)
                self.assertIn('PASS',result.stdout)

    def test_actual_storage_task_complete_output_imports(self):
        out=self.base/'actual-storage';out.mkdir()
        subprocess.run([str(self.storage_exe),'7',str(out)],check=True,capture_output=True,timeout=30)
        result=import_session(out,self.root)
        self.assertEqual(result['status'],'complete');self.assertTrue(result['synthetic'])
        self.assertEqual(verify_session(self.root,result['session_id'])['frames'],3)

    def test_actual_storage_task_sync_failure_recovery_not_clean_session(self):
        out=self.base/'failed-storage';out.mkdir()
        subprocess.run([str(self.storage_exe),'3',str(out)],check=True,capture_output=True,timeout=30)
        self.assertFalse((out/'session.json').exists())
        result=import_session(out,self.root)
        self.assertEqual(result['status'],'incomplete');self.assertEqual(result['frames'],3)

    def test_actual_storage_task_gap_record_not_silently_complete(self):
        out=self.base/'gapped-storage';out.mkdir()
        subprocess.run([str(self.storage_exe),'9',str(out)],check=True,capture_output=True,timeout=30)
        result=import_session(out,self.root)
        self.assertEqual(result['status'],'incomplete')
        report=json.loads((Path(result['path'])/'integrity.json').read_text())
        self.assertTrue(report['segments'][0]['gaps'])

    def test_independent_c_producer_python_parser_and_64bit_times(self):
        records,report=self.scan()
        self.assertEqual(len(records),3);self.assertEqual(report['issues'],[])
        self.assertEqual(records[0][0]['source_sequence'],2**32)
        self.assertEqual(records[0][0]['timestamp_us'],2**32*1000)
        self.assertEqual(records[2][1],bytes([22])*22560)
        raw=self.path.read_bytes()
        self.assertEqual(raw[:16],bytes.fromhex('5343524157310d0a0100000040000000'))
        self.assertEqual(int.from_bytes(raw[60:64],'little'),zlib.crc32(raw[:60]))

    def test_v1_import_without_draft_switch_and_pixel_verification(self):
        self.complete();result=import_session(self.source,self.root)
        self.assertEqual(result['status'],'complete');self.assertEqual(result['format_status'],'TRAINING_V1')
        self.assertEqual(verify_session(self.root,self.meta['session_id'])['frames'],3)

    def test_power_loss_no_manifest_read_only_recovery_is_incomplete(self):
        before=self.path.read_bytes();result=import_session(self.source,self.root)
        self.assertEqual(result['status'],'incomplete');self.assertEqual(result['frames'],3)
        self.assertFalse((self.source/'session.json').exists());self.assertEqual(self.path.read_bytes(),before)
        archived=Path(result['path']);self.assertTrue((archived/'recovered_session.json').is_file())
        self.assertFalse((archived/'A0/session.json').exists())

    def test_corruption_resynchronizes_without_invented_frame(self):
        records,_=self.scan();raw=bytearray(self.path.read_bytes())
        raw[records[1][0]['payload_offset']+8]^=0xff;self.path.write_bytes(raw)
        result,report=self.scan();self.assertEqual([f['source_sequence'] for f,_ in result],[2**32,2**32+2])
        self.assertIn('record_crc',[i['kind'] for i in report['issues']])
        self.assertIn('resynchronized',[i['kind'] for i in report['issues']])

    def test_truncation_keeps_only_completed_records(self):
        self.path.write_bytes(self.path.read_bytes()[:-20]);result,report=self.scan()
        self.assertEqual(len(result),2);self.assertTrue(report['issues'])

    def test_partial_final_json_is_archived_and_recovers_incomplete(self):
        broken=b'{"format":"SCRAW-1","session_id":'
        (self.source/'session.json').write_bytes(broken)
        result=import_session(self.source,self.root)
        self.assertEqual(result['status'],'incomplete');self.assertEqual(result['frames'],3)
        self.assertEqual((Path(result['path'])/'A0/session.json').read_bytes(),broken)

    def test_header_crc_geometry_uuid_and_unknown_version_fail_closed(self):
        original=self.path.read_bytes()
        for at,value in ((8,2),(14,1),(26,2),(28,2),(30,1),(56,1)):
            raw=bytearray(original);raw[at]=value;raw[60:64]=struct.pack('<I',zlib.crc32(raw[:60]));self.path.write_bytes(raw)
            with self.assertRaises(ValueError):self.scan()
        self.path.write_bytes(original)
        wrong=copy.deepcopy(self.meta);wrong['session_uuid_hex']='ff'*16
        with self.path.open('rb') as f:self.assertEqual(scraw_v1.header(f)[0],self.meta)
        report={'issues':[],'ending':None}
        with self.assertRaises(ValueError):list(scraw_v1.scan(self.path,scraw_v1.expected(wrong,0),report))

    def test_missing_segment_index_is_preserved_as_incomplete(self):
        self.complete();raw=bytearray(self.path.read_bytes());raw[16:20]=struct.pack('<I',1)
        raw[60:64]=struct.pack('<I',zlib.crc32(raw[:60]));self.path.write_bytes(raw)
        self.path.rename(self.source/'000001.scraw');(self.source/'session.json').unlink()
        result=import_session(self.source,self.root);self.assertEqual(result['status'],'incomplete')
        self.assertEqual(result['frames'],3)

    def test_unknown_loss_not_promoted_to_exact_capture(self):
        ending=self.ending();ending['capture_ambiguous']=1;ending['unknown_source_loss']=True
        with self.path.open('ab') as f:f.write(self.control_record(b'END1',ending))
        (self.source/'session.json').write_bytes(encoded(dict(self.meta,segments=['000000.scraw'])))
        result=import_session(self.source,self.root);self.assertEqual(result['status'],'incomplete')
        base,manifest,frames=load_session(self.root,self.meta['session_id'])
        report=json.loads((base/'integrity.json').read_text())
        self.assertIsNone(report['segments'][0]['ending']['capture_frames'])

    def test_data_after_end_is_rejected(self):
        original=self.path.read_bytes();records,_=self.scan();start=records[-1][0]['payload_offset']-40
        self.complete()
        with self.path.open('ab') as f:f.write(original[start:])
        with self.assertRaisesRegex(ValueError,'after END'):self.scan()

    def test_identity_mix_is_rejected_before_recovery(self):
        raw=bytearray(self.path.read_bytes());raw[16:20]=struct.pack('<I',1);raw[32]^=1
        raw[60:64]=struct.pack('<I',zlib.crc32(raw[:60]));(self.source/'000001.scraw').write_bytes(raw)
        with self.assertRaises(ValueError):import_session(self.source,self.root)


if __name__=='__main__':unittest.main()
