"""Compile the real experiment radio transport against a bounded queue mock."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import struct
import zlib

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from prepare_recorder_motion import RADIO
from motion_radio_protocol import Decoder,valid_pong


class MotionRadioTests(unittest.TestCase):
    def test_real_formatter_binary_queue_backpressure_and_timeout(self):
        base=ROOT/'.build/motion-radio-tests';base.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as tmp:
            p=Path(tmp)
            (p/'main.h').write_text('#include <stdint.h>\n#define GPT_TIM_1 0\nuint32_t timer_get(int);\n')
            (p/'app_link.h').write_text('''#include <stdint.h>
#define APP_LINK_TX_SIZE 8192U
#define APP_LINK_RADIO 1U
int app_link_send(unsigned,const uint8_t *,uint32_t);
void app_link_poll(void);
''')
            (p/'task.h').write_text('#define taskSCHEDULER_RUNNING 2\nint xTaskGetSchedulerState(void);\nvoid vTaskDelay(unsigned);\n')
            (p/'radio.c').write_text(RADIO)
            (p/'test.c').write_text(r'''#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
int motion_printf(const char *,...);int motion_radio_write(const uint8_t *,uint32_t);
static uint8_t sent[8192];static uint32_t length,clock_ms;
static unsigned fail,polls,delays;static int scheduler=2;
uint32_t timer_get(int index){(void)index;return clock_ms++;}
int app_link_send(unsigned channel,const uint8_t *data,uint32_t n){
 assert(channel==1);if(fail){--fail;return 0;}memcpy(sent,data,n);length=n;return 1;
}
void app_link_poll(void){++polls;}
int xTaskGetSchedulerState(void){return scheduler;}
void vTaskDelay(unsigned n){assert(n==1&&scheduler==2);++delays;}
int main(void){
 _setmode(_fileno(stdout),_O_BINARY);
 const char expected[]="MOTION id=TEST c26=1\r\n";
 assert(motion_printf("MOTION id=%s c26=%u\r\n","TEST",1)==(int)sizeof(expected)-1);
 assert(length==sizeof(expected)-1+14&&!memcmp(sent,"RMR1",4));
 assert(!memcmp(sent+10,expected,sizeof(expected)-1));
 fwrite(sent,1,length,stdout);
 assert(motion_printf("%s",expected)==(int)sizeof(expected)-1);fwrite(sent,1,length,stdout);
 uint8_t binary[4096];for(unsigned i=0;i<4096;++i)binary[i]=(uint8_t)i;
 fail=3;assert(motion_radio_write(binary,4096));assert(delays==3&&polls>=4);
 assert(length==4096&&!memcmp(sent,binary,4096));
 char huge[1100];memset(huge,'x',sizeof(huge));huge[1099]=0;
 assert(motion_printf("%s",huge)==-1);assert(length==4096);
 assert(!motion_radio_write(binary,8193));
 scheduler=0;fail=10000;delays=0;clock_ms=0;
 assert(!motion_radio_write(binary,4096));assert(!delays&&clock_ms>=3000);
 return 0;
}''')
            exe=p/'test.exe'
            compiled=subprocess.run(['gcc','-std=c99','-O2','-Wall','-Wextra','-Werror','-I'+str(p),
                                     str(p/'radio.c'),str(p/'test.c'),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(compiled.returncode,0,compiled.stderr)
            run=subprocess.run([str(exe)],capture_output=True,timeout=30)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            d=Decoder();self.assertEqual(d.feed(run.stdout),['MOTION id=TEST c26=1'])
            self.assertEqual(d.good,2);self.assertEqual(d.bad,0)

    def test_resync_corruption_truncation_fragmentation_and_nonce(self):
        def frame(seq,payload):
            b=b'RMR1'+struct.pack('<IH',seq,len(payload))+payload
            return b+struct.pack('<I',zlib.crc32(b))
        d=Decoder();line='MOTION PONG nonce=0123456789abcdef source=test c26=1'
        corrupt=bytearray(frame(3,b'evil\n'));corrupt[-1]^=1
        stream=b'foreign\0data'+frame(0,b'\n')+frame(1,b'old\n')+bytes(corrupt)+frame(4,b'\n')+frame(5,line[:20].encode())+frame(6,(line[20:]+'\r\n').encode())
        found=[]
        for c in stream:found+=d.feed(bytes([c]))
        self.assertIn(line,found);self.assertNotIn('evil',found);self.assertGreater(d.bad,0)
        self.assertTrue(valid_pong(line,'0123456789abcdef','test'))
        self.assertFalse(valid_pong(line,'fedcba9876543210','test'))
        self.assertFalse(valid_pong(line,'0123456789abcdef','other'))
        self.assertFalse(valid_pong(line.replace('c26=1','c26=0'),'0123456789abcdef','test'))
        d=Decoder();self.assertEqual(d.feed(frame(10,b'partial')[:-2]),[])
        self.assertLessEqual(len(d.buffer),110)

    def test_gap_never_splices_a_line_and_noise_is_bounded(self):
        def frame(seq,payload):
            b=b'RMR1'+struct.pack('<IH',seq,len(payload))+payload
            return b+struct.pack('<I',zlib.crc32(b))
        d=Decoder();self.assertEqual(d.feed(frame(0,b'\n')+frame(1,b'prefix')+frame(3,b'suffix\n')),[])
        self.assertEqual(d.feed(frame(4,b'intact\n')),['intact'])
        d.feed(b'\x00'*100000);self.assertLessEqual(len(d.buffer),3)


if __name__=='__main__':unittest.main()
