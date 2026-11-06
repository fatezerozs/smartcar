param([string]$Uv4='E:\ElecTronic\Keil\UV4\UV4.exe')
$ErrorActionPreference='Stop'
$here=$PSScriptRoot
$lock=$null
try {
 $lock=[IO.File]::Open((Join-Path $here 'build.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
} catch {throw 'Another SD diagnostic build is active; no build started'}
try {
$run='run-'+(Get-Date -Format 'yyyyMMddTHHmmssfff')+'-'+$PID
$dest=Join-Path $here ('artifacts\'+$run)
if(Test-Path -LiteralPath $dest){throw 'Refusing to reuse artifact directory'}
New-Item -ItemType Directory -Path $dest | Out-Null
$project=Join-Path $here 'mdk\sd_performance.uvprojx'
& python (Join-Path $here 'configure.py') $run
if($LASTEXITCODE -ne 0){throw 'Project configuration failed'}
$log=Join-Path $dest 'keil-build.log'
$proc=$null
try {
 $proc=Start-Process -FilePath $Uv4 -ArgumentList @('-b',('"'+$project+'"'),'-j0','-o',('"'+$log+'"')) -WindowStyle Hidden -PassThru
 $timer=[Diagnostics.Stopwatch]::StartNew()
 while(-not $proc.WaitForExit(1000)){if($timer.Elapsed.TotalSeconds -gt 180){throw 'Build timeout'}}
 if(!(Test-Path -LiteralPath $log)){throw 'Build log absent'}
 $text=Get-Content -LiteralPath $log -Raw
 Write-Output $text
 if($proc.ExitCode -ne 0 -or $text -notmatch '0 Error\(s\), 0 Warning\(s\)'){throw 'Keil build failed'}
 $fromelf=Join-Path (Split-Path (Split-Path $Uv4 -Parent) -Parent) 'ARM\ARMCLANG\bin\fromelf.exe'
 $axf=Join-Path $dest 'Objects\sd_performance.axf'
 $bin=Join-Path $dest 'Objects\sd_performance.bin'
 if(Test-Path -LiteralPath $bin){throw 'Refusing to overwrite BIN'}
 & $fromelf --bincombined --output $bin $axf
 if($LASTEXITCODE -ne 0){throw 'BIN conversion failed'}
 & python (Join-Path $here 'verify.py') $dest
 if($LASTEXITCODE -ne 0){throw 'Isolation/preservation verification failed'}
 Write-Output "PASS: $dest; hardware unverified; no flash or serial communication"
} finally {
 if($null -ne $proc){try{if(!$proc.HasExited){& "$env:SystemRoot\System32\taskkill.exe" /PID ([string]$proc.Id) /T /F | Out-Null}}finally{$proc.Dispose()}}
}

} finally {if($null -ne $lock){$lock.Dispose()}}
