"""Prepare a stationary camera candidate from audited existing Recorder modules.
No original SDK generation script, directory-wide SDK copy, hardware or download.
"""
from pathlib import Path
import argparse,json,hashlib,shutil,os,xml.etree.ElementTree as E,subprocess,re
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def prepare(output,dependencies_from=None):
 out=Path(output).resolve();allowed=(ROOT/'.build/recorder-camera-stages-20261106').resolve()
 assert out.is_relative_to(allowed) and out!=allowed and not out.exists()
 src=ROOT/'.build/recorder-train/adapted';proof=json.loads((src/'provenance.json').read_text())
 for name,h in proof['baseline_sha256'].items():assert sha(ROOT/name)==h,name
 for name,h in proof['generated_sha256'].items():assert sha(src/name)==h,name
 out.mkdir(parents=True);adapt=out/'adapted';adapt.mkdir();(out/'Objects').mkdir();(out/'Listings').mkdir()
 needed=['zf_device_mt9v03x.c','fsl_sdmmc_host.c','fsl_sdmmc_host.h','sdmmc_config.c','fsl_sd.c','fsl_usdhc.c','ff.c','ff.h','ffconf.h','diskio.c','diskio.h','ffunicode.c']
 for name in needed:shutil.copyfile(src/name,adapt/name)
 original=ROOT/'demo/mdk/training/recorder_train.uvprojx';tree=E.parse(original);target=tree.find('.//Target')
 target.find('TargetName').text='RECORDER_CAMERA_B2_STATIC_1'
 mapping={f.findtext('FileName').lower():(original.parent/f.findtext('FilePath').replace('\\','/')).resolve() for f in target.findall('.//File')}
 names=['startup_MIMXRT1064.S','system_MIMXRT1064.c','board.c','clock_config.c','fsl_common.c','fsl_common_arm.c','fsl_sdmmc_common.c','zf_device_config.lib','fsl_clock.c','fsl_gpio.c','fsl_gpt.c','fsl_csi.c','fsl_dcdc.c','fsl_lpuart.c','fsl_cache.c','fsl_flexspi_nor_boot.c','evkmimxrt1064_flexspi_nor_config.c','evkmimxrt1064_sdram_ini_dcd.c','zf_common_clock.c','zf_common_interrupt.c','zf_common_function.c','zf_common_fifo.c','zf_driver_gpio.c','zf_driver_uart.c','zf_driver_delay.c','zf_driver_timer.c','zf_driver_soft_iic.c','zf_device_camera.c','zf_device_type.c','zf_driver_csi.c','tasks.c','queue.c','list.c','port.c']
 sources=[]
 for name in names:
  if name.lower() not in mapping:raise ValueError('Missing audited source '+name)
  sources.append(mapping[name.lower()])
 sources += [adapt/n for n in needed if n.endswith('.c')]
 sources += [ROOT/'demo/recorder'/n for n in ['recorder_core.c','recorder_capture.c','recorder_storage.c','recorder_disk.c','recorder_sd_pins.c','recorder_sd_osa.c']]
 sources += [ROOT/'demo/recorder_b2/b2_main.c']
 for e in tree.findall('.//IncludePath'):
  if e.text:
   paths=[(original.parent/x.strip().replace('\\','/')).resolve() for x in e.text.split(';') if x.strip()]
   paths=[adapt if p==src else p for p in paths]
   e.text=';'.join(map(str,[ROOT/'demo/recorder_b2/include',*paths]))
 c=target.find('.//Cads/VariousControls')
 c.find('Define').text='RECORDER_TRAINING=1,VISION_MIDTERM_TRAINING=1,VISION_ENCODER_ENABLE=0,VISION_DRIVE_ENABLE=0,VISION_MIDTERM_AUTONOMOUS=0,VISION_MIDTERM_ROUTE_TRIAL=0,VISION_ROUTE_SWITCH=0,VISION_START_SWITCH_ENABLE=0,VISION_DUAL_UART_ENABLE=0,MIDTERM_VISUAL_REVIEWED=0,CPU_MIMXRT1064DVL6A,SKIP_SYSCLK_INIT,XIP_EXTERNAL_FLASH=1,XIP_BOOT_HEADER_DCD_ENABLE=1,XIP_BOOT_HEADER_ENABLE=1,FSL_DRIVER_TRANSFER_DOUBLE_WEAK_IRQ=0,SDK_DEBUGCONSOLE=0,SD_ENABLED,DEBUG'
 c.find('MiscControls').text='-fno-common -fdata-sections -ffunction-sections -ffreestanding -fno-builtin -mthumb -Wno-invalid-source-encoding -include "'+(ROOT/'demo/recorder_b2/include/b2_build.h').as_posix()+'"'
 common=target.find('.//TargetCommonOption');common.find('OutputDirectory').text=str(out/'Objects')+'\\';common.find('ListingPath').text=str(out/'Listings')+'\\';common.find('OutputName').text='recorder_b2';common.find('CreateHexFile').text='0'
 for hook in ['BeforeCompile','BeforeMake','AfterMake']:
  for e in common.find(hook):
   if e.tag.startswith('Run') or e.tag.startswith('nStop'):e.text='0'
   elif 'Name' in e.tag:e.text=''
 target.find('.//UpdateFlashBeforeDebugging').text='0'
 for e in tree.findall('.//ScatterFile'):e.text=str((original.parent/e.text.replace('\\','/')).resolve())
 groups=target.find('Groups');groups.clear();g=E.SubElement(groups,'Group');E.SubElement(g,'GroupName').text='stationary_camera_no_actuators';files=E.SubElement(g,'Files')
 for srcpath in sources:
  assert srcpath.is_file(),srcpath
  f=E.SubElement(files,'File');E.SubElement(f,'FileName').text=srcpath.name;E.SubElement(f,'FileType').text='2' if srcpath.suffix=='.S' else '4' if srcpath.suffix=='.lib' else '1';E.SubElement(f,'FilePath').text=str(srcpath)
 inputs={str(p.relative_to(ROOT)):sha(p) for p in sources}
 inputs.update({(ROOT/'demo/recorder_b2/include/b2_build.h').as_posix():sha(ROOT/'demo/recorder_b2/include/b2_build.h')})
 # Freeze dependency headers from a bootstrap build before the final link.
 if dependencies_from:
  seed=Path(dependencies_from).resolve();assert seed.is_relative_to(allowed)
  for dep in (seed/'Objects').glob('*.d'):
   for token in re.findall(r'[A-Za-z]:[\\/][^\s]+\.(?:h|c|S)\b',dep.read_text(errors='replace')):
    path=Path(token).resolve()
    if path.is_relative_to(seed/'adapted'):path=adapt/path.relative_to(seed/'adapted')
    if path.name=='recorder_identity.h':continue
    assert path.is_file(),path
    key=str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)
    inputs[key]=sha(path)
 inputs[str(ROOT/'demo/recorder_b2/include/b2_policy.h')]=sha(ROOT/'demo/recorder_b2/include/b2_policy.h')
 inputs[str(ROOT/'tools/prepare_recorder_b2.py')]=sha(ROOT/'tools/prepare_recorder_b2.py')
 scatter=Path(target.findtext('.//ScatterFile'));inputs[str(scatter)]=sha(scatter)
 config={'defines':c.findtext('Define'),'compiler_flags':c.findtext('MiscControls'),'assembler':target.findtext('.//Aads/VariousControls/Define')}
 digest=hashlib.sha256(json.dumps({'files':inputs,'config':config},sort_keys=True).encode()).hexdigest();head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
 (adapt/'recorder_identity.h').write_text('#define RECORDER_GIT_SHA "'+head+'"\n#define RECORDER_TREE_SHA256 "'+digest+'"\n#define RECORDER_GIT_DIRTY 1\n')
 project=ROOT/'demo/recorder_b2/mdk/recorder_b2.uvprojx';project.parent.mkdir(exist_ok=True);E.indent(tree,space='  ');tree.write(project,encoding='utf-8',xml_declaration=True)
 (out/'inputs.json').write_text(json.dumps({'sources':inputs,'config':config,'dependency_bootstrap':str(dependencies_from),'manifest_digest':digest,'adapter_origin':str(src),'adapter_sha256':{n:sha(adapt/n) for n in needed},'project_sha256':sha(project),'entry':'wait B2 START; no automatic camera or Session','volume':'2:'},indent=2))
 print(project)
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',required=True);p.add_argument('--dependencies-from');a=p.parse_args();prepare(a.output,a.dependencies_from)
