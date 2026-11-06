param([string]$Port='COM14',[switch]$RequireMetrics)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$out=Join-Path $repo ('docs/changes/recorder-camera-stages-20261106/evidence/b2-hardware-'+[DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ'))
New-Item -ItemType Directory -Path $out | Out-Null
$serial=[System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$serial.DtrEnable=$false; $serial.RtsEnable=$false; $serial.Handshake=[System.IO.Ports.Handshake]::None
$serial.ReadTimeout=200; $serial.WriteTimeout=1000
$raw=[System.IO.File]::Create((Join-Path $out 'serial.bin'))
$events=[System.IO.StreamWriter]::new((Join-Path $out 'events.jsonl'))
$script:received=''
function Event($kind,$value){$events.WriteLine((@{utc=[DateTime]::UtcNow.ToString('o');kind=$kind;value=$value}|ConvertTo-Json -Compress));$events.Flush()}
function Receive-For([double]$seconds){
 $clock=[Diagnostics.Stopwatch]::StartNew()
 while($clock.Elapsed.TotalSeconds -lt $seconds){
  $n=$serial.BytesToRead
  if($n -gt 0){
   $buffer=New-Object byte[] ([Math]::Min($n,4096));$count=$serial.Read($buffer,0,$buffer.Length)
   $raw.Write($buffer,0,$count);$raw.Flush()
   $s=[Text.Encoding]::ASCII.GetString($buffer,0,$count);$script:received+=$s;Event 'rx' $s
  }
  Start-Sleep -Milliseconds 25
 }
}
$result=@{port=$Port;baud=115200;identity_verified=$false;start_sent=$false;stop_sent=$false;board_closed=$false;hardware_accepted=$false;pc_session_verified=$false;agent_flash_or_reset=$false;out=$out}
try {
 $serial.Open();Event 'open' 'DTR=false RTS=false; user confirmed flash and physical isolation'
 Receive-For 1
 $serial.Write("B2 STATUS`r`n");Event 'tx' 'B2 STATUS';Receive-For 6
 if($script:received -notmatch '\[B2\] ready id=RECORDER_CAMERA_B2_STATIC_1 rx=irq camera_started=0 session_started=0' -or ($RequireMetrics -and $script:received -notmatch 'export=1 metrics=2')){
  $result.reason='No matching idle B2 identity; START withheld'
 }else{
  $result.identity_verified=$true
  $serial.Write("B2 START`r`n");Event 'tx' 'B2 START';$result.start_sent=$true
  $timer=[Diagnostics.Stopwatch]::StartNew()
  while($timer.Elapsed.TotalSeconds -lt 35){
   Receive-For 0.5
   if($script:received -match '\[B2\] camera init failed'){ $result.reason='Camera initialization failed';break }
   if($script:received -match '\[B2\] state=5\b'){ $result.reason='Storage fault';break }
   if($script:received -match '\[B2\] state=4 error=0 written=(\d+) synced=(\d+)'){
    $result.board_closed=$true;$result.written=[long]$Matches[1];$result.synced=[long]$Matches[2]
    if(!$RequireMetrics -or $script:received -match 'B2METRIC END session='){
     $result.reason='Board reports closed; full Session extraction and PC verification pending';Receive-For 1;break
    }
   }
   if($script:received -match 'B2METRIC ERROR'){$result.reason='Metrics self-check failed; closed state and Session must be reviewed separately';break}
   if($timer.Elapsed.TotalSeconds -ge 20 -and !$result.stop_sent -and !$result.board_closed){$serial.Write("B2 STOP`r`n");Event 'tx' 'B2 STOP';$result.stop_sent=$true}
  }
  if(!$result.reason){$result.reason=if($result.board_closed){'Timed out waiting for complete measurement report; no repeat START'}else{'Timed out without closed/fault result; no repeat START'}}
 }
}catch{$result.reason=$_.Exception.Message;Event 'exception' $result.reason}
finally{
 if($serial.IsOpen){$serial.Close()};$serial.Dispose();$raw.Dispose();$events.Dispose()
 [IO.File]::WriteAllText((Join-Path $out 'serial.txt'),$script:received)
 $result|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $out 'result.json') -Encoding utf8
}
$result|ConvertTo-Json -Depth 5
$script:received
if(!$result.board_closed -or ($RequireMetrics -and $script:received -notmatch 'B2METRIC END session=')){exit 1}
