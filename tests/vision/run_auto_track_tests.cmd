@echo off
setlocal
set "testingMode=optimized"
if "%~1"=="asan" set "testingMode=asan"
if not defined SMARTCAR_TEST_COMPILER set "SMARTCAR_TEST_COMPILER=msvc"
pwsh -NoLogo -NoProfile -NonInteractive -File "%~dp0..\..\tools\testing\Invoke-TestPlan.ps1" -Suite auto-track -Compiler "%SMARTCAR_TEST_COMPILER%" -Mode "%testingMode%"
exit /b %errorlevel%
