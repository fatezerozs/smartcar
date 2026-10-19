$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$mdkRoot = Join-Path $projectRoot 'demo\mdk'
[xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $mdkRoot 'rt1064.uvprojx')
$targets = @($xml.Project.Targets.Target | Where-Object TargetName -eq 'nor_sdram_zf_dtcm')
if ($targets.Count -ne 1) { throw 'Missing/duplicate MDK target' }
$files = @($targets[0].Groups.Group.Files.File)
foreach ($file in $files) {
    $resolved = [IO.Path]::GetFullPath((Join-Path $mdkRoot ([string]$file.FilePath)))
    if (-not $resolved.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw ('Source escapes project: ' + $file.FilePath) }
    if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) { throw ('Missing project input: ' + $file.FilePath) }
}
foreach ($name in @('vision_storage.c','vision_storage.h','vision_storage_board.c','vision_field.c','vision_field.h','track_bayes.c','track_bayes.h','midterm_open_loop.c','midterm_open_loop.h','vision_midterm.c','vision_midterm.h','vision_debug.c','vision_debug.h','vision_control.c','vision_control.h','vision_drive.c','vision_drive.h','vision_profile.h')) {
    if (@($files | Where-Object FileName -eq $name).Count -ne 1) { throw ('Bad file reference: ' + $name) }
}
$main = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'demo\user\src\main.c')
if ($main -notmatch 'vision_debug_run\(\)' -or $main -match 'Island_Detect\(|zebra_handle\(|encoder_quad_init\(') { throw 'Old runtime entry detected' }
foreach ($name in @('Camera.c','Island.c','pid.c','img_process.c')) {
    if (@($files | Where-Object FileName -eq $name).Count) { throw ('Legacy algorithm is still built: ' + $name) }
}
if ($main -match 'DIAG_UART_ONLY|\[UART_DIAG\]') { throw 'Temporary diagnostic firmware must not be delivered' }
$profile = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'demo\code\vision_profile.h')
if ($profile -notmatch '(?m)^#define\s+VISION_DRIVE_ENABLE\s+1\s*$') { throw 'Field control runtime must be compiled in' }
foreach ($name in @('VISION_DRIVE_PROFILE_CONFIRMED','VISION_MOTOR_DIRECTION_CONFIRMED','VISION_CAMERA_CALIBRATED','VISION_SERVO_CENTER_US','VISION_SERVO_MIN_US','VISION_SERVO_MAX_US','VISION_SERVO_RIGHT_SIGN')) {
    if ($profile -notmatch ('(?m)^#define\s+' + $name + '\s+0\s*$')) { throw ('Actuator profile is not locked: ' + $name) }
}
foreach ($name in @('VISION_MOTOR_DUTY_CAP','VISION_MOTOR_CRUISE_DUTY')) {
    if ($profile -notmatch ('(?m)^#define\s+' + $name + '\s+0\.0f\s*$')) { throw ('Nonzero motor configuration: ' + $name) }
}
$defines = [string]$targets[0].TargetOption.TargetArmAds.Cads.VariousControls.Define
if ($defines -match '\bTEST_(VISION|SERVO|MOTOR|UART)|\bVISION_(DRIVE|CAMERA|SERVO|MOTOR)_|\bVISION_START_SWITCH_ENABLE') { throw 'Target overrides require manual review' }
$isr = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'demo\user\src\isr.c')
if ($isr -notmatch '(?s)PIT_CH1.*?#if VISION_DRIVE_ENABLE\s+vision_drive_switch_sample\([^;]+;\s+vision_drive_safety_tick\(\)') { throw 'Missing gated PIT1 safety hook' }
Write-Output ('PASS: ' + $files.Count + ' MDK references exist; correct entry; outputs locked; PIT1 safety hook present.')
$scatter=Get-Content -Raw -LiteralPath (Join-Path $mdkRoot 'scf\MIMXRT1064xxxxx_flexspi_nor.scf')
if($scatter -notmatch '(?m)^#define\s+m_text_size\s+0x003FBC00\s*$'){throw 'Persistent slots not reserved by the load region'}
if($isr -notmatch 'vision_drive_switch_sample\(gpio_get_level\(VISION_START_SWITCH_PIN\)'){throw 'Missing PIT1 physical switch stop'}
$mainHeader=Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'demo\user\inc\main.h')
if($mainHeader -notmatch '(?m)^#define VISION_START_SWITCH_PIN C12\b'){throw 'Master switch is not C12'}
if($profile -notmatch '(?m)^#define VISION_START_SWITCH_ENABLE 1U\b'){throw 'C12 master switch is not enabled'}
