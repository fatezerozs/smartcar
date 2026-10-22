<#
.SYNOPSIS
Capture stopped-car SeekFree V2 images, or decode an offline byte recording.
.EXAMPLE
.\capture_vision.ps1 -Port COM10 -Frames 0 -DurationSeconds 120 -Preview
#>
[CmdletBinding(DefaultParameterSetName='Serial')]
param(
    [Parameter(ParameterSetName='Serial')][string]$Port='COM10',
    [Parameter(Mandatory=$true,ParameterSetName='Replay')][string]$ReplayPath,
    [ValidateSet('g','b','o')][string]$Mode='g',
    [ValidateRange(0,1000000)][int]$Frames=1,
    [ValidateRange(1,86400)][int]$DurationSeconds=60,
    [ValidateRange(1,3600)][int]$FrameTimeoutSeconds=12,
    [string]$OutputDirectory=(Join-Path (Split-Path $PSScriptRoot -Parent) '.build\field'),
    [switch]$Preview
)
$ErrorActionPreference='Stop'
$taskIsReplay=$PSCmdlet.ParameterSetName -eq 'Replay'

# Retain partial headers between reads; consume a complete payload by length.
# Header-like bytes inside image pixels must not start another packet.
function Pop-VisionPacket {
    param([Collections.Generic.List[byte]]$Data,[byte[]]$Header,[hashtable]$Stats)
    $taskIndex=-1
    for($taskI=0;$taskI -le $Data.Count-$Header.Length;$taskI++) {
        $taskMatch=$true
        for($taskJ=0;$taskJ -lt $Header.Length;$taskJ++) {
            if($Data[$taskI+$taskJ] -ne $Header[$taskJ]){$taskMatch=$false;break}
        }
        if($taskMatch){$taskIndex=$taskI;break}
    }
    if($taskIndex -lt 0) {
        $taskDiscard=[Math]::Max(0,$Data.Count-($Header.Length-1))
        if($taskDiscard){$Data.RemoveRange(0,$taskDiscard);$Stats.discarded_bytes+=$taskDiscard}
        return $null
    }
    if($taskIndex){$Data.RemoveRange(0,$taskIndex);$Stats.discarded_bytes+=$taskIndex}
    if($Data.Count -lt 5652){return $null}
    $taskPayload=$Data.GetRange(12,5640).ToArray()
    $Data.RemoveRange(0,5652)
    return [pscustomobject]@{Payload=$taskPayload}
}

function Save-VisionImage {
    param([byte[]]$Pixels,[string]$Stem,[bool]$MakePreview)
    [IO.File]::WriteAllBytes($Stem+'.gray',$Pixels)
    $taskBitmap=[Drawing.Bitmap]::new(94,60)
    try {
        for($taskY=0;$taskY -lt 60;$taskY++) {
            for($taskX=0;$taskX -lt 94;$taskX++) {
                $taskV=[int]$Pixels[$taskY*94+$taskX]
                $taskBitmap.SetPixel($taskX,$taskY,[Drawing.Color]::FromArgb($taskV,$taskV,$taskV))
            }
        }
        $taskBitmap.Save($Stem+'.png',[Drawing.Imaging.ImageFormat]::Png)
        if($MakePreview) {
            $taskPreview=[Drawing.Bitmap]::new(752,480)
            $taskGraphics=$null
            try {
                $taskGraphics=[Drawing.Graphics]::FromImage($taskPreview)
                $taskGraphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
                $taskGraphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::Half
                $taskGraphics.DrawImage($taskBitmap,[Drawing.Rectangle]::new(0,0,752,480))
                $taskPreview.Save($Stem+'.preview.png',[Drawing.Imaging.ImageFormat]::Png)
            } finally {
                if($taskGraphics){$taskGraphics.Dispose()}
                $taskPreview.Dispose()
            }
        }
    } finally {$taskBitmap.Dispose()}
}

