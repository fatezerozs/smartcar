param([string]$Port='COM14',[ValidateSet(115200,460800,921600)][int]$Baud=921600,[switch]$CheckOnly)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'tools\SerialExport.cs')
[SdSerialExport]::CheckOnly()
if($CheckOnly){exit 0}
$dir=Join-Path $PSScriptRoot ('evidence\readback-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')+'-'+$PID)
New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
$record=[ordered]@{port=$Port;baud=$Baud;user_power_cycle_confirmed_in_chat=$true;agent_reset=$false;agent_flashed=$false;sd_write_test_sent=$false;downloaded=$false;pc_verified=$false;started_utc=[DateTime]::UtcNow.ToString('o')}
try {
 [SdSerialExport]::Download($Port,$Baud,$dir)
 $record.downloaded=$true
 & python (Join-Path $PSScriptRoot 'tools\verify_download.py') $dir
 if($LASTEXITCODE -ne 0){throw 'Independent PC file verification failed'}
 $record.pc_verified=$true
} catch {
 $record.error=$_.Exception.Message
 Write-Host ('STOP: '+$_.Exception.Message)
} finally {
 $record.finished_utc=[DateTime]::UtcNow.ToString('o')
 $record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $dir 'acquisition.json') -Encoding UTF8
 Write-Host ('Evidence: '+$dir)
}
if(!$record.pc_verified){exit 1}
