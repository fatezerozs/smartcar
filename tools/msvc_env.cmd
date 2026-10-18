@echo off
where cl.exe >nul 2>nul
if not errorlevel 1 exit /b 0
set "visionVsWhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%visionVsWhere%" (
  echo ERROR: Run from an x64 Visual Studio Developer Command Prompt.
  exit /b 1
)
set "visionVsInstall="
for /f "usebackq delims=" %%I in (`"%visionVsWhere%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "visionVsInstall=%%I"
if not defined visionVsInstall exit /b 1
call "%visionVsInstall%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
