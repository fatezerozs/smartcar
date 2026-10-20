@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output\midterm mkdir output\midterm
set "speedSource=..\..\demo\code"
set "speedFlags=/O2"
if "%~1"=="asan" set "speedFlags=/Od /Zi /fsanitize=address"
set "review=1"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MIDTERM_AUTONOMOUS=1 /DMIDTERM_SPEED_CONFIRMED=1 /DMIDTERM_ENCODER_LEFT_SIGN=1 /DMIDTERM_ENCODER_RIGHT_SIGN=1 /DMIDTERM_SPEED_MAX_TICKS=10 /DMIDTERM_SPEED_KP=5.0f /DMIDTERM_SPEED_KI=0.05f /DMIDTERM_VISUAL_REVIEWED=%review% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_midterm_boot.c "%speedSource%\midterm_boot.c" "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\midterm\ /Fdoutput\midterm\ /Feoutput\midterm\test_midterm_boot.exe
if not "%errorlevel%"=="0" goto fail
output\midterm\test_midterm_boot.exe
if not "%errorlevel%"=="0" goto fail
set "review=0"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MIDTERM_AUTONOMOUS=1 /DMIDTERM_SPEED_CONFIRMED=1 /DMIDTERM_ENCODER_LEFT_SIGN=1 /DMIDTERM_ENCODER_RIGHT_SIGN=1 /DMIDTERM_SPEED_MAX_TICKS=10 /DMIDTERM_SPEED_KP=5.0f /DMIDTERM_SPEED_KI=0.05f /DMIDTERM_VISUAL_REVIEWED=%review% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_midterm_boot.c "%speedSource%\midterm_boot.c" "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\midterm\ /Fdoutput\midterm\ /Feoutput\midterm\test_midterm_boot.exe
if not "%errorlevel%"=="0" goto fail
output\midterm\test_midterm_boot.exe
if not "%errorlevel%"=="0" goto fail
set "review=1"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MIDTERM_AUTONOMOUS=1 /DMIDTERM_SPEED_CONFIRMED=0 /DMIDTERM_ENCODER_LEFT_SIGN=1 /DMIDTERM_ENCODER_RIGHT_SIGN=1 /DMIDTERM_SPEED_MAX_TICKS=10 /DMIDTERM_SPEED_KP=5.0f /DMIDTERM_SPEED_KI=0.05f /DMIDTERM_VISUAL_REVIEWED=%review% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_midterm_boot.c "%speedSource%\midterm_boot.c" "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\midterm\ /Fdoutput\midterm\ /Feoutput\midterm\test_midterm_boot.exe
if not "%errorlevel%"=="0" goto fail
output\midterm\test_midterm_boot.exe
if not "%errorlevel%"=="0" goto fail
set "dualSource=..\..\demo\code"
set "dualProfile=/DVISION_MIDTERM_AUTONOMOUS=1 /DVISION_ENCODER_ENABLE=1 /DMIDTERM_SPEED_CONFIRMED=1 /DMIDTERM_ENCODER_LEFT_SIGN=1 /DMIDTERM_ENCODER_RIGHT_SIGN=1 /DMIDTERM_SPEED_MAX_TICKS=10 /DMIDTERM_SPEED_KP=5.0f /DMIDTERM_SPEED_KI=0.05f /DMIDTERM_VISUAL_REVIEWED=1 /DVISION_DUAL_UART_ENABLE=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /DVISION_DRIVE_PROFILE_CONFIRMED=1 /DVISION_MOTOR_DIRECTION_CONFIRMED=1 /DVISION_CAMERA_CALIBRATED=1 /DVISION_SERVO_CENTER_US=1500 /DVISION_SERVO_MIN_US=1200 /DVISION_SERVO_MAX_US=1800 /DVISION_SERVO_RIGHT_SIGN=1 /DVISION_MOTOR_DUTY_CAP=0.15f /DVISION_MOTOR_CRUISE_DUTY=0.1f"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% %dualProfile% /I"stubs" /I"%dualSource%" test_midterm_runtime.c "%dualSource%\midterm_boot.c" "%dualSource%\vision_speed.c" "%dualSource%\wheel_speed.c" "%dualSource%\app_link.c" "%dualSource%\app_transport.c" "%dualSource%\app_runtime.c" "%dualSource%\vision_debug.c" "%dualSource%\vision_frame.c" "%dualSource%\vision_telemetry.c" "%dualSource%\vision_midterm.c" "%dualSource%\track_bayes.c" "%dualSource%\vision_control.c" "%dualSource%\midterm_open_loop.c" "%dualSource%\vision_drive.c" "%dualSource%\vision_motion.c" "%dualSource%\vision_actuator.c" "%dualSource%\vision_guard.c" "%dualSource%\vision_probe.c" "%dualSource%\vision_startup.c" "%dualSource%\vision_vehicle_profile.c" "%dualSource%\app_mode.c" "%dualSource%\vision_field.c" "%dualSource%\vision_storage.c" /Fooutput\midterm\ /Fdoutput\midterm\ /Feoutput\midterm\test_midterm_runtime.exe
if not "%errorlevel%"=="0" goto fail
output\midterm\test_midterm_runtime.exe
if not "%errorlevel%"=="0" goto fail
popd
exit /b 0
:fail
popd
exit /b 1
