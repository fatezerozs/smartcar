param(
    [string]$Port='COM10',
    [ValidateRange(1,1800)][int]$Seconds=600,
    [string]$OutputRoot=(Join-Path (Split-Path $PSScriptRoot -Parent) 'recordings')
)
# Receive only. Preserve image bytes as well as TEXT; never call SerialPort.Write.
$ErrorActionPreference='Stop'
$captureDirectory=Join-Path $OutputRoot ('wireless-'+(Get-Date -Format 'yyyyMMdd-HHmmss-ffffff'))
New-Item -ItemType Directory -Path $captureDirectory -ErrorAction Stop | Out-Null
$captureDataPath=Join-Path $captureDirectory 'rx.bin'
$captureTimesPath=Join-Path $captureDirectory 'chunks.csv'
$captureMetaPath=Join-Path $captureDirectory 'session.json'
$captureStopPath=Join-Path $captureDirectory 'STOP'
$capturePort=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$capturePort.DtrEnable=$false;$capturePort.RtsEnable=$false
$capturePort.ReadTimeout=200
$captureMeta=[ordered]@{port=$Port;baud=115200;receive_only=$true;started_utc=[DateTime]::UtcNow.ToString('o');state='opening';bytes=0;raw_file='rx.bin';chunk_times='chunks.csv';stop_file='STOP';requested_seconds=$Seconds;firmware_identity='unverified'}
$captureData=$null;$captureTimes=$null;$captureOffset=[long]0
$captureTimer=[Diagnostics.Stopwatch]::new();$captureLastFlush=0;$captureLastReport=0
function Save-CaptureMetadata {
    [IO.File]::WriteAllText($captureMetaPath,($captureMeta|ConvertTo-Json -Depth 4),[Text.UTF8Encoding]::new($false))
}
Save-CaptureMetadata
try {
    $capturePort.Open()
    $captureData=[IO.FileStream]::new($captureDataPath,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::ReadWrite)
    $captureTimes=[IO.StreamWriter]::new($captureTimesPath,$false,[Text.UTF8Encoding]::new($false))
    $captureTimes.WriteLine('host_utc,elapsed_ms,offset,bytes')
    $captureMeta.state='recording';Save-CaptureMetadata
    $captureTimer.Start();$captureBuffer=[byte[]]::new(16384)
    Write-Output ('LISTENING '+$Port+' RX ONLY: '+$captureDirectory)
    while($captureTimer.Elapsed.TotalSeconds -lt $Seconds -and -not(Test-Path -LiteralPath $captureStopPath)) {
        $captureWaiting=$capturePort.BytesToRead
        if($captureWaiting -gt 0) {
            $captureCount=$capturePort.Read($captureBuffer,0,[Math]::Min($captureWaiting,$captureBuffer.Length))
            $captureData.Write($captureBuffer,0,$captureCount)
            $captureTimes.WriteLine(('{0},{1},{2},{3}' -f [DateTime]::UtcNow.ToString('o'),$captureTimer.ElapsedMilliseconds,$captureOffset,$captureCount))
            $captureOffset+=$captureCount
        } else {Start-Sleep -Milliseconds 20}
        if($captureTimer.ElapsedMilliseconds-$captureLastFlush -ge 1000) {
            $captureData.Flush();$captureTimes.Flush();$captureLastFlush=$captureTimer.ElapsedMilliseconds
        }
        if($captureTimer.ElapsedMilliseconds-$captureLastReport -ge 10000) {
            Write-Output ('RX bytes='+$captureOffset+' elapsed_s='+[int]$captureTimer.Elapsed.TotalSeconds)
            $captureLastReport=$captureTimer.ElapsedMilliseconds
        }
    }
    $captureMeta.state='closed';$captureMeta.reason=if(Test-Path -LiteralPath $captureStopPath){'stop_requested'}else{'duration_elapsed'}
} catch {
    $captureMeta.state='failed';$captureMeta.error=$_.Exception.Message
    Write-Output ('CAPTURE ERROR: '+$captureMeta.error)
} finally {
    if($captureData){$captureData.Dispose()};if($captureTimes){$captureTimes.Dispose()}
    if($capturePort.IsOpen){$capturePort.Close()};$capturePort.Dispose()
    $captureMeta.bytes=$captureOffset;$captureMeta.ended_utc=[DateTime]::UtcNow.ToString('o')
    if(Test-Path -LiteralPath $captureDataPath){$captureMeta.sha256=(Get-FileHash -LiteralPath $captureDataPath -Algorithm SHA256).Hash}
    Save-CaptureMetadata
    Write-Output ('CAPTURE END state='+$captureMeta.state+' bytes='+$captureOffset+' path='+$captureDirectory)
}
if($captureMeta.state -eq 'failed'){exit 1}
