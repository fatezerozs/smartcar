@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
set "motionOutput=output\motion"
set "motionDiag="
if "%~2"=="diag" set "motionOutput=output\motion-diag"
if "%~2"=="diag" set "motionDiag=/DVISION_MOTION_DIAG=1 /DVISION_MOTOR_THRESHOLD_DIAG=1"
if not exist %motionOutput% mkdir %motionOutput%
set "motionSource=..\..\demo\code"
set "motionFlags=/O2"
if "%~1"=="asan" set "motionFlags=/Od /Zi /fsanitize=address"
cl /nologo /utf-8 /std:c11 /W4 /WX %motionFlags% %motionDiag% /DVISION_DRIVE_ENABLE=1 /DVISION_START_SWITCH_ENABLE=0 /I"stubs" /I"%motionSource%" test_motion.c "%motionSource%\vision_motion.c" "%motionSource%\vision_drive.c" "%motionSource%\vision_actuator.c" "%motionSource%\vision_guard.c" "%motionSource%\vision_probe.c" "%motionSource%\vision_startup.c" "%motionSource%\vision_vehicle_profile.c" "%motionSource%\vision_control.c" "%motionSource%\midterm_open_loop.c" "%motionSource%\app_mode.c" "%motionSource%\vision_field.c" "%motionSource%\vision_storage.c" /Fo%motionOutput%\ /Fd%motionOutput%\ /Fe%motionOutput%\test_motion.exe
if errorlevel 1 goto fail
%motionOutput%\test_motion.exe
if errorlevel 1 goto fail
popd
exit /b 0
:fail
popd
exit /b 1
