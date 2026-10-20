@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output\speed mkdir output\speed
set "speedSource=..\..\demo\code"
set "speedFlags=/O2"
if "%~1"=="asan" set "speedFlags=/Od /Zi /fsanitize=address"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MOTION_DIAG=1 /DVISION_MOTOR_THRESHOLD_DIAG=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_speed.c "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\speed\ /Fdoutput\speed\ /Feoutput\speed\test_speed.exe
if errorlevel 1 goto fail
output\speed\test_speed.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /I"stubs\encoder_board" /I"%speedSource%" test_encoder_board.c "%speedSource%\board_encoder.c" /Fooutput\speed\ /Fdoutput\speed\ /Feoutput\speed\test_encoder_board.exe
if errorlevel 1 goto fail
output\speed\test_encoder_board.exe
if errorlevel 1 goto fail
popd
exit /b 0
:fail
popd
exit /b 1
