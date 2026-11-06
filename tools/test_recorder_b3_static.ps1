param([string]$Port='COM14',[switch]$CheckOnly,[switch]$C26TransitionTest,[switch]$DownloadOnly)
$ErrorActionPreference='Stop'
Add-Type -Path @((Join-Path $PSScriptRoot 'B2SessionExport.cs'),(Join-Path $PSScriptRoot 'B3StaticTrace.cs'))
[B2SessionExport]::SelfTest();[B3StaticTrace]::SelfTest()
if($CheckOnly){exit 0}
$repo=Split-Path $PSScriptRoot -Parent
$build=(Get-Content -LiteralPath (Join-Path $repo 'docs/changes/recorder-b3-20261106/evidence/current-candidate.txt') -Raw).Trim()
$identity=(Get-Content -LiteralPath (Join-Path $build 'inputs.json') -Raw|ConvertFrom-Json).manifest_digest
$out=Join-Path (Split-Path $repo -Parent) ('b3_static_trace_'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $out|Out-Null
$record=@{role='B3 prerequisite stationary primary baseline';port=$Port;out=$out;downloaded=$false;pc_verified=$false;motor_servo_power_changed=$false;flash_reset=$false;motion_accepted=$false;build=$build}
try{
 [B3StaticTrace]::Capture($Port,$out,$identity,[bool]$C26TransitionTest,[bool]$DownloadOnly);$record.downloaded=$true
 & python (Join-Path $PSScriptRoot 'verify_b3_static.py') $out
 if($LASTEXITCODE -ne 0){throw 'Static trace incomplete or invalid; preserve raw bytes'}
 $record.pc_verified=$true
}catch{$record.error=$_.Exception.Message;Write-Host ('STOP: '+$record.error)}
finally{$record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'acquisition.json') -Encoding utf8;Write-Host ('Evidence: '+$out)}
if(!$record.pc_verified){exit 1}
