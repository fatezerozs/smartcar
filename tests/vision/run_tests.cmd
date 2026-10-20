@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output mkdir output
set "visionSource=%~dp0..\..\demo\code"
set "visionAdapter="%visionSource%\vision_motion.c" "%visionSource%\vision_actuator.c" "%visionSource%\vision_guard.c" "%visionSource%\vision_probe.c" "%visionSource%\vision_startup.c" "%visionSource%\vision_vehicle_profile.c" "%visionSource%\app_mode.c""
set "visionStorage=%visionSource%\vision_storage.c"
set "visionFlags=/O2 /DVISION_DUAL_UART_ENABLE=0"
if "%~1"=="asan" set "visionFlags=/Od /Zi /Fdoutput\ /fsanitize=address /DVISION_DUAL_UART_ENABLE=0"
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" test_vision.c "%visionSource%\vision_midterm.c" /Fooutput\ /Feoutput\test_vision.exe
if errorlevel 1 goto fail
pushd output
test_vision.exe
set "visionResult=%errorlevel%"
popd
if not "%visionResult%"=="0" goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"stubs" /I"%visionSource%"  /DVISION_DRIVE_ENABLE=0 "%visionSource%\app_mode.c" test_debug.c "%visionSource%\vision_debug.c" "%visionSource%\app_runtime.c" "%visionSource%\app_transport.c" "%visionSource%\vision_frame.c" "%visionSource%\vision_telemetry.c" "%visionSource%\vision_midterm.c" "%visionSource%\track_bayes.c" /Fooutput\ /Feoutput\test_debug.exe
if errorlevel 1 goto fail
pushd output
test_debug.exe
set "visionResult=%errorlevel%"
popd
if not "%visionResult%"=="0" goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" test_control.c "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" /Fooutput\ /Feoutput\test_control.exe
if errorlevel 1 goto fail
output\test_control.exe
if errorlevel 1 goto fail
set "visionDriveDefines=/DVISION_DRIVE_ENABLE=0 /DVISION_START_SWITCH_ENABLE=0"
call :drive test_drive
if errorlevel 1 goto fail
set "visionDriveDefines=/DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0"
call :drive test_drive_unconfigured
if errorlevel 1 goto fail
set "visionDriveDefines=/DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /DVISION_DRIVE_PROFILE_CONFIRMED=1 /DVISION_MOTOR_DIRECTION_CONFIRMED=1 /DVISION_CAMERA_CALIBRATED=1 /DVISION_SERVO_CENTER_US=1500 /DVISION_SERVO_MIN_US=1200 /DVISION_SERVO_MAX_US=1800 /DVISION_SERVO_RIGHT_SIGN=1 /DVISION_MOTOR_DUTY_CAP=0.2f /DVISION_MOTOR_CRUISE_DUTY=0.1f"
call :drive test_drive_enabled
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% %visionDriveDefines% /I"stubs" /I"%visionSource%" test_debug.c "%visionSource%\vision_debug.c" "%visionSource%\app_runtime.c" "%visionSource%\app_transport.c" "%visionSource%\vision_frame.c" "%visionSource%\vision_telemetry.c" "%visionSource%\vision_midterm.c" "%visionSource%\track_bayes.c" "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_field.c" "%visionStorage%" /Fooutput\ /Feoutput\test_debug_enabled.exe
if errorlevel 1 goto fail
pushd output
test_debug_enabled.exe
set "visionResult=%errorlevel%"
popd
if not "%visionResult%"=="0" goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% %visionDriveDefines% /DVISION_WIRELESS_UART_ENABLE=1 /I"stubs" /I"%visionSource%" test_debug.c "%visionSource%\vision_debug.c" "%visionSource%\app_runtime.c" "%visionSource%\app_transport.c" "%visionSource%\vision_frame.c" "%visionSource%\vision_telemetry.c" "%visionSource%\vision_midterm.c" "%visionSource%\track_bayes.c" "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_field.c" "%visionStorage%" /Fooutput\ /Feoutput\test_debug_wireless.exe
if errorlevel 1 goto fail
pushd output
test_debug_wireless.exe
set "visionResult=%errorlevel%"
popd
if not "%visionResult%"=="0" goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" test_bayes.c "%visionSource%\track_bayes.c" "%visionSource%\midterm_open_loop.c" /Fooutput\ /Feoutput\test_bayes.exe
if errorlevel 1 goto fail
output\test_bayes.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%visionSource%" test_field.c "%visionSource%\vision_field.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionStorage%" /Fooutput\ /Feoutput\test_field.exe
if errorlevel 1 goto fail
output\test_field.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%visionSource%" test_servo_probe.c "%visionSource%\vision_field.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionStorage%" /Fooutput\ /Feoutput\test_servo_probe.exe
if errorlevel 1 goto fail
output\test_servo_probe.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%visionSource%" test_motor_probe.c "%visionSource%\vision_field.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionStorage%" /Fooutput\ /Feoutput\test_motor_probe.exe
if errorlevel 1 goto fail
output\test_motor_probe.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=1 /I"stubs" /I"%visionSource%" test_startup.c "%visionSource%\vision_field.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionStorage%" /Fooutput\ /Feoutput\test_startup.exe
if errorlevel 1 goto fail
output\test_startup.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"stubs" /I"%visionSource%" test_board_mode.c "%visionSource%\board_diagnostics.c" "%visionSource%\board_io.c" "%visionSource%\board_camera.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" /Fooutput\ /Feoutput\test_board_mode.exe
if errorlevel 1 goto fail
output\test_board_mode.exe
if errorlevel 1 goto fail
echo ALL HOST SUITES PASSED. Synthetic IO is not real-vehicle validation.
popd
exit /b 0
:drive
set "visionDriveName=%1"
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% %visionDriveDefines% /I"stubs" /I"%visionSource%" test_drive.c "%visionSource%\vision_control.c" "%visionSource%\midterm_open_loop.c" "%visionSource%\vision_drive.c" %visionAdapter% "%visionSource%\vision_field.c" "%visionStorage%" /Fooutput\ /Feoutput\%visionDriveName%.exe
if errorlevel 1 exit /b 1
output\%visionDriveName%.exe
exit /b %errorlevel%
:fail
echo HOST TEST FAILED
popd
exit /b 1
