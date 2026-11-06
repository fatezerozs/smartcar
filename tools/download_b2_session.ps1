param([string]$Port='COM14',[string]$Session='R0000001',[switch]$CheckOnly)
$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'B2SessionExport.cs')
[B2SessionExport]::SelfTest()
if($CheckOnly){exit 0}
$repo=Split-Path $PSScriptRoot -Parent
$out=Join-Path (Split-Path $repo -Parent) ('b2_real_session_'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $out -ErrorAction Stop|Out-Null
$record=@{port=$Port;session=$Session;out=$out;downloaded=$false;pc_verified=$false;camera_start_sent=$false;reset_or_flash=$false;sd_writes=$false}
try{
 [B2SessionExport]::Download($Port,$Session,$out);$record.downloaded=$true
 & python (Join-Path $PSScriptRoot 'verify_b2_session.py') --source (Join-Path $out $Session) --output (Join-Path $out 'pc')
 if($LASTEXITCODE -ne 0){throw 'PC Session verification failed or incomplete'}
 $record.pc_verified=$true
}catch{$record.error=$_.Exception.Message;Write-Host ('STOP: '+$record.error)}
finally{$record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'acquisition.json') -Encoding utf8;Write-Host ('Evidence: '+$out)}
if(!$record.pc_verified){exit 1}
