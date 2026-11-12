@echo off
call "%~dp0..\tools\msvc_env.cmd"
cl /nologo /utf-8 /std:c11 /W4 /WX /Od /Zi /fsanitize=address /Itests /Idemo\user\inc tests\test_s_turn.c /Fo.build\ /Fe.build\test_s_turn.exe
if errorlevel 1 exit /b 1
.build\test_s_turn.exe
