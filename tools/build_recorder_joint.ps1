param([string]$Uv4='E:\ElecTronic\Keil\UV4\UV4.exe')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$base=Join-Path $repo ('.build/recorder-joint-20261106/run-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmss')+'-'+$PID)
function Build-One([string]$out){
 $project=Join-Path $out 'candidate.uvprojx';$proc=$null
 try{
  $log=Join-Path $out 'keil-build.log'
  $proc=Start-Process -FilePath $Uv4 -ArgumentList @('-b',('"'+$project+'"'),'-j0','-o',('"'+$log+'"')) -WindowStyle Hidden -PassThru
  $watch=[Diagnostics.Stopwatch]::StartNew()
  while(!$proc.WaitForExit(1000)){if($watch.Elapsed.TotalSeconds -gt 180){throw 'Owned joint candidate build timed out'}}
  if($proc.ExitCode -ne 0 -or (Get-Content -LiteralPath $log -Raw) -notmatch '0 Error\(s\), 0 Warning\(s\)'){throw ('Keil build failed: '+$log)}
 }finally{if($null -ne $proc){try{if(!$proc.HasExited){& "$env:SystemRoot/System32/taskkill.exe" /PID ([string]$proc.Id) /T /F|Out-Null}}finally{$proc.Dispose()}}}
}
Push-Location $repo
try{
 $seed=$base+'-seed';$final=$base+'-final'
 & python tools/prepare_recorder_joint.py --output $seed
 if($LASTEXITCODE -ne 0){throw 'Joint preparation failed'}
 Build-One $seed
 & python tools/prepare_recorder_joint.py --output $final --dependencies-from $seed
 if($LASTEXITCODE -ne 0){throw 'Joint dependency closure failed'}
 Build-One $final
 $fromelf=Join-Path (Split-Path (Split-Path $Uv4 -Parent) -Parent) 'ARM/ARMCLANG/bin/fromelf.exe'
 & $fromelf --bincombined --output (Join-Path $final 'Objects/recorder_joint_static.bin') (Join-Path $final 'Objects/recorder_joint_static.axf')
 if($LASTEXITCODE -ne 0){throw 'Joint BIN conversion failed'}
 & python tools/verify_recorder_joint.py $final
 if($LASTEXITCODE -ne 0){throw 'Joint source/MAP verification failed'}
 Write-Output ('Joint static build only; no download: '+$final)
}finally{Pop-Location}
