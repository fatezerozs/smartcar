param([string]$UV4 = 'C:\Keil_v5\UV4\UV4.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'demo\mdk\rt1064.uvprojx'
$outputRoot = Join-Path $projectRoot '.build\keil'
$compilerTemp = Join-Path $outputRoot 'tmp'
New-Item -ItemType Directory -Path $compilerTemp -Force | Out-Null
if (-not (Test-Path -LiteralPath $UV4)) { throw 'Keil UV4 not found; supply -UV4 for your installation.' }
$logFile = Join-Path $outputRoot 'build.log'
# Child-only temporary directory avoids restricted or unavailable system TEMP.
# Rebuild only: no download/debug switches and no hardware access requested.
$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $UV4
$startInfo.Arguments = '-r "' + $projectFile + '" -t "nor_sdram_zf_dtcm" -o "' + $logFile + '"'
$startInfo.WorkingDirectory = Split-Path $projectFile -Parent
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Hidden
$startInfo.EnvironmentVariables['TEMP'] = $compilerTemp
$startInfo.EnvironmentVariables['TMP'] = $compilerTemp
$buildProcess = [System.Diagnostics.Process]::Start($startInfo)
$buildProcess.WaitForExit()
$buildExit = $buildProcess.ExitCode
Write-Output ('Keil rebuild exit code: ' + $buildExit)
if (Test-Path -LiteralPath $logFile) { Get-Content -LiteralPath $logFile -Tail 18 }
if ($buildExit -ne 0) { throw ('Keil rebuild failed; inspect ' + $logFile) }
Write-Output 'FULL KEIL REBUILD PASSED; firmware was not flashed.'
