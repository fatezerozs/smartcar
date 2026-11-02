@echo off
setlocal
set "testingMode=optimized"
if "%~1"=="asan" set "testingMode=asan"
if not defined SMARTCAR_TEST_COMPILER set "SMARTCAR_TEST_COMPILER=msvc"
set "testingVariant=normal"
if "%~2"=="diag" set "testingVariant=diagnostic"
pwsh -NoLogo -NoProfile -NonInteractive -File "%~dp0..\..\tools\testing\Invoke-TestPlan.ps1" -Suite motion -Compiler "%SMARTCAR_TEST_COMPILER%" -Mode "%testingMode%" -Variant "%testingVariant%"
exit /b %errorlevel%
