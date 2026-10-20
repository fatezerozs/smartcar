@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output\link mkdir output\link
set "linkFlags=/O2"
if "%~1"=="asan" set "linkFlags=/Od /Zi /fsanitize=address"
cl /nologo /utf-8 /std:c11 /W4 /WX %linkFlags% /I"..\..\demo\code" test_link.c "..\..\demo\code\app_link.c" /Fooutput\link\ /Fdoutput\link\ /Feoutput\link\test_link.exe
if errorlevel 1 goto fail
output\link\test_link.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %linkFlags% /I"stubs\link_board" /I"..\..\demo\code" test_link_board.c "..\..\demo\code\app_link.c" "..\..\demo\code\app_link_board.c" /Fooutput\link\ /Fdoutput\link\ /Feoutput\link\test_link_board.exe
if errorlevel 1 goto fail
output\link\test_link_board.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %linkFlags% /DVISION_DUAL_UART_ENABLE=1 /I"stubs" /I"..\..\demo\code" test_transport.c "..\..\demo\code\app_transport.c" "..\..\demo\code\app_link.c" /Fooutput\link\ /Fdoutput\link\ /Feoutput\link\test_transport.exe
if errorlevel 1 goto fail
output\link\test_transport.exe
if errorlevel 1 goto fail
set "dualSource=..\..\demo\code"
set "dualProfile=/DVISION_DUAL_UART_ENABLE=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /DVISION_DRIVE_PROFILE_CONFIRMED=1 /DVISION_MOTOR_DIRECTION_CONFIRMED=1 /DVISION_CAMERA_CALIBRATED=1 /DVISION_SERVO_CENTER_US=1500 /DVISION_SERVO_MIN_US=1200 /DVISION_SERVO_MAX_US=1800 /DVISION_SERVO_RIGHT_SIGN=1 /DVISION_MOTOR_DUTY_CAP=0.2f /DVISION_MOTOR_CRUISE_DUTY=0.1f"
cl /nologo /utf-8 /std:c11 /W4 /WX %linkFlags% %dualProfile% /I"stubs" /I"%dualSource%" test_dual_runtime.c "%dualSource%\app_link.c" "%dualSource%\app_transport.c" "%dualSource%\app_runtime.c" "%dualSource%\vision_debug.c" "%dualSource%\vision_frame.c" "%dualSource%\vision_telemetry.c" "%dualSource%\vision_midterm.c" "%dualSource%\track_bayes.c" "%dualSource%\vision_control.c" "%dualSource%\midterm_open_loop.c" "%dualSource%\vision_drive.c" "%dualSource%\vision_motion.c" "%dualSource%\vision_actuator.c" "%dualSource%\vision_guard.c" "%dualSource%\vision_probe.c" "%dualSource%\vision_startup.c" "%dualSource%\vision_vehicle_profile.c" "%dualSource%\app_mode.c" "%dualSource%\vision_field.c" "%dualSource%\vision_storage.c" /Fooutput\link\ /Fdoutput\link\ /Feoutput\link\test_dual_runtime.exe
if errorlevel 1 goto fail
output\link\test_dual_runtime.exe
if errorlevel 1 goto fail
popd
exit /b 0
:fail
popd
exit /b 1
