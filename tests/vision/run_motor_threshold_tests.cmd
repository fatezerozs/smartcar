@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output\motor-threshold mkdir output\motor-threshold
set "thresholdSource=..\..\demo\code"
set "thresholdFlags=/O2"
if "%~1"=="asan" set "thresholdFlags=/Od /Zi /fsanitize=address"
for %%D in (0 1) do (
cl /nologo /utf-8 /std:c11 /W4 /WX %thresholdFlags% /DVISION_MOTOR_THRESHOLD_DIAG=%%D /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%thresholdSource%" test_motor_threshold.c "%thresholdSource%\vision_motion.c" "%thresholdSource%\vision_drive.c" "%thresholdSource%\vision_actuator.c" "%thresholdSource%\vision_guard.c" "%thresholdSource%\vision_probe.c" "%thresholdSource%\vision_startup.c" "%thresholdSource%\vision_vehicle_profile.c" "%thresholdSource%\vision_control.c" "%thresholdSource%\midterm_open_loop.c" "%thresholdSource%\app_mode.c" "%thresholdSource%\vision_field.c" "%thresholdSource%\vision_storage.c" /Fooutput\motor-threshold\ /Fdoutput\motor-threshold\ /Feoutput\motor-threshold\test_threshold_%%D.exe
if errorlevel 1 goto fail
output\motor-threshold\test_threshold_%%D.exe
if errorlevel 1 goto fail
)
popd
exit /b 0
:fail
popd
exit /b 1