$taskSequence='camera-'+$Mode+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$taskRoot=Join-Path $OutputDirectory $taskSequence
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$taskRoot=(Resolve-Path -LiteralPath $taskRoot).Path
$taskData=[Collections.Generic.List[byte]]::new()
$taskHeader=[byte[]]@(0xAA,0x49,0x02,0x02,0x5E,0,0x3C,0,0x01,0,0,0)
$taskStats=@{received_bytes=0L;discarded_bytes=0L;frames=0}
$taskSerial=$null;$taskReplay=$null;$taskRaw=$null;$taskWatch=$null
$taskSession=[ordered]@{
    sequence=$taskSequence;source=$(if($taskIsReplay){'replay'}else{'serial'})
    port=$(if($taskIsReplay){$null}else{$Port});baud=115200;mode=$Mode
    width=94;height=60;requested_frames=$Frames;duration_limit_seconds=$DurationSeconds
    frame_timeout_seconds=$FrameTimeoutSeconds;started_at=[DateTimeOffset]::Now.ToString('o')
    timestamp_basis=$(if($taskIsReplay){'host replay decode; not original capture time'}else{'host complete-packet reception; not camera exposure time'})
    labels='Unlabelled. Review state/features separately; predictions are not ground truth.'
    protocol_integrity='Fixed header/length only; no checksum or frame counter. Corrupt payloads and lost frames cannot be reliably detected.'
    status='interrupted';error=$null
}
try {
    Add-Type -AssemblyName System.Drawing
    if($taskIsReplay) {
        $taskReplay=[IO.File]::OpenRead((Resolve-Path -LiteralPath $ReplayPath).Path)
    } else {
        $taskSerial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
        $taskSerial.DtrEnable=$false;$taskSerial.RtsEnable=$false
        $taskSerial.ReadTimeout=200;$taskSerial.WriteTimeout=300
        $taskSerial.Open()
        # Stop and claim the link before image mode. Never send arm/heartbeat.
        $taskSerial.Write("!`r`n")
        Start-Sleep -Milliseconds 250
        $taskSerial.DiscardInBuffer()
        $taskSerial.Write("x`r`n")
        $taskClaimText='';$taskClaimWatch=[Diagnostics.Stopwatch]::StartNew()
        while($taskClaimWatch.ElapsedMilliseconds -lt 2000) {
            $taskClaimText+=$taskSerial.ReadExisting()
            if($taskClaimText -match '\[LINK\] port=(usb|radio) owner=\1\b'){break}
            Start-Sleep -Milliseconds 10
        }
        $taskSession.link_response=$taskClaimText
        if($taskClaimText -notmatch '\[LINK\] port=(usb|radio) owner=\1\b') {
            throw 'Image capture port ownership not confirmed; no image mode requested.'
        }
        $taskSerial.Write("t`r`n")
        Start-Sleep -Milliseconds 250
        $taskSerial.DiscardInBuffer()
    }
    $taskRaw=[IO.File]::Open((Join-Path $taskRoot 'received.bin'),[IO.FileMode]::CreateNew)
    $taskWatch=[Diagnostics.Stopwatch]::StartNew()
    $taskLastFrameMs=0L
    $taskBuffer=[byte[]]::new(8192)
    if(-not $taskIsReplay){$taskSerial.Write($Mode)}
    while($true) {
        if($taskWatch.Elapsed.TotalSeconds -ge $DurationSeconds){$taskSession.status='duration_limit';break}
        $taskRead=0
        if($taskIsReplay) {
            $taskRead=$taskReplay.Read($taskBuffer,0,$taskBuffer.Length)
            if(-not $taskRead){$taskSession.status='replay_eof';break}
        } elseif($taskSerial.BytesToRead) {
            $taskRead=$taskSerial.Read($taskBuffer,0,[Math]::Min($taskSerial.BytesToRead,$taskBuffer.Length))
        }
        if($taskRead) {
            $taskRaw.Write($taskBuffer,0,$taskRead)
            $taskRaw.Flush()
            $taskStats.received_bytes+=$taskRead
            for($taskI=0;$taskI -lt $taskRead;$taskI++){$taskData.Add($taskBuffer[$taskI])}
        }
        while($null -ne ($taskPacket=Pop-VisionPacket -Data $taskData -Header $taskHeader -Stats $taskStats)) {
            $taskArrivalMs=$taskWatch.ElapsedMilliseconds
            $taskArrivalAt=[DateTimeOffset]::Now.ToString('o')
            $taskFrameNumber=$taskStats.frames+1
            $taskName='frame-{0:D6}' -f $taskFrameNumber
            $taskStem=Join-Path $taskRoot $taskName
            Save-VisionImage -Pixels $taskPacket.Payload -Stem $taskStem -MakePreview $Preview.IsPresent
            # Empty state/features: neither predicted nor invented labels.
            [pscustomobject][ordered]@{
                sequence=$taskSequence;frame=$taskFrameNumber;ms=$taskArrivalMs;host_received_at=$taskArrivalAt
                mode=$Mode;image=($taskName+'.png');gray=($taskName+'.gray')
                state='';heading='';curvature='';open_left='';open_right='';split='';stripes='';notes=''
            } | Export-Csv -LiteralPath (Join-Path $taskRoot 'frames.csv') -NoTypeInformation -Append -Encoding UTF8
            $taskStats.frames=$taskFrameNumber
            $taskLastFrameMs=$taskWatch.ElapsedMilliseconds
            Write-Output ('PNG='+$taskStem+'.png')
            Write-Output ('GRAY='+$taskStem+'.gray')
            if($Preview){Write-Output ('PREVIEW='+$taskStem+'.preview.png')}
            if($Frames -gt 0 -and $taskStats.frames -ge $Frames){break}
        }
        if($Frames -gt 0 -and $taskStats.frames -ge $Frames){$taskSession.status='frame_limit';break}
        if(-not $taskIsReplay -and $taskWatch.ElapsedMilliseconds-$taskLastFrameMs -ge $FrameTimeoutSeconds*1000) {
            throw "No complete 94x60 V2 frame for $FrameTimeoutSeconds seconds; check firmware/data link. Raw bytes retained."
        }
        if(-not $taskRead){Start-Sleep -Milliseconds 10}
    }
    if(-not $taskStats.frames){throw 'No complete 94x60 V2 camera packet; raw bytes retained.'}
} catch {
    $taskSession.status='error';$taskSession.error=$_.Exception.Message
    throw
} finally {
    # Restore text output and close the port; never resume driving.
    if($taskSerial) {
        try {if($taskSerial.IsOpen){try{$taskSerial.Write('t')}catch{};$taskSerial.Close()}}
        finally {$taskSerial.Dispose()}
    }
    if($taskReplay){$taskReplay.Dispose()}
    if($taskRaw){$taskRaw.Dispose()}
    $taskSession.ended_at=[DateTimeOffset]::Now.ToString('o')
    $taskSession.elapsed_ms=$(if($taskWatch){$taskWatch.ElapsedMilliseconds}else{0})
    $taskSession.frames=$taskStats.frames
    $taskSession.received_bytes=$taskStats.received_bytes
    $taskSession.discarded_bytes=$taskStats.discarded_bytes
    $taskSession.buffered_bytes=$taskData.Count
    $taskSession | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $taskRoot 'session.json') -Encoding UTF8
    Write-Output ('SESSION='+$taskRoot)
}
