@echo off
call "%~dp0..\tools\msvc_env.cmd"
cl /nologo /utf-8 /std:c11 /W3 /Od /Zi /fsanitize=address tests\test_entry_compat.c /Fo.build\ /Fe.build\entry_compat.exe
if errorlevel 1 exit /b 1
.build\entry_compat.exe
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /std:c11 /W3 /Od /Zi /fsanitize=address /Itests\brick_host\stubs /Idemo\user\inc /Idemo\mdk tests\test_brick_isolation.c demo\mdk\IMG_DEAL.c demo\mdk\Threshold.c /Fo.build\ /Fe.build\brick_isolation.exe
if errorlevel 1 exit /b 1
.build\brick_isolation.exe
