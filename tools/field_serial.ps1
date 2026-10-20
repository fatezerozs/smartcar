param(
    [string]$Port,
    [ValidateRange(1,120)][int]$Seconds = 6,
    [switch]$TextMode,
    [string[]]$Command = @(),
    [ValidateRange(0,60)][int]$RunSeconds = 0,
    [switch]$TrackReady,
    [switch]$ServoReady,
    [switch]$MotorReady,
    [switch]$RequireMotorGate,
    [switch]$SaveForAutostart,
    [string]$LogFile,
    [switch]$ListPorts
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'field_protocol.ps1')
$taskPorts = @([System.IO.Ports.SerialPort]::GetPortNames())
if ($ListPorts) { $taskPorts; exit 0 }
if (-not $Port) {
    if ($taskPorts.Count -ne 1) { throw ('Specify -Port; detected: ' + ($taskPorts -join ', ')) }
    $Port = $taskPorts[0]
}
if ($RunSeconds -gt 0 -and -not $TrackReady) {
    throw 'Motion requires explicit -TrackReady after the operator confirms wheels/track and stop access.'
}
# Only framed configuration/query commands are accepted here; standalone a/h
# cannot be smuggled through a parameter string. Firmware validates full ranges.
foreach ($taskCommand in $Command) {
    $taskValid = Test-FieldCommand $taskCommand
    if($taskCommand -eq '@save' -and -not $SaveForAutostart){throw 'Persistent save requires -SaveForAutostart: saved settings may launch next boot when C12 is enabled.'}
    if (-not $taskValid -or $taskCommand.Length -gt 94) { throw ('Invalid field command: ' + $taskCommand) }
    if ($taskCommand.StartsWith('@probe ') -and (-not $ServoReady -or $Command.Count -ne 1 -or $RunSeconds)) {
        throw 'Servo motion requires explicit -ServoReady, exactly one probe command, and no motor run.'
    }
    if ($taskCommand.StartsWith('@wheel ') -and (-not $MotorReady -or $Command.Count -ne 1 -or $RunSeconds)) {
        throw 'Wheel motion requires explicit -MotorReady, exactly one wheel command, and no driving run.'
    }
}
if (-not $LogFile) {
    $taskLogRoot = Join-Path (Split-Path $PSScriptRoot -Parent) '.build\field'
    New-Item -ItemType Directory -Path $taskLogRoot -Force | Out-Null
    $LogFile = Join-Path $taskLogRoot ('serial-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
}
$taskSerial = [System.IO.Ports.SerialPort]::new($Port,115200,[System.IO.Ports.Parity]::None,8,[System.IO.Ports.StopBits]::One)
$taskSerial.DtrEnable = $false
$taskSerial.RtsEnable = $false
$taskSerial.ReadTimeout = 200
$taskSerial.WriteTimeout = 200
$taskStream = [IO.FileStream]::new($LogFile,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
$taskWriter = [IO.StreamWriter]::new($taskStream,[Text.UTF8Encoding]::new($false))
$taskWriter.AutoFlush = $true
$taskTranscript = [Text.StringBuilder]::new()
$taskRunStarted = $false
$taskProbeSent = $false

function Receive-FieldSlice([int]$DurationMs) {
    $taskWatch = [Diagnostics.Stopwatch]::StartNew()
    while ($taskWatch.ElapsedMilliseconds -lt $DurationMs) {
        $taskChunk = $taskSerial.ReadExisting()
        if ($taskChunk) {
            $taskWriter.Write($taskChunk)
            [void]$taskTranscript.Append($taskChunk)
            # Avoid unbounded memory in a long debug session; full log stays on disk.
            if ($taskTranscript.Length -gt 65536) { [void]$taskTranscript.Remove(0,32768) }
            Write-Output $taskChunk
        }
        Start-Sleep -Milliseconds 10
    }
}
try {
    $taskSerial.Open()
    $taskWriter.WriteLine('# port=' + $Port + ' baud=115200 host_time=' + (Get-Date -Format o))
    if ($TextMode -or $Command.Count -or $RunSeconds) {
        # Legacy firmware also understands STOP/help/text. No a is sent during
        # discovery; @ strings are forbidden until the new build identifies itself.
        # ! cancels an incomplete previous @ line; ordinary s inside that line
        # would be consumed as payload. CRLF supplies a redundant terminator,
        # not a duplicate action: the parser consumes only the first newline.
        $taskSerial.Write("!s`r`n")
        Receive-FieldSlice 300
        $taskOffset=$taskTranscript.Length
        $taskSerial.Write("x`r`n")
        Receive-FieldSlice 1000
        if($taskTranscript.ToString().Substring($taskOffset) -notmatch '\[LINK\] port=(usb|radio) owner=\1\b') {
            throw 'Current port ownership not confirmed; configuration and motion were NOT sent.'
        }
        $taskSerial.Write("?t`r`n")
        Receive-FieldSlice 1000
    }
    if ($Command.Count -or $RunSeconds) {
        if ($taskTranscript.ToString() -notmatch ('\[FIELD\] build='+$fieldVersionPattern+';')) {
            throw 'New firmware marker absent; framed configuration and motion were NOT sent.'
        }
        if ($RequireMotorGate -and $taskTranscript.ToString() -notmatch '\[WHEEL\] @wheel channel permille level hold_ms') {
            throw 'Independent motor confirmation requires FIELD_2 or later; parameters and motion were NOT sent.'
        }
        foreach ($taskCommand in $Command) {
            $taskProbe=$taskCommand.StartsWith('@probe ')
            $taskWheel=$taskCommand.StartsWith('@wheel ')
            if (($taskCommand -in @('@save','@load') -or $taskCommand.StartsWith('@startup ')) -and
                $taskTranscript.ToString() -notmatch '\[STARTUP\] @startup enabled delay_ms') {
                throw 'Firmware lacks persistent startup support; command was NOT sent.'
            }
            if ($taskCommand.StartsWith('@balance ') -and
                $taskTranscript.ToString() -notmatch '\[OPENLOOP\] @balance motor1_scale motor2_scale') {
                throw 'Firmware lacks motor balance support; command was NOT sent.'
            }
            if (($taskWheel -or $taskCommand.StartsWith('@motor_dir ')) -and
                $taskTranscript.ToString() -notmatch '\[WHEEL\] @wheel channel permille level hold_ms') {
                throw 'Firmware lacks independent wheel support; command was NOT sent.'
            }
            if ($taskProbe -and $taskTranscript.ToString() -notmatch '\[PROBE\] @probe pulse_us hold_ms') {
                throw 'Firmware lacks servo-only probe support; motion command was NOT sent.'
            }
            if ($taskCommand.StartsWith('@bands ') -and $taskTranscript.ToString() -notmatch '@bands near_y far_y half') {
                throw 'Firmware lacks @bands support; command was NOT sent. Flash the updated build first.'
            }
            $taskOffset = $taskTranscript.Length
            $taskWriter.WriteLine('# TX ' + $taskCommand)
            if ($taskProbe -or $taskWheel) { $taskProbeSent = $true }
            $taskSerial.Write($taskCommand + "`r`n")
            # Allow this one bounded pulse to finish before @status stops it.
            # Operator confirmation is mandatory; no automatic multi-step sweep.
            if ($taskProbe -or $taskWheel) { Receive-FieldSlice 1150 } else { Receive-FieldSlice 1000 }
            $taskReply = $taskTranscript.ToString().Substring($taskOffset)
            if (-not (Test-FieldReply $taskCommand $taskReply)) {
                throw ('Missing/rejected configuration acknowledgement: ' + $taskCommand)
            }
        }
        $taskOffset = $taskTranscript.Length
        $taskSerial.Write("@status`r`n")
        Receive-FieldSlice 1000
        if (-not (Test-FieldReply '@status' $taskTranscript.ToString().Substring($taskOffset))) {
            throw 'Missing final status readback; no further motion is sent.'
        }
    }
    if ($RunSeconds -gt 0) {
        $taskStatuses = [regex]::Matches($taskTranscript.ToString(),('\[FIELD\] version='+$fieldVersionPattern+' [^\r\n]*'))
        if ($taskStatuses.Count -eq 0) { throw 'No configuration readback; refusing arm.' }
        $taskStatus = $taskStatuses[$taskStatuses.Count-1].Value
        if ($taskStatus -notmatch '\bready=1\b') { throw 'Calibration/PWM profile is not ready; refusing arm.' }
        $taskLimitMatch = [regex]::Match($taskStatus,'\blimit_ms=(\d+)\b')
        if (-not $taskLimitMatch.Success -or $RunSeconds*1000 -gt [int]$taskLimitMatch.Groups[1].Value) {
            throw 'Requested run exceeds the reported firmware lease.'
        }
        # @status stopped the vehicle; wait for fresh frames before explicit arm.
        Receive-FieldSlice 300
        $taskRunStarted = $true
        $taskSerial.Write('a')
        $taskRunWatch = [Diagnostics.Stopwatch]::StartNew()
        while ($taskRunWatch.ElapsedMilliseconds -lt $RunSeconds*1000) {
            $taskSerial.Write('h')
            Receive-FieldSlice 100
        }
        $taskSerial.Write('!s')
        $taskRunStarted = $false
        Receive-FieldSlice 300
    } else {
        Receive-FieldSlice ($Seconds*1000)
    }
} finally {
    if ($taskSerial.IsOpen) {
        if ($taskRunStarted -or $taskProbeSent) { try { $taskSerial.Write('!s') } catch {} }
        $taskSerial.Close()
    }
    $taskSerial.Dispose()
    $taskWriter.Dispose()
    Write-Output ('Log: ' + $LogFile)
}
