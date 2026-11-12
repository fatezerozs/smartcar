@echo off
call "%~dp0..\tools\msvc_env.cmd"
cl /nologo /utf-8 /std:c11 /W4 /WX /Od /Zi /fsanitize=address /Idemo\user\inc tests\test_tuning.c /Fo.build\ /Fe.build\test_tuning.exe
if errorlevel 1 exit /b 1
.build\test_tuning.exe
cl /nologo /utf-8 /std:c11 /W4 /WX /Od /Zi /fsanitize=address /Itests\stubs /Idemo\user\inc tests\test_pid.c demo\user\src\pid.c /Fo.build\ /Fe.build\test_pid.exe
if errorlevel 1 exit /b 1
.build\test_pid.exe
