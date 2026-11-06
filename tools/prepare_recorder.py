"""Generate an isolated training MDK target and auditable private SDK adapters.

Does not flash, run Keil hooks, touch an SD card, or rewrite any base SDK file.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from io import BytesIO

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / '.build/recorder-train/adapted'
PIN = ROOT / 'demo/recorder/sdk_sources.lock.json'
SOURCES = {
    'camera.c': 'libraries/zf_device/zf_device_mt9v03x.c',
    'sd.c': 'libraries/components/sdmmc/sd/fsl_sd.c',
    'host.c': 'libraries/components/sdmmc/host/usdhc/non_blocking/fsl_sdmmc_host.c',
    'host.h': 'libraries/components/sdmmc/host/usdhc/fsl_sdmmc_host.h',
    'board.c': 'libraries/components/sdmmc/sdmmc_config.c',
    'usdhc.c': 'libraries/sdk/drives/fsl_usdhc.c',
    'drive.c': 'demo/code/vision_drive.c',
    'board_io.c': 'demo/code/board_io.c',
    **{name: 'libraries/components/fatfs/source/' + name for name in
       ('ff.c', 'ff.h', 'ffconf.h', 'diskio.c', 'diskio.h', 'ffunicode.c')},
}


def write_if_changed(path, data):
    """Preserve mtimes on identical output so MDK can build incrementally."""
    if path.is_file() and path.read_bytes() == data:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return True


def replace(text, old, new, count=1):
    if text.count(old) != count:
        raise ValueError('SDK adaptation context changed: ' + old[:100])
    return text.replace(old, new)


def body(text, signature, replacement):
    start = text.index(signature)
    begin = text.index('{', start)
    depth, at = 1, begin + 1
    while depth:
        if text[at] == '{': depth += 1
        elif text[at] == '}': depth -= 1
        at += 1
    return text[:begin] + '{\n' + replacement + '\n}' + text[at:]


def prepare(reviewed=False):
    kernel = ROOT/'libraries/third_party/FreeRTOS-Kernel'
    kernel_lock = json.loads((kernel/'sources.lock.json').read_text('utf-8'))
    for name, digest in kernel_lock['sha256_lf'].items():
        if hashlib.sha256((kernel/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=digest:
            raise ValueError('Pinned FreeRTOS source changed: '+name)
    expected = json.loads(PIN.read_text('utf-8'))
    if set(expected) != set(SOURCES.values()): raise ValueError('SDK source lock coverage')
    raw = {}
    for key, path in SOURCES.items():
        data = (ROOT / path).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected[path]:
            raise ValueError('Baseline changed; audit before updating lock: ' + path)
        # Preserve original source bytes, including the vendor GBK comments.
        raw[key] = data.decode('latin1').replace('\r\n', '\n')
    generated = {}
    camera = replace(raw['camera.c'], 'mt9v03x_finished_callback, MT9V03X_VSYNC_PIN', 'NULL, MT9V03X_VSYNC_PIN')
    camera = replace(camera, '        csi_add_empty_buffer(&csi_handle, mt9v03x_image1[0]);\n'
        '        csi_add_empty_buffer(&csi_handle, mt9v03x_image2[0]);\n'
        '        csi_start(&csi_handle);', '        recorder_capture_start(&csi_handle);')
    generated['zf_device_mt9v03x.c'] = '#include "recorder_runtime.h"\n' + camera
    generated['vision_drive.c'] = replace(raw['drive.c'], 'VISION_ROUTE_SWITCH?MT_ROUTE_FAST_TICKS:40',
                                         'VISION_ROUTE_SWITCH?40U:40U')
    generated['board_io.c'] = replace(raw['board_io.c'],
        '{gpio_set_level(BOARD_BUZZER_PIN,on?GPIO_HIGH:GPIO_LOW);}',
        '{(void)on;gpio_set_level(BOARD_BUZZER_PIN,GPIO_LOW);}')
    host = '#include "recorder_sd.h"\n' + raw['host.c']
    host = replace(host, '#define SDMMCHOST_TRANSFER_COMPLETE_TIMEOUT (~0U)',
                   '#define SDMMCHOST_TRANSFER_COMPLETE_TIMEOUT (250U)')
    marker = '    (void)SDMMC_OSAMutexLock(&host->lock, osaWaitForever_c);'
    # Transfer and Init return status; Reset/Deinit return void.
    parts = host.split(marker)
    if len(parts) != 5: raise ValueError('Host lock call audit changed')
    host = parts[0] + '    if (recorder_sd_expired() || SDMMC_OSAMutexLock(&host->lock, 0U)!=kStatus_Success) return kStatus_Fail;' + parts[1]
    host += '    if (SDMMC_OSAMutexLock(&host->lock, 0U)!=kStatus_Success) return kStatus_Fail;' + parts[2]
    host += '    if (SDMMC_OSAMutexLock(&host->lock, 0U)!=kStatus_Success) return;' + parts[3]
    host += '    if (SDMMC_OSAMutexLock(&host->lock, 0U)!=kStatus_Success) return;' + parts[4]
    # A transfer error is terminal for this session. Keep all DMA buffers quarantined.
    host = replace(host, '        SDMMCHOST_ErrorRecovery(host->hostController.base);',
                   '        SDMMCHOST_ErrorRecovery(host->hostController.base);\n        recorder_sd_fault();')
    # The SDK assigns capability bits unconditionally: header macros alone
    # do NOT prevent voltage/UHS/wide-bus negotiation at runtime.
    host = replace(host, '    host->maxBlockCount = SDMMCHOST_SUPPORT_MAX_BLOCK_COUNT;', '''
    /* Training wiring shares DATA2/3 with UART8 and VSELECT with servo C30. */
    host->capability &= ~((uint32_t)kSDMMCHOST_SupportHighSpeed |
        (uint32_t)kSDMMCHOST_SupportVoltage1v8 | (uint32_t)kSDMMCHOST_SupportVoltage1v2 |
        (uint32_t)kSDMMCHOST_Support4BitDataWidth | (uint32_t)kSDMMCHOST_Support8BitDataWidth |
        (uint32_t)kSDMMCHOST_SupportDDRMode | (uint32_t)kSDMMCHOST_SupportDetectCardByData3 |
        (uint32_t)kSDMMCHOST_SupportSDR104 | (uint32_t)kSDMMCHOST_SupportSDR50 |
        (uint32_t)kSDMMCHOST_SupportHS200 | (uint32_t)kSDMMCHOST_SupportHS400);
    host->maxBlockCount = SDMMCHOST_SUPPORT_MAX_BLOCK_COUNT;''')
    generated['fsl_sdmmc_host.c'] = host
    host_h = raw['host.h']
    for name in ('HIGH_SPEED', 'VOLTAGE_1V8', 'VOLTAGE_1V2', 'DDR_MODE',
                 '4_BIT_WIDTH','8_BIT_WIDTH','DETECT_CARD_BY_DATA3','SDR104','SDR50','HS200','HS400'):
        host_h, n = re.subn(r'(#define SDMMCHOST_SUPPORT_' + name + r'\s+)\(1U\)', r'\g<1>(0U)', host_h)
        if n != 1: raise ValueError('Host capability: ' + name)
    generated['fsl_sdmmc_host.h'] = host_h
    board = '#include "zf_common_typedef.h"\n#include "recorder_sd.h"\n' + raw['board.c']
    board = replace(board, 'AT_NONCACHEABLE_SECTION_ALIGN(static uint32_t s_sdmmcHostDmaBuffer',
                    'AT_SDRAM_NONCACHE_SECTION_ALIGN(static uint32_t s_sdmmcHostDmaBuffer')
    board = replace(board, 'SDK_ALIGN(static uint8_t s_sdmmcCacheLineAlignBuffer',
                    'AT_SDRAM_NONCACHE_SECTION_ALIGN(static uint8_t s_sdmmcCacheLineAlignBuffer')
    board = body(board, 'uint32_t BOARD_USDHC1ClockConfiguration(void)', '''
    /* Reuse the already-running PFD2. Never restart SYS PLL or any PFD
     * while executing from SDRAM, and never reconfigure this during driving. */
    uint32_t source=CLOCK_GetFreq(kCLOCK_SysPllPfd2Clk);
    uint32_t divider=(source+197999999U)/198000000U;
    if(!divider || divider>8U)return 0;
    CLOCK_SetDiv(kCLOCK_Usdhc1Div,divider-1U);CLOCK_SetMux(kCLOCK_Usdhc1Mux,0U);
    return source/divider;''')
    board = body(board,'void BOARD_SD_Pin_Config(uint32_t freq)',
                 '    recorder_sd_pad_config(freq);')
    board = body(board,'void BOARD_SDCardDAT3PullFunction(uint32_t status)',
                 '    (void)status; /* Dedicated GPIO card-detect; never touch UART8 RX. */')
    generated['sdmmc_config.c'] = board
    sd = '#include "recorder_sd.h"\n#include "FreeRTOS.h"\n#include "task.h"\n' + raw['sd.c']
    sd = body(sd, 'status_t SD_PollingCardInsert(sd_card_t *card, uint32_t status)', '''
    if(recorder_sd_expired() || !card || !card->usrParam.cd ||
       card->usrParam.cd->type!=kSD_DetectCardByGpioCD || !card->usrParam.cd->cardDetected)return kStatus_Fail;
    if(card->usrParam.cd->cardDetected()!=(status==(uint32_t)kSD_Inserted))return kStatus_Fail;
    SDMMC_OSADelay(card->usrParam.cd->cdDebounce_ms);
    return !recorder_sd_expired() && card->usrParam.cd->cardDetected()==(status==(uint32_t)kSD_Inserted)?kStatus_Success:kStatus_Fail;''')
    sd = body(sd, 'status_t SD_PollingCardStatusBusy(sd_card_t *card, uint32_t timeoutMs)', '''
    TickType_t start=xTaskGetTickCount();
    while(!recorder_sd_expired() && (uint32_t)(xTaskGetTickCount()-start)<pdMS_TO_TICKS(timeoutMs)) {
        if(!SDMMCHOST_IsCardBusy(card->host) && SD_SendCardStatus(card)==kStatus_SDMMC_CardStatusIdle)
            return kStatus_SDMMC_CardStatusIdle;
        vTaskDelay(1);
    }
    return kStatus_SDMMC_CardStatusBusy;''')
    # Each bounded retry loop must stop issuing new commands after the outer deadline.
    sd = replace(sd, '        error = SDMMCHOST_TransferFunction(card->host, content);',
                 '        if(recorder_sd_expired())return kStatus_SDMMC_TransferFailed;\n        error = SDMMCHOST_TransferFunction(card->host, content);')
    sd = replace(sd,'    if ((card->flags & (uint32_t)kSD_Support4BitWidthFlag) != 0U)',
        '    if ((card->flags & (uint32_t)kSD_Support4BitWidthFlag) != 0U &&\n'
        '        (card->host->capability & (uint32_t)kSDMMCHOST_Support4BitDataWidth) != 0U)')
    generated['fsl_sd.c'] = sd
    usdhc = '#include "recorder_sd.h"\n#include "FreeRTOS.h"\n#include "task.h"\n' + raw['usdhc.c']
    usdhc = replace(usdhc, '    while (!IS_USDHC_FLAG_SET(base->PRES_STATE, USDHC_PRES_STATE_SDSTB_MASK))\n    {\n    }',
        '    while (!IS_USDHC_FLAG_SET(base->PRES_STATE, USDHC_PRES_STATE_SDSTB_MASK))\n    {\n        if(recorder_sd_expired())return 0;\n        vTaskDelay(1);\n    }')
    generated['fsl_usdhc.c'] = usdhc
    for name in ('ff.c', 'ff.h', 'ffconf.h', 'diskio.c', 'diskio.h', 'ffunicode.c'):
        generated[name] = raw[name]
    generated['ffconf.h'] = replace(generated['ffconf.h'], '#define FF_USE_LFN\t\t0', '#define FF_USE_LFN\t\t2')
    generated['ffconf.h'] = replace(generated['ffconf.h'], '#define FF_CODE_PAGE\t932', '#define FF_CODE_PAGE\t437')
    OUT.mkdir(parents=True, exist_ok=True)
    for name, content in generated.items():
        write_if_changed(OUT / name, content.encode('latin1'))
    # Content identity is explicit and reproducible; Git HEAD alone does not
    # identify this dirty training worktree. Binary hash is recorded after link.
    inputs = {}
    for folder in ('demo', 'libraries'):
        for path in sorted((ROOT / folder).rglob('*')):
            if path.is_file() and path.suffix.lower() in ('.c','.h','.s','.scf','.lib'):
                inputs[path.relative_to(ROOT).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    inputs |= {'generated/'+name: hashlib.sha256((OUT/name).read_bytes()).hexdigest() for name in generated}
    inputs['tools/prepare_recorder.py'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    inputs['demo/mdk/base/rt1064.uvprojx'] = hashlib.sha256((ROOT/'demo/mdk/base/rt1064.uvprojx').read_bytes()).hexdigest()
    inputs['motion_reviewed'] = str(bool(reviewed))
    digest = hashlib.sha256(json.dumps(inputs, sort_keys=True, separators=(',',':')).encode()).hexdigest()
    head = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True,timeout=15).strip()
    dirty = bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT,text=True,timeout=15).strip())
    identity = ('#define RECORDER_GIT_SHA "'+head+'"\n'
        '#define RECORDER_TREE_SHA256 "'+digest+'"\n#define RECORDER_GIT_DIRTY '+str(int(dirty))+'\n')
    write_if_changed(OUT/'recorder_identity.h', identity.encode('ascii'))
    report = {'baseline_sha256': expected, 'generated_sha256': {
        name: hashlib.sha256((OUT / name).read_bytes()).hexdigest() for name in generated},
        'firmware_inputs':inputs,'firmware_input_tree_sha256':digest,'git_head':head,'git_dirty':dirty,
        'motion_reviewed':reviewed,'requested_speed_max_ticks_per_10ms':40}
    write_if_changed(OUT / 'provenance.json', (json.dumps(report, indent=2)+'\n').encode('utf-8'))
    project = ROOT / 'demo/mdk/training/recorder_train.uvprojx'
    project.parent.mkdir(parents=True, exist_ok=True)
    tree = ET.parse(ROOT / 'demo/mdk/base/rt1064.uvprojx')
    target = tree.find('.//Target')
    common = target.find('TargetOption/TargetCommonOption')
    common.find('OutputDirectory').text = '..\\..\\..\\.build\\recorder-train\\Objects\\'
    common.find('OutputName').text = 'recorder_train'
    common.find('ListingPath').text = '..\\..\\..\\.build\\recorder-train\\Listings\\'
    for node in common.iter():
        if node.tag.startswith('RunUserProg'): node.text = '0'
    controls = target.find('TargetOption/TargetArmAds/Cads/VariousControls')
    flags = ('RECORDER_TRAINING=1,VISION_MIDTERM_TRAINING=1,VISION_ENCODER_ENABLE=1,'
        'VISION_MIDTERM_AUTONOMOUS=1,VISION_MIDTERM_ROUTE_TRIAL=1,VISION_ROUTE_SWITCH=1,'
        'VISION_START_SWITCH_ENABLE=0,MIDTERM_VISUAL_REVIEWED='+str(int(reviewed))+',')
    controls.find('Define').text = flags + controls.find('Define').text
    controls.find('IncludePath').text = ('..\\..\\recorder\\include;..\\..\\..\\.build\\recorder-train\\adapted;'
        '..\\..\\..\\libraries\\third_party\\FreeRTOS-Kernel\\include;'
        '..\\..\\..\\libraries\\third_party\\FreeRTOS-Kernel\\portable\\GCC\\ARM_CM7\\r0p1;') + controls.find('IncludePath').text
    controls.find('MiscControls').text += ' -include ../../recorder/include/recorder_build.h'
    replacements = {name: '..\\..\\..\\.build\\recorder-train\\adapted\\'+name for name in generated if name.endswith('.c')}
    replacements |= {'main.c': '..\\..\\recorder\\recorder_main.c', 'isr.c': '..\\..\\recorder\\recorder_isr.c',
                     'zf_common_vector.c': '..\\..\\recorder\\recorder_vector.c',
                     'fsl_sd_disk.c': '..\\..\\recorder\\recorder_disk.c',
                     'fsl_sdmmc_osa.c': '..\\..\\recorder\\recorder_sd_osa.c'}
    seen = set()
    for file in target.findall('.//Group/Files/File'):
        name = file.findtext('FileName')
        if name in replacements:
            file.find('FilePath').text = replacements[name];seen.add(name)
    if set(replacements) - seen: raise ValueError('Missing base project source: '+repr(set(replacements)-seen))
    group = ET.SubElement(target.find('Groups'), 'Group')
    ET.SubElement(group, 'GroupName').text = 'recorder_training_only'
    files = ET.SubElement(group, 'Files')
    sources = sorted((ROOT / 'demo/recorder').glob('*.c'))
    sources = [p for p in sources if p.name not in ('recorder_main.c','recorder_isr.c','recorder_vector.c','recorder_disk.c','recorder_sd_osa.c')]
    sources += [ROOT / 'libraries/third_party/FreeRTOS-Kernel' / n for n in
                ('tasks.c','queue.c','list.c','portable/GCC/ARM_CM7/r0p1/port.c')]
    import os
    for source in sources:
        file = ET.SubElement(files, 'File')
        ET.SubElement(file,'FileName').text = source.name
        ET.SubElement(file,'FileType').text = '1'
        ET.SubElement(file,'FilePath').text = os.path.relpath(source,project.parent).replace('/', '\\')
    ET.indent(tree, space='  ')
    serialized = BytesIO()
    tree.write(serialized, encoding='utf-8', xml_declaration=True)
    write_if_changed(project, serialized.getvalue())
    return str(project)


def check_map(path):
    text=Path(path).read_text(encoding='utf-8',errors='replace')
    buffers={}
    for name,alignment in (('g_sd',64),('bounce',64),('raw',64),('recording',64),('s_sdmmcHostDmaBuffer',4)):
        matches=re.findall(r'^\s{4}'+name+r'\s+0x([0-9a-fA-F]+)\s+Data\s+(\d+)\s+',text,re.M)
        if len(matches)!=1: raise ValueError('Missing/ambiguous map object: '+name)
        address,size=int(matches[0][0],16),int(matches[0][1])
        if address%alignment or address<0x81e00000 or address+size>0x82000000:
            raise ValueError('Noncache/size/alignment violation: '+name)
        buffers[name]={'address':hex(address),'bytes':size,'required_alignment':alignment}
    if buffers['raw']['bytes']!=8*22592 or buffers['recording']['bytes']!=64*22656:
        raise ValueError('Capture/recorder static capacity changed')
    for handler in ('SVC_Handler','PendSV_Handler','SysTick_Handler'):
        if not re.search(r'^\s{4}'+handler+r'\s+0x[0-9a-fA-F]+\s+Thumb Code\s+\d+\s+port\.o\(',text,re.M):
            raise ValueError('Kernel vector not bound to port: '+handler)
    regions={}
    for name in ('SDRAM_ncache','RW_m_data'):
        m=re.search(r'Execution Region '+name+r' \(.*?Size: 0x([0-9a-fA-F]+), Max: 0x([0-9a-fA-F]+)',text)
        if not m: raise ValueError('Missing memory region: '+name)
        used,maximum=int(m[1],16),int(m[2],16)
        if used>maximum: raise ValueError('Memory region overflow: '+name)
        regions[name]={'used':used,'maximum':maximum,'remaining':maximum-used}
    return {'buffers':buffers,'regions':regions,'kernel_vectors':'verified_from_linker_map',
            'hardware_dma_reachability':'NOT_MEASURED'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--motion-reviewed', action='store_true', help='Enable the existing visual review gate ONLY after the documented board acceptance')
    parser.add_argument('--check-map',type=Path,help='Validate a completed recorder linker map without regenerating sources')
    args=parser.parse_args()
    print(json.dumps(check_map(args.check_map),indent=2) if args.check_map else prepare(args.motion_reviewed))
