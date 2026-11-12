@echo off
call "C:\Users\dick\Desktop\智能车\c26-28-debug-20261104\smartcar-8453672\tools\msvc_env.cmd"
cl /nologo /utf-8 /std:c11 /W3 /Od /Zi /fsanitize=address /Itests\brick_host\stubs /Idemo\user\inc /Idemo\mdk tests\brick_host\test_brick.c demo\mdk\IMG_DEAL.c demo\mdk\Threshold.c /Fotests\brick_host\ /Fetests\brick_host\brick.exe
if errorlevel 1 exit /b 1
tests\brick_host\brick.exe
