param([string]$Uv4='E:\ElecTronic\Keil\UV4\UV4.exe')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$run='run-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')+'-'+$PID
$base=Join-Path $repo ('.build\recorder-camera-stages-20261106\'+$run)
function Build-One([string]$out){
 $original=Join-Path $repo 'demo\recorder_b2\mdk\recorder_b2.uvprojx'
 $project=Join-Path $out 'candidate.uvprojx'
 Copy-Item -LiteralPath $original -Destination $project -ErrorAction Stop
 $proc=$null
 try {
  $log=Join-Path $out 'keil-build.log'
  $proc=Start-Process -FilePath $Uv4 -ArgumentList @('-b',('"'+$project+'"'),'-j0','-o',('"'+$log+'"')) -WindowStyle Hidden -PassThru
  $timer=[Diagnostics.Stopwatch]::StartNew()
  while(!$proc.WaitForExit(1000)){if($timer.Elapsed.TotalSeconds -gt 180){throw 'Owned Keil build timeout'}}
  if($proc.ExitCode -ne 0 -or (Get-Content -LiteralPath $log -Raw) -notmatch '0 Error\(s\), 0 Warning\(s\)'){throw ('Keil failed: '+$log)}
 } finally {if($null -ne $proc){try{if(!$proc.HasExited){& "$env:SystemRoot\System32\taskkill.exe" /PID ([string]$proc.Id) /T /F | Out-Null}}finally{$proc.Dispose()}}}
}
Push-Location $repo
try {
 $seed=$base+'-seed';$final=$base+'-final'
 & python tools/prepare_recorder_b2.py --output $seed
 if($LASTEXITCODE -ne 0){throw 'Preparation failed'}
 Build-One $seed
 & python tools/prepare_recorder_b2.py --output $final --dependencies-from $seed
 if($LASTEXITCODE -ne 0){throw 'Dependency freeze failed'}
 Build-One $final
 $fromelf=Join-Path (Split-Path (Split-Path $Uv4 -Parent) -Parent) 'ARM\ARMCLANG\bin\fromelf.exe'
 & $fromelf --bincombined --output (Join-Path $final 'Objects\recorder_b2.bin') (Join-Path $final 'Objects\recorder_b2.axf')
 if($LASTEXITCODE -ne 0){throw 'BIN conversion failed'}
 & python tools/verify_recorder_b2.py $final
 if($LASTEXITCODE -ne 0){throw 'MAP/dependency isolation check failed'}
 Write-Output ('Build only, hardware unverified; new artifacts: '+$final)
} finally {Pop-Location}
