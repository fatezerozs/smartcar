param(
    [ValidateSet('line','shuttle','circle')][string]$Mode='line',
    [string]$Port,
    [ValidateSet('usb','radio')][string]$Role='radio',
    [ValidateRange(1,150)][int]$DutyPermille=30,
    [ValidateRange(100,3000)][int]$DurationMs=500,
    [ValidateRange(0,2500)][int]$ServoUs=0,
    [ValidateRange(0,10000)][int]$StopAfterMs=0,
    [ValidateRange(0,10000)][int]$DropHeartbeatAfterMs=0,
    [switch]$ExternalStop,
    [switch]$Execute
)
$ErrorActionPreference='Stop'
if(($StopAfterMs -or $ExternalStop) -and $DropHeartbeatAfterMs){throw 'Choose STOP or heartbeat-drop, not both.'}
if($Mode -ne 'shuttle' -and $StopAfterMs -and $StopAfterMs -ge $DurationMs+200){throw 'STOP must occur before the timed leg ends (duration + 200ms steering settle).'}
if($Mode -ne 'shuttle' -and $DropHeartbeatAfterMs -and $DropHeartbeatAfterMs+500 -ge $DurationMs+200){throw 'Heartbeat expiry must precede the timed leg end; increase DurationMs.'}
if($Mode -eq 'circle' -and $ServoUs -lt 500){throw 'Circle requires a measured -ServoUs within the calibrated window.'}
$taskCommand='@motion '+$Mode+' '+$DutyPermille+' '+$DurationMs
if($Mode -eq 'circle'){$taskCommand+=' '+$ServoUs}
Write-Output ('PLAN: '+$taskCommand+'; heartbeat every 100ms; STOP='+$StopAfterMs+'ms; external STOP='+$ExternalStop+'; drop heartbeat='+$DropHeartbeatAfterMs+'ms')
if(-not $Execute){Write-Output 'DRY RUN. No port opened. Confirm directions, servo, cap and clear test area before using -Execute.';return}
if(-not $Port){throw '-Port is required for execution; use the actual USB/radio COM port.'}
if($Mode -eq 'shuttle' -and [Console]::IsInputRedirected){throw 'Shuttle needs an interactive console: observe a complete stop, then press R after the coast interval.'}
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskLogRoot=Join-Path $taskRoot '.build\motion-runs'
New-Item -ItemType Directory -Force -Path $taskLogRoot | Out-Null
$taskLog=Join-Path $taskLogRoot ((Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'.jsonl')
$taskSerial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$taskSerial.Handshake=[IO.Ports.Handshake]::None
$taskSerial.WriteTimeout=500
$taskSerial.DtrEnable=$false;$taskSerial.RtsEnable=$false
$taskWatch=[Diagnostics.Stopwatch]::StartNew()
$taskBuffer='';$taskStarted=$false;$taskDone=$false;$taskStopSent=$false
$taskPhase=-1;$taskReason=-1;$taskWaitSeen=-1L;$taskLastHeartbeat=-100L
$taskReverseSent=$false
function Write-MotionLog([string]$direction,[string]$value) {
    $taskRecord=[ordered]@{time=(Get-Date -Format o);elapsed_ms=$taskWatch.ElapsedMilliseconds;direction=$direction;data=$value}
    Add-Content -LiteralPath $taskLog -Value ($taskRecord|ConvertTo-Json -Compress) -Encoding utf8
}
function Send-MotionText([string]$value) {
    $taskSerial.Write($value);Write-MotionLog 'TX' $value
}
function Read-MotionText {
    $taskChunk=$taskSerial.ReadExisting()
    if($taskChunk){Write-MotionLog 'RX' $taskChunk;Write-Host -NoNewline $taskChunk}
    return $taskChunk
}
try {
    $taskSerial.Open()
    # Stop and synchronize before claiming; never assume the port was idle.
    Send-MotionText "`r`n!`r`n"
    $taskDrain=[Diagnostics.Stopwatch]::StartNew()
    while($taskDrain.ElapsedMilliseconds -lt 400){$null=Read-MotionText;Start-Sleep -Milliseconds 10}
    Send-MotionText "x`r`n"
    $taskClaim=[Diagnostics.Stopwatch]::StartNew();$taskClaimText=''
    while($taskClaim.ElapsedMilliseconds -lt 2000) {
        $taskClaimText+=Read-MotionText
        if($taskClaimText -match ('\[LINK\] port='+$Role+' owner='+$Role)){break}
        Start-Sleep -Milliseconds 10
    }
    if($taskClaimText -notmatch ('\[LINK\] port='+$Role+' owner='+$Role)){throw 'Control claim not confirmed. Motion was not requested.'}
    $taskWatch.Restart()
    Send-MotionText ($taskCommand+"`r`n") # Exactly once; never auto-retry a motion command.
    $taskLimit=if($Mode -eq 'shuttle'){2*$DurationMs+12000}else{$DurationMs+2000}
    while($taskWatch.ElapsedMilliseconds -lt $taskLimit) {
        $taskNow=$taskWatch.ElapsedMilliseconds
        if($StopAfterMs -and -not $taskStopSent -and $taskNow -ge $StopAfterMs) {
            Send-MotionText "!`r`n";$taskStopSent=$true # Stop heartbeats too if STOP delivery fails.
        }
        if(-not $taskStopSent -and (-not $DropHeartbeatAfterMs -or $taskNow -lt $DropHeartbeatAfterMs) -and
           $taskNow-$taskLastHeartbeat -ge 100) {
            Send-MotionText 'h';$taskLastHeartbeat=$taskNow
        }
        $taskBuffer+=Read-MotionText
        if($taskBuffer -match '\[MOTION\] REJECT'){throw 'Firmware rejected the request; inspect profile, mode, limits and coast.'}
        if(-not $taskStarted) {
            $taskAck=[regex]::Match($taskBuffer,'\[MOTION\] STARTED')
            if($taskAck.Success){$taskStarted=$true;$taskBuffer=$taskBuffer.Substring($taskAck.Index)}
        }
        if($taskStarted) {
            $taskMatches=[regex]::Matches($taskBuffer,'\[MOTION\] seq=\d+ kind=\d+ phase=(\d+) reason=(\d+)')
            if($taskMatches.Count) {
                $taskLatest=$taskMatches[$taskMatches.Count-1]
                $taskPhase=[int]$taskLatest.Groups[1].Value;$taskReason=[int]$taskLatest.Groups[2].Value
                if($taskPhase -eq 0){$taskDone=$true;break}
                if($taskPhase -eq 3 -and $taskWaitSeen -lt 0 -and -not $taskReverseSent) {
                    $taskWaitSeen=$taskNow
                    Write-Host 'FORWARD ENDED. Observe a complete mechanical stop; after 2 seconds press R once to permit reverse. ESC stops.'
                }
            }
        } elseif($taskNow -gt 1500){throw 'STARTED acknowledgement not received; stopping without retry.'}
        if(-not [Console]::IsInputRedirected -and [Console]::KeyAvailable) {
            $taskKey=[Console]::ReadKey($true).Key
            if($taskKey -eq [ConsoleKey]::Escape){Send-MotionText "!`r`n";$taskStopSent=$true}
            if($taskKey -eq [ConsoleKey]::R -and $Mode -eq 'shuttle') {
                if($taskPhase -eq 3 -and $taskWaitSeen -ge 0 -and $taskNow-$taskWaitSeen -ge 2000 -and -not $taskReverseSent) {
                    Send-MotionText 'v';$taskReverseSent=$true
                } else {Write-Host 'Reverse not requested: wait for phase=3 and at least 2 seconds, then confirm the car is stationary.'}
            }
        }
        if($taskBuffer.Length -gt 16000){$taskBuffer=$taskBuffer.Substring($taskBuffer.Length-8000)}
        Start-Sleep -Milliseconds 10
    }
    if(-not $taskDone){throw 'Final stopped-state telemetry not confirmed; do not retry until physical state is checked.'}
    $taskExpected=if($StopAfterMs -or $ExternalStop){2}elseif($DropHeartbeatAfterMs){3}else{1}
    Write-MotionLog 'RESULT' ('phase='+$taskPhase+' reason='+$taskReason+' expected='+$taskExpected)
    if($taskReason -ne $taskExpected){throw ('Unexpected stop reason '+$taskReason+'; expected '+$taskExpected)}
    Write-Output ('FIRMWARE STOP REPORT CONFIRMED: reason='+$taskReason+'. Measure physical stop separately. Log: '+$taskLog)
} finally {
    if($taskSerial.IsOpen) {
        try {Send-MotionText "!`r`n"} finally {$taskSerial.Close()}
    }
    $taskSerial.Dispose()
}
