# Use only after a successful field_serial preparation in this powered session.
# Does not configure, query status, capture images, or retry arm.
param([string]$Port='COM15',[switch]$TrackReady,
      [ValidateRange(1,55)][int]$RunSeconds=5,[switch]$PreparedRoute)
$ErrorActionPreference='Stop'
if(-not $TrackReady){throw 'Explicit current track-ready instruction required.'}
if($RunSeconds -gt 5 -and -not $PreparedRoute){throw 'Long runs require the prepared MIDTERM_ROUTE_TRAIN_1 firmware (60s backstop).'}
$taskRunLimitMs=$RunSeconds*1000
$taskLogRoot=Join-Path (Split-Path $PSScriptRoot -Parent) '.build\field'
$taskLog=Join-Path $taskLogRoot ('quick-run-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.log')
$taskWriter=[IO.StreamWriter]::new($taskLog,$false,[Text.UTF8Encoding]::new($false))
$taskWriter.AutoFlush=$true
$taskSerial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$taskSerial.DtrEnable=$false;$taskSerial.RtsEnable=$false;$taskSerial.WriteTimeout=200
$taskReply=[Text.StringBuilder]::new()
function Receive-Quick {
    $taskChunk=$taskSerial.ReadExisting()
    if($taskChunk){$taskWriter.Write($taskChunk);[void]$taskReply.Append($taskChunk)}
}
try {
    $taskSerial.Open()
    $taskWriter.WriteLine('# prepared-session run_seconds='+$RunSeconds+' port='+$Port+' time='+(Get-Date -Format o))
    $taskSerial.DiscardInBuffer()
    $taskSerial.Write("!s`r`nx`r`n")
    $taskClaimWatch=[Diagnostics.Stopwatch]::StartNew()
    while($taskReply.ToString() -notmatch '\[LINK\] port=(usb|radio) owner=\1\b') {
        Receive-Quick
        if($taskClaimWatch.ElapsedMilliseconds -ge 1000){throw 'No current link ownership; arm not sent.'}
        Start-Sleep -Milliseconds 5
    }
    $taskWriter.WriteLine('# TX a; then h every 100ms; maximum_ms='+$taskRunLimitMs)
    $taskSerial.Write('ah')
    $taskRunWatch=[Diagnostics.Stopwatch]::StartNew();$taskNextHeartbeat=100
    while($taskRunWatch.ElapsedMilliseconds -lt $taskRunLimitMs) {
        if($taskRunWatch.ElapsedMilliseconds -ge $taskNextHeartbeat){
            $taskSerial.Write('h');$taskNextHeartbeat=$taskRunWatch.ElapsedMilliseconds+100
        }
        Receive-Quick
        if($taskRunWatch.ElapsedMilliseconds -ge 500 -and $taskReply.ToString() -notmatch '\[RUN\] n=\d+ ms=\d+'){throw 'No RUN confirmation within 500ms; stopping without retry.'}
        Start-Sleep -Milliseconds 5
    }
} finally {
    if($taskSerial.IsOpen){
        try {
            $taskSerial.Write("!s`r`n")
            $taskStopWatch=[Diagnostics.Stopwatch]::StartNew()
            while($taskStopWatch.ElapsedMilliseconds -lt 350){Receive-Quick;Start-Sleep -Milliseconds 5}
        } finally {$taskSerial.Close()}
    }
    $taskSerial.Dispose();$taskWriter.Dispose()
    Write-Output ('Log: '+$taskLog)
}
