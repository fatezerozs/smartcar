param([string]$ArmBin = 'C:\Keil_v5\ARM\ARMCLANG\bin')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$mdkRoot = Join-Path $projectRoot 'demo\mdk\base'
$outputRoot = Join-Path $projectRoot '.build\arm-check'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$compiler = Join-Path $ArmBin 'armclang.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'ARMCLANG not found; supply -ArmBin for your installation.' }
[xml]$xml = Get-Content -Raw -LiteralPath (Join-Path $mdkRoot 'rt1064.uvprojx')
$target = @($xml.Project.Targets.Target | Where-Object TargetName -eq 'nor_sdram_zf_dtcm')
if ($target.Count -ne 1) { throw 'Expected one MDK target' }
$controls = $target[0].TargetOption.TargetArmAds.Cads.VariousControls
$flags = @('--target=arm-arm-none-eabi', '-mcpu=cortex-m7', '-mfpu=fpv5-d16', '-mfloat-abi=hard', '-mlittle-endian', '-funsigned-char', '-fshort-enums', '-fshort-wchar', '-std=c99', '-Oz', '-g', '-ffunction-sections', '-Werror', '-Wno-nonportable-include-path', '-D__MICROLIB', '-D__UVISION_VERSION=539')
$flags += @(([string]$controls.MiscControls).Split(' ', [StringSplitOptions]::RemoveEmptyEntries))
foreach ($definition in ([string]$controls.Define).Split(',')) { $flags += '-D' + ($definition.Trim() -replace '\s*=\s*', '=') }
foreach ($include in ([string]$controls.IncludePath).Split(';')) { if ($include.Trim()) { $flags += '-I' + $include.Trim() } }
Push-Location $mdkRoot
try {
    foreach ($unit in @('midterm_boot','app_mode','app_runtime','vision_drive','vision_actuator')) {
        & $compiler @flags '-DVISION_ENCODER_ENABLE=1' '-DVISION_MIDTERM_AUTONOMOUS=1' '-c' ('..\..\code\'+$unit+'.c') '-o' (Join-Path $outputRoot ($unit+'-auto.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM autonomous branch failed: '+$unit) }
    }
    foreach ($unit in @('wheel_speed','vision_speed','board_encoder')) {
        & $compiler @flags '-c' ('..\..\code\'+$unit+'.c') '-o' (Join-Path $outputRoot ($unit+'.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM speed module failed: '+$unit) }
    }
    foreach ($unit in @('vision_speed','vision_motion','vision_drive','vision_actuator','vision_guard','vision_vehicle_profile','vision_field','vision_telemetry','app_runtime')) {
        & $compiler @flags '-DVISION_ENCODER_ENABLE=1' '-DVISION_MOTION_DIAG=1' '-DVISION_MOTOR_THRESHOLD_DIAG=1' '-c' ('..\..\code\'+$unit+'.c') '-o' (Join-Path $outputRoot ($unit+'-speed.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM closed-loop branch failed: '+$unit) }
    }
    foreach ($unit in @('vision_motion','app_link','app_link_board','board_camera','app_transport','vision_frame','vision_telemetry','app_runtime','app_mode','board_io','vision_actuator','vision_guard','vision_probe','vision_startup','vision_vehicle_profile','vision_storage', 'vision_storage_board', 'vision_field', 'track_bayes', 'midterm_open_loop', 'vision_midterm', 'vision_debug', 'vision_control', 'vision_drive')) {
        & $compiler @flags '-c' ('..\..\code\' + $unit + '.c') '-o' (Join-Path $outputRoot ($unit + '.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM compile failed: ' + $unit) }
        Write-Output ('PASS ARM default: ' + $unit)
    }
    foreach ($unit in @('vision_debug', 'vision_drive')) {
        & $compiler @flags '-DVISION_DRIVE_ENABLE=1' '-c' ('..\..\code\' + $unit + '.c') '-o' (Join-Path $outputRoot ($unit + '-enabled.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM enabled branch failed: ' + $unit) }
    }
    foreach ($unit in @('main', 'isr')) {
        & $compiler @flags '-c' ('..\..\user\src\' + $unit + '.c') '-o' (Join-Path $outputRoot ($unit + '.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM user unit failed: ' + $unit) }
    }
    foreach ($source in @('..\..\code\app_transport.c','..\..\code\vision_telemetry.c','..\..\code\app_runtime.c', '..\..\code\vision_debug.c', '..\..\user\src\main.c')) {
        & $compiler @flags '-DVISION_WIRELESS_UART_ENABLE=1' '-DVISION_DRIVE_ENABLE=1' '-c' $source '-o' (Join-Path $outputRoot ((Split-Path $source -Leaf) + '-wireless.o'))
        if ($LASTEXITCODE -ne 0) { throw ('ARM wireless branch failed: ' + $source) }
    }
    & $compiler @flags '-DVISION_DRIVE_ENABLE=1' '-c' '..\..\user\src\isr.c' '-o' (Join-Path $outputRoot 'isr-enabled.o')
    if ($LASTEXITCODE -ne 0) { throw 'ARM enabled ISR failed' }
    # Synthetic profile exists only in these compiler arguments, never deployed.
    & $compiler @flags '-DVISION_DRIVE_ENABLE=1' '-DVISION_DRIVE_PROFILE_CONFIRMED=1' '-DVISION_CAMERA_CALIBRATED=1' '-DVISION_SERVO_CENTER_US=1500' '-DVISION_SERVO_MIN_US=1200' '-DVISION_SERVO_MAX_US=1800' '-DVISION_SERVO_RIGHT_SIGN=1' '-DVISION_MOTOR_DUTY_CAP=0.2f' '-DVISION_MOTOR_CRUISE_DUTY=0.1f' '-c' '..\..\code\vision_drive.c' '-o' (Join-Path $outputRoot 'vision_drive-test-profile.o')
    if ($LASTEXITCODE -ne 0) { throw 'ARM synthetic profile failed' }
    Write-Output 'ARM SOURCE CHECK PASSED; this does not link or flash firmware.'
} finally { Pop-Location }
