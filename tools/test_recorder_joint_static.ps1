param([string]$Port='COM14',[switch]$CheckOnly,[switch]$C26TransitionTest)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'JointStaticClient.cs')
[JointStaticClient]::SelfTest()
if($CheckOnly){exit 0}
$repo=Split-Path $PSScriptRoot -Parent
$build=(Get-Content -LiteralPath (Join-Path $repo 'docs/changes/recorder-b3-20261106/encoder-joint-static-20261106/current-candidate.txt') -Raw).Trim()
$identity=(Get-Content -LiteralPath (Join-Path $build 'inputs.json') -Raw|ConvertFrom-Json).manifest_digest
$out=Join-Path (Split-Path $repo -Parent) ('joint_static_'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $out|Out-Null
$record=@{port=$Port;build=$build;source=$identity;out=$out;c26_transition_test=[bool]$C26TransitionTest;serial_comparison_complete=$false;sd_file_crc_verified_on_PC=$false;hardware_motion_accepted=$false;flash_reset=$false;motor_servo_power_changed=$false}
try{[JointStaticClient]::Run($Port,$out,$identity,[bool]$C26TransitionTest);$record.serial_comparison_complete=$true}
catch{$record.error=$_.Exception.Message;Write-Host ('STOP: '+$record.error)}
finally{$record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'acquisition.json') -Encoding utf8;Write-Host ('Evidence: '+$out)}
if(!$record.serial_comparison_complete){exit 1}
