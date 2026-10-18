@echo off
setlocal
call "%~dp0..\..\tools\msvc_env.cmd"
if errorlevel 1 exit /b 1
pushd "%~dp0"
if not exist output mkdir output
set "visionSource=%~dp0..\..\demo\code"
set "visionFlags=/O2"
if "%~1"=="asan" set "visionFlags=/Od /Zi /Fdoutput\ /fsanitize=address"
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" /Dvision_midterm_reset=reference_reset /Dvision_midterm_process=reference_process /Dvision_midterm_preview=reference_preview /Dvision_midterm_status_name=reference_status_name /c reference\vision_reference.c /Fooutput\reference.obj
if errorlevel 1 goto fail
cl /nologo /utf-8 /std:c11 /W4 /WX %visionFlags% /I"%visionSource%" test_equivalence.c "%visionSource%\vision_midterm.c" output\reference.obj /Fooutput\ /Feoutput\test_equivalence.exe
if errorlevel 1 goto fail
output\test_equivalence.exe
set "visionResult=%errorlevel%"
popd
exit /b %visionResult%
:fail
popd
exit /b 1
