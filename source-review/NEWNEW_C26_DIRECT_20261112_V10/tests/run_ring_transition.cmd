@echo off
call "%~dp0..\tools\msvc_env.cmd"
cl /nologo /utf-8 /std:c11 /W3 /Od /Zi /fsanitize=address /Itests\brick_host\stubs /Idemo\user\inc /Idemo\mdk tests\test_ring_transition.c demo\mdk\IMG_DEAL.c demo\mdk\Threshold.c /Fo.build\ /Fe.build\ring_transition.exe
if errorlevel 1 exit /b 1
.build\ring_transition.exe
