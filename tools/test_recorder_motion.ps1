param([string]$Port='COM14',[string]$BuildDirectory,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'MotionClient.cs')
[MotionClient]::SelfTest()
if($CheckOnly){exit 0}
if(!$BuildDirectory){throw 'An explicit motion candidate directory is required'}
$repo=Split-Path $PSScriptRoot -Parent
$proof=Get-Content -LiteralPath (Join-Path $BuildDirectory 'motion-evidence.json') -Raw|ConvertFrom-Json
if(!$proof.build_passed -or $proof.limit_ms -ne 2900 -or $proof.motor_pwm_cap_permille -ne 200){throw 'Wrong motion profile'}
$out=Join-Path (Split-Path $repo -Parent) ('recorder_motion_'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $out -ErrorAction Stop|Out-Null
$record=@{port=$Port;build=$BuildDirectory;source=$proof.source_digest;out=$out;motion_command_comparison_complete=$false;physical_movement_verified=$false;sd_file_crc_verified_on_PC=$false;flash_reset=$false;PI_static_gate='USER_WAIVED_NOT_MEASURED_PASS'}
try{[MotionClient]::Run($Port,$out,$proof.source_digest,$true);$record.motion_command_comparison_complete=$true}
catch{$record.error=$_.Exception.Message;Write-Host ('STOP: '+$record.error)}
finally{$record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'acquisition.json') -Encoding utf8;Write-Host ('Evidence: '+$out)}
if(!$record.motion_command_comparison_complete){exit 1}
