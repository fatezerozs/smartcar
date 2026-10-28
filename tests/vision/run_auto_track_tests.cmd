@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output\auto-track mkdir output\auto-track
set "speedSource=..\..\demo\code"
set "speedFlags=/O2"
if "%~1"=="asan" set "speedFlags=/Od /Zi /fsanitize=address"
set "reviewFlags="
call :suite
if errorlevel 1 goto fail
set "reviewFlags=/DMIDTERM_VISUAL_REVIEWED=1"
call :suite
if errorlevel 1 goto fail
popd
exit /b 0
:suite
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MIDTERM_TRAINING=1 /DVISION_MIDTERM_AUTONOMOUS=1 /DVISION_MIDTERM_AUTO_TRACK=1 %reviewFlags% /DVISION_MIDTERM_ROUTE_TRIAL=1 /DVISION_ROUTE_SWITCH=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_auto_track.c "%speedSource%\midterm_boot.c" "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\auto-track\ /Fdoutput\auto-track\ /Feoutput\auto-track\test_auto_track.exe
if errorlevel 1 exit /b 1
output\auto-track\test_auto_track.exe
exit /b %errorlevel%
:fail
popd
exit /b 1
