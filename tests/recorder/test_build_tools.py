"""Build preparation contracts only: never starts Keil or a C compiler."""
import hashlib
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('recorder_prepare_contract',ROOT/'tools/prepare_recorder.py')
prepare=importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)


class BuildPreparationTests(unittest.TestCase):
    def setUp(self):
        parent=ROOT/'.build/recorder-tools-tests';parent.mkdir(parents=True,exist_ok=True)
        temporary=tempfile.TemporaryDirectory(dir=parent);self.addCleanup(temporary.cleanup)
        self.root=Path(temporary.name)

    def test_identical_bytes_keep_timestamp_and_changed_bytes_are_written(self):
        path=self.root/'generated/file.h'
        self.assertTrue(prepare.write_if_changed(path,b'first'))
        stamp=path.stat().st_mtime_ns
        self.assertFalse(prepare.write_if_changed(path,b'first'))
        self.assertEqual(path.stat().st_mtime_ns,stamp)
        self.assertTrue(prepare.write_if_changed(path,b'second'))
        self.assertEqual(path.read_bytes(),b'second')

    def test_repeated_preparation_preserves_all_generated_outputs(self):
        for relative in prepare.SOURCES.values():
            target=self.root/relative;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(ROOT/relative,target)
        for relative in ('demo/recorder','libraries/third_party/FreeRTOS-Kernel'):
            shutil.copytree(ROOT/relative,self.root/relative,dirs_exist_ok=True)
        template=self.root/'demo/mdk/base/rt1064.uvprojx';template.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(ROOT/'demo/mdk/base/rt1064.uvprojx',template)
        output=self.root/'.build/recorder-train/adapted'
        def git_only(command,**kwargs):
            self.assertEqual(command[0],'git')
            self.assertEqual(kwargs['timeout'],15)
            return 'a'*40+'\n' if command[1]=='rev-parse' else ' M fixture\n'
        with patch.object(prepare,'ROOT',self.root),patch.object(prepare,'OUT',output),\
                patch.object(prepare,'PIN',self.root/'demo/recorder/sdk_sources.lock.json'),\
                patch.object(prepare.subprocess,'check_output',side_effect=git_only):
            project=Path(prepare.prepare(False))
            files=[*output.iterdir(),project]
            snapshot={p:(p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in files}
            prepare.prepare(False)
            self.assertEqual(snapshot,{p:(p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()) for p in files})
            tree=ET.parse(project)
            self.assertTrue(all(node.text=='0' for node in tree.iter() if node.tag.startswith('RunUserProg')))
            self.assertIn('MIDTERM_VISUAL_REVIEWED=0',tree.findtext('.//Cads/VariousControls/Define'))
            buzzer=(output/'board_io.c').read_bytes()
            self.assertIn(b'{(void)on;gpio_set_level(BOARD_BUZZER_PIN,GPIO_LOW);}',buzzer)
            self.assertNotIn(b'on?GPIO_HIGH:GPIO_LOW',buzzer)
            self.assertIn(b'on?GPIO_HIGH:GPIO_LOW',(self.root/'demo/code/board_io.c').read_bytes())
            # Check the generated native bus and pad policy, not only header
            # flags: the upstream host/card source hard-codes 4-bit capability.
            host=(output/'fsl_sdmmc_host.c').read_text('latin1')
            self.assertIn('host->capability &= ~(',host)
            self.assertIn('kSDMMCHOST_Support4BitDataWidth',host)
            sd=(output/'fsl_sd.c').read_text('latin1')
            self.assertIn('(card->host->capability & (uint32_t)kSDMMCHOST_Support4BitDataWidth)',sd)
            board=(output/'sdmmc_config.c').read_text('latin1')
            self.assertIn('recorder_sd_pad_config(freq);',board)
            self.assertNotIn('IOMUXC_GPIO_SD_B0_05_USDHC1_DATA3',board)
            header=(output/'fsl_sdmmc_host.h').read_text('latin1')
            self.assertRegex(header,r'SDMMCHOST_SUPPORT_4_BIT_WIDTH\s+\(0U\)')
            self.assertRegex(header,r'SDMMCHOST_SUPPORT_VOLTAGE_1V8\s+\(0U\)')
            prepare.prepare(True)
            changed=sorted(p.name for p in files if snapshot[p]!=(p.stat().st_mtime_ns,hashlib.sha256(p.read_bytes()).hexdigest()))
            self.assertEqual(changed,['provenance.json','recorder_identity.h','recorder_train.uvprojx'])
            self.assertIn('MIDTERM_VISUAL_REVIEWED=1',ET.parse(project).findtext('.//Cads/VariousControls/Define'))

    def test_peripheral_init_precedes_first_kernel_object(self):
        # The CM7 port starts with nonzero critical nesting: queue creation
        # masks BASEPRI until the scheduler starts. Camera IRQ configuration
        # must be done before this transition, while its pool is already valid.
        main=(ROOT/'demo/recorder/recorder_main.c').read_text(encoding='utf-8')
        main=main[main.index('int main(void)'):]
        self.assertLess(main.index('recorder_capture_prepare();'),main.index('mt9v03x_init()'))
        self.assertLess(main.index('mt9v03x_init()'),main.index('recorder_runtime_init();'))
        self.assertLess(main.index('recorder_sd_board_init();'),main.index('recorder_runtime_init();'))
        self.assertLess(main.index('recorder_runtime_init();'),main.index('vTaskStartScheduler();'))


if __name__=='__main__':unittest.main(verbosity=2)
