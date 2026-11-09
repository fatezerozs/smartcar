"""Execute the generated Vision task at the finite recording boundaries."""
import contextlib
import io
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_recorder_joint import prepare
from prepare_recorder_b3_static import function


class JointCaptureWindowTests(unittest.TestCase):
    def test_actual_generated_vision_task_excludes_post_window_frames(self):
        compiler = shutil.which('gcc')
        self.assertIsNotNone(compiler, 'GCC required for this standalone regression')
        base = ROOT / '.build/recorder-joint-20261106'
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='vision-window-', dir=base) as tmp:
            directory = Path(tmp)
            with contextlib.redirect_stdout(io.StringIO()):
                prepare(directory / 'candidate')
            generated = (directory / 'candidate/adapted/recorder_capture.c').read_text()
            original = (ROOT / 'demo/recorder/recorder_capture.c').read_text()
            harness = r'''
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#define REC_NONE 255
#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define GPT_TIM_1 0
static uint8_t pending, ack, vision_owner[3];
typedef struct {uint64_t sequence;} frame_t;
static frame_t vision[3];
static struct {uint64_t base_source,last_source,vision_processed;uint32_t vision_max_ms;} counters;
static jmp_buf done;
static unsigned published;
static uint32_t timer_get(int ignored){(void)ignored;return 1;}
void recorder_vision_init(void){}
static void recorder_publish_vision(frame_t *frame){(void)frame;++published;}
static void vTaskDelay(int ignored){(void)ignored;longjmp(done,1);}
TASK
static int check(uint64_t sequence,int recording,unsigned expected){
    pending=0;ack=(uint8_t)recording;vision[0].sequence=sequence;
    counters.base_source=10;counters.last_source=110;counters.vision_processed=0;
    published=0;
    if(!setjmp(done))recorder_vision_task(0);
    return published==1 && counters.vision_processed==expected;
}
int main(void){
    if(!check(111,1,0)||!check(10,1,0)||!check(11,1,1)||!check(110,1,1)||!check(20,0,0))return 1;
    puts("PASS actual Vision task: base excluded, first/final included, post-cap excluded, recording OFF excluded");
    return 0;
}
'''
            for label, source, expected in [('before', original, 1), ('fixed', generated, 0)]:
                blocks = []
                function(source, 'recorder_vision_task', lambda block: blocks.append(block) or block)
                cfile = directory / (label + '.c')
                executable = directory / (label + '.exe')
                cfile.write_text(harness.replace('TASK', 'void ' + blocks[0]))
                compiled = subprocess.run([compiler, '-std=c99', '-O2', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                                           str(cfile), '-o', str(executable)], capture_output=True, text=True, timeout=60)
                self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
                result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
                if label == 'fixed':
                    print(result.stdout, end='')


if __name__ == '__main__':
    unittest.main()
