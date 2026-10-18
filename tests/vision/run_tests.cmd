@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output mkdir output
set "visionSource=%~dp0..\..\demo\code"
set "visionFlags=/O2"
if "%~1"=="asan" set "visionFlags=/Od /Zi /Fdoutput\ /fsanitize=address"
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" test_vision.c "%visionSource%\vision_midterm.c" /Fooutput\ /Feoutput\test_vision.exe
if errorlevel 1 goto fail
pushd output
test_vision.exe
set "visionResult=%errorlevel%"
popd
if not "%visionResult%"=="0" goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"stubs" /I"%visionSource%" test_debug.c "%visionSource%\vision_debug.c" "%visionSource%\vision_midterm.c" /Fooutput\ /Feoutput\test_debug.exe
if errorlevel 1 goto fail
pushd output
test_debug.exe
set "visionResult=%errorlevel%"
popd
if not "%visionResult%"=="0" goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" test_control.c "%visionSource%\vision_control.c" /Fooutput\ /Feoutput\test_control.exe
if errorlevel 1 goto fail
output\test_control.exe
if errorlevel 1 goto fail
set "visionDriveDefines="
call :drive test_drive
if errorlevel 1 goto fail
set "visionDriveDefines=/DVISION_DRIVE_ENABLE=1"
call :drive test_drive_unconfigured
if errorlevel 1 goto fail
set "visionDriveDefines=/DVISION_DRIVE_ENABLE=1 /DVISION_DRIVE_PROFILE_CONFIRMED=1 /DVISION_CAMERA_CALIBRATED=1 /DVISION_SERVO_CENTER_US=1500 /DVISION_SERVO_MIN_US=1200 /DVISION_SERVO_MAX_US=1800 /DVISION_SERVO_RIGHT_SIGN=1 /DVISION_MOTOR_DUTY_CAP=0.2f /DVISION_MOTOR_CRUISE_DUTY=0.1f"
call :drive test_drive_enabled
if errorlevel 1 goto fail
echo ALL HOST SUITES PASSED. Synthetic IO is not real-vehicle validation.
popd
exit /b 0
:drive
set "visionDriveName=%1"
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% %visionDriveDefines% /I"stubs" /I"%visionSource%" test_drive.c "%visionSource%\vision_control.c" "%visionSource%\vision_drive.c" /Fooutput\ /Feoutput\%visionDriveName%.exe
if errorlevel 1 exit /b 1
output\%visionDriveName%.exe
exit /b %errorlevel%
:fail
echo HOST TEST FAILED
popd
exit /b 1
