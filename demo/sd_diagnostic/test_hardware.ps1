param([string]$Port='COM14')
$ErrorActionPreference='Stop'
$run='hardware-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$dir=Join-Path $PSScriptRoot ('evidence\'+$run)
New-Item -ItemType Directory -Path $dir -ErrorAction Stop | Out-Null
$log=Join-Path $dir 'serial.log'
$record=[ordered]@{port=$Port;baud=115200;flashed_by_user=$true;reset_by_agent=$false;started_utc=[DateTime]::UtcNow.ToString('o');help=$null;status=$null;basic=$null;stress=$null;outcome='incomplete'}
$serial=New-Object System.IO.Ports.SerialPort $Port,115200,([System.IO.Ports.Parity]::None),8,([System.IO.Ports.StopBits]::One)
$serial.Handshake=[System.IO.Ports.Handshake]::None
$serial.DtrEnable=$false
$serial.RtsEnable=$false
$serial.ReadTimeout=200
$serial.WriteTimeout=1000
function Save-Line([string]$s){Add-Content -LiteralPath $log -Value $s -Encoding UTF8;Write-Host $s}
function Query([string]$command,[string]$pattern,[int]$seconds){
 Save-Line ("[{0}] TX {1}" -f [DateTime]::UtcNow.ToString('o'),$command)
 $serial.Write($command+"`r`n")
 $watch=[Diagnostics.Stopwatch]::StartNew();$response=''
 while($watch.Elapsed.TotalSeconds -lt $seconds){
  $chunk=$serial.ReadExisting()
  if($chunk){$response+=$chunk;Add-Content -LiteralPath $log -Value $chunk -Encoding UTF8 -NoNewline;Write-Host -NoNewline $chunk}
  if($response -match $pattern){return $response}
  Start-Sleep -Milliseconds 50
 }
 throw "Timeout waiting for $command after ${seconds}s; no further command will be sent"
}
try {
 $serial.Open()
 Save-Line ('Opened '+$Port+' 115200 8N1; DTR/RTS disabled; no reset requested')
 Start-Sleep -Milliseconds 1500
 $initial=$serial.ReadExisting();if($initial){Save-Line ('INITIAL '+$initial)}
 $record.help=Query 'SDDIAG HELP' 'SDDIAG HELP \| STATUS \| RUN BASIC \| RUN STRESS \| RESULT\r?\n' 10
 $record.status=Query 'SDDIAG STATUS' 'SD present=[^\r\n]+\r?\n' 20
 if($record.status -notmatch 'FS mounted=1' -or $record.status -notmatch 'SD present=1 initialized=1 fault=0'){throw 'SD STATUS not ready; no write test sent'}
 $record.basic=Query 'SDDIAG RUN BASIC' 'RESULT [^\r\n]+\r?\n' 60
 if($record.basic -notmatch 'RESULT pass=1 errors=0 stage=done' -or $record.basic -notmatch 'written=65536 read=65536'){throw 'BASIC failed; STRESS not sent'}
 $null=Query 'SDDIAG RESULT' 'RESULT [^\r\n]+\r?\n' 10
 Save-Line 'BASIC verified; starting one bounded 16MiB STRESS'
 $record.stress=Query 'SDDIAG RUN STRESS' 'RESULT [^\r\n]+\r?\n' 300
 if($record.stress -notmatch 'RESULT pass=1 errors=0 stage=done' -or $record.stress -notmatch 'written=16777216 read=16777216'){throw 'STRESS failed'}
 $null=Query 'SDDIAG RESULT' 'RESULT [^\r\n]+\r?\n' 10
 $record.outcome='passed'
 Save-Line 'PASS hardware BASIC and 16MiB STRESS; files retained'
} catch {
 $record.outcome='stopped';$record.error=$_.Exception.Message
 Save-Line ('STOP '+$_.Exception.Message)
} finally {
 if($serial.IsOpen){$serial.Close()};$serial.Dispose()
 $record.finished_utc=[DateTime]::UtcNow.ToString('o')
 $record|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $dir 'result.json') -Encoding UTF8
 Write-Host ('Evidence: '+$dir)
}
if($record.outcome -ne 'passed'){exit 1}
