@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output\switch mkdir output\switch
set "speedSource=..\..\demo\code"
set "speedFlags=/O2"
if "%~1"=="asan" set "speedFlags=/Od /Zi /fsanitize=address"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MIDTERM_TRAINING=1 /DVISION_MIDTERM_AUTONOMOUS=0 /DVISION_MIDTERM_ROUTE_TRIAL=1 /DVISION_ROUTE_SWITCH=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_route_switch.c "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\switch\ /Fdoutput\switch\ /Feoutput\switch\test_route_switch.exe
if errorlevel 1 goto fail
output\switch\test_route_switch.exe
if errorlevel 1 goto fail
set "dualSource=..\..\demo\code"
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_MIDTERM_AUTONOMOUS=0 /DVISION_MIDTERM_TRAINING=1 /DVISION_MIDTERM_ROUTE_TRIAL=1 /DVISION_ROUTE_SWITCH=1 /DVISION_ENCODER_ENABLE=1 /DVISION_DUAL_UART_ENABLE=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%dualSource%" test_switch_runtime.c "%dualSource%\midterm_boot.c" "%dualSource%\vision_speed.c" "%dualSource%\wheel_speed.c" "%dualSource%\app_link.c" "%dualSource%\app_transport.c" "%dualSource%\app_runtime.c" "%dualSource%\vision_debug.c" "%dualSource%\vision_frame.c" "%dualSource%\vision_telemetry.c" "%dualSource%\vision_midterm.c" "%dualSource%\track_bayes.c" "%dualSource%\vision_control.c" "%dualSource%\midterm_open_loop.c" "%dualSource%\vision_drive.c" "%dualSource%\vision_motion.c" "%dualSource%\vision_actuator.c" "%dualSource%\vision_guard.c" "%dualSource%\vision_probe.c" "%dualSource%\vision_startup.c" "%dualSource%\vision_vehicle_profile.c" "%dualSource%\app_mode.c" "%dualSource%\vision_field.c" "%dualSource%\vision_storage.c" /Fooutput\switch\ /Fdoutput\switch\ /Feoutput\switch\test_switch_runtime.exe
if errorlevel 1 goto fail
output\switch\test_switch_runtime.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ENCODER_ENABLE=1 /DVISION_MIDTERM_TRAINING=1 /DVISION_MIDTERM_AUTONOMOUS=0 /DVISION_MIDTERM_ROUTE_TRIAL=1 /DVISION_ROUTE_SWITCH=1 /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%speedSource%" test_turn_route.c "%speedSource%\vision_midterm.c" "%speedSource%\vision_speed.c" "%speedSource%\wheel_speed.c" "%speedSource%\vision_motion.c" "%speedSource%\vision_drive.c" "%speedSource%\vision_actuator.c" "%speedSource%\vision_guard.c" "%speedSource%\vision_probe.c" "%speedSource%\vision_startup.c" "%speedSource%\vision_vehicle_profile.c" "%speedSource%\vision_control.c" "%speedSource%\midterm_open_loop.c" "%speedSource%\app_mode.c" "%speedSource%\vision_field.c" "%speedSource%\vision_storage.c" /Fooutput\switch\ /Fdoutput\switch\ /Feoutput\switch\test_turn_route.exe
if errorlevel 1 goto fail
output\switch\test_turn_route.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ROUTE_SWITCH=1 /I"stubs" /I"%speedSource%" test_roundabout.c "%speedSource%\vision_midterm.c" /Fooutput\switch\ /Fdoutput\switch\ /Feoutput\switch\test_roundabout.exe
if errorlevel 1 goto fail
output\switch\test_roundabout.exe
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %speedFlags% /DVISION_ROUTE_SWITCH=1 /I"stubs" /I"%speedSource%" test_roundabout_projection.c "%speedSource%\vision_midterm.c" /Fooutput\switch\ /Fdoutput\switch\ /Feoutput\switch\test_roundabout_projection.exe
if errorlevel 1 goto fail
output\switch\test_roundabout_projection.exe
if errorlevel 1 goto fail
popd
exit /b 0
:fail
popd
exit /b 1
