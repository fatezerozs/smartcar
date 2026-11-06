"""Check exact MAP object allowlist, memory placement, vectors and preservation."""
from pathlib import Path
import sys,re,json,hashlib,struct,subprocess
here=Path(__file__).resolve().parent
root=here.parent.parent
out=Path(sys.argv[1]).resolve()
assert out.is_relative_to(here/'artifacts')
mp=out/'Listings/sd_diagnostic.map'
s=mp.read_text(errors='replace')
table=s.split('Object Name')[1].split('Object Totals')[0]
objects=re.findall(r'^\s+(?:\d+\s+){6}(\S+\.o)\s*$',table,re.M)
expected={'board.o','diag_core.o','diag_system.o','evkmimxrt1064_flexspi_nor_config.o','ff.o','fsl_cache.o','fsl_clock.o','fsl_flexspi_nor_boot.o','fsl_gpio.o','fsl_lpuart.o','fsl_sdmmc_common.o','fsl_usdhc.o','host_policy.o','osa.o','sd_policy.o','startup_mimxrt1064.o'}
assert set(objects)==expected,(objects,expected-set(objects))
for forbidden in ['vision_','recorder_','midterm_','board_camera','board_encoder','zf_driver_pwm','zf_driver_csi','vTaskStartScheduler']:
 assert forbidden not in s,forbidden
for forbidden in ['f_mkfs','f_unlink']:
 assert not re.search(r'^\s+'+forbidden+r'\s+0x[0-9a-f]+\s+Thumb Code',s,re.M),forbidden
buffers={}
for name in ['bounce','card','host','descriptors']:
 m=re.search(r'^\s+'+name+r'\s+0x([0-9a-fA-F]+)\s+Data\s+(\d+)\s+board.o',s,re.M);assert m,name
 addr,size=int(m[1],16),int(m[2]);assert addr%64==0 and 0x20200000<=addr and addr+size<=0x20270000
 buffers[name]={'address':hex(addr),'size':size,'alignment':64}
binpath=out/'Objects/sd_diagnostic.bin';data=binpath.read_bytes();sp,reset=struct.unpack_from('<II',data,0x2000)
assert sp==0x20280000 and 0x70002400<=reset<0x703fe000 and reset&1
assert len(data)<0x3fe000
# IRQ vector trampoline must bind to real SD driver; SysTick to board implementation.
assert re.search(r'startup_mimxrt1064.o\(\.text\) refers to fsl_usdhc.o\(\.text.USDHC1_DriverIRQHandler\)',s)
assert re.search(r'SysTick_Handler\s+0x[0-9a-f]+\s+Thumb Code\s+\d+\s+board.o',s)
baseline=json.loads((here/'evidence/baseline-sha256.json').read_text())
changed=[f for f,h in baseline.items() if not (root/f).is_file() or hashlib.sha256((root/f).read_bytes()).hexdigest()!=h]
assert not changed,changed
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
inputs={}
import xml.etree.ElementTree as E
project=here/'mdk/sd_diagnostic.uvprojx'
for f in E.parse(project).findall('.//FilePath'):
 path=(project.parent/f.text.replace('\\','/')).resolve();inputs[str(path.relative_to(root))]=sha(path)
# Transitive SDK inputs are selected from compiler dependency files, not directory scans.
for dep in (out/'Objects').glob('*.d'):
 for line in dep.read_text(errors='replace').splitlines():
  for raw in re.findall(r'(?:\.\.[/\\])[^\s]+\.(?:h|c|S)',line):
   path=(project.parent/raw.replace('\\','/')).resolve()
   if path.is_file() and path.is_relative_to(root):inputs[str(path.relative_to(root))]=sha(path)
for path in [root/'libraries/components/sdmmc/sd/fsl_sd.c',root/'libraries/components/sdmmc/host/usdhc/non_blocking/fsl_sdmmc_host.c',here/'include/ffconf.h',here/'mdk/sd_diagnostic.scf',project]:inputs[str(path.relative_to(root))]=sha(path)
record={'passed':True,'objects':objects,'dma_buffers':buffers,'initial_sp':hex(sp),'reset_vector':hex(reset),'binary_bytes':len(data),'existing_files_checked':len(baseline),'existing_files_changed':changed,'git_head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'sha256':{str(p.relative_to(here)):sha(p) for p in [mp,binpath,out/'Objects/sd_diagnostic.axf',out/'keil-build.log']},'inputs_sha256':inputs,'flashed':False,'serial_opened':False,'hardware_validated':False}
(out/'verification.json').write_text(json.dumps(record,indent=2),encoding='utf-8')
(here/'evidence/latest-build.txt').write_text(str(out),encoding='utf-8')
print(json.dumps({k:v for k,v in record.items() if k not in ['inputs_sha256','sha256']},indent=2))
