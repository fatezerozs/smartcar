param([Parameter(Mandatory=$true)][string]$Port, [ValidateSet('g','b','o')][string]$Mode='g')
$ErrorActionPreference='Stop'
$taskRoot=Join-Path (Split-Path $PSScriptRoot -Parent) '.build\field'
New-Item -ItemType Directory -Path $taskRoot -Force | Out-Null
$taskStem=Join-Path $taskRoot ('camera-'+$Mode+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$taskSerial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
$taskSerial.DtrEnable=$false
$taskSerial.RtsEnable=$false
$taskSerial.ReadTimeout=200
$taskSerial.WriteTimeout=300
$taskData=[Collections.Generic.List[byte]]::new()
$taskHeader=[byte[]]@(0xAA,0x49,0x02,0x02,0x5E,0,0x3C,0,0x01,0,0,0)
$taskPayload=$null
try {
    $taskSerial.Open()
    # Image entry must be preceded by STOP. No arm/heartbeat is ever transmitted.
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
    if($taskClaimText -notmatch '\[LINK\] port=(usb|radio) owner=\1\b'){throw 'Image capture port ownership not confirmed; no image mode requested.'}
    $taskSerial.Write("t`r`n")
    Start-Sleep -Milliseconds 250
    $taskSerial.DiscardInBuffer()
    $taskSerial.Write($Mode)
    $taskWatch=[Diagnostics.Stopwatch]::StartNew()
    $taskBuffer=[byte[]]::new(8192)
    while ($taskWatch.ElapsedMilliseconds -lt 12000 -and -not $taskPayload) {
        $taskAvailable=$taskSerial.BytesToRead
        if ($taskAvailable) {
            $taskRead=$taskSerial.Read($taskBuffer,0,[Math]::Min($taskAvailable,$taskBuffer.Length))
            for($taskI=0;$taskI -lt $taskRead;$taskI++){ $taskData.Add($taskBuffer[$taskI]) }
            if($taskData.Count -gt 65536){ throw 'No complete V2 packet within buffer bound.' }
            for($taskI=0;$taskI -le $taskData.Count-5652;$taskI++){
                $taskMatch=$true
                for($taskJ=0;$taskJ -lt 12;$taskJ++){
                    if($taskData[$taskI+$taskJ] -ne $taskHeader[$taskJ]){$taskMatch=$false;break}
                }
                if($taskMatch){$taskPayload=$taskData.GetRange($taskI+12,5640).ToArray();break}
            }
        }
        Start-Sleep -Milliseconds 10
    }
    if(-not $taskPayload){throw 'No complete 94x60 V2 camera packet. Confirm running firmware and data link.'}
    # Generated capture artifacts, not source edits. Preserve exact 5640 pixels.
    [IO.File]::WriteAllBytes($taskStem+'.gray',$taskPayload)
    Add-Type -AssemblyName System.Drawing
    $taskBitmap=[Drawing.Bitmap]::new(94,60)
    try {
        for($taskY=0;$taskY -lt 60;$taskY++){
            for($taskX=0;$taskX -lt 94;$taskX++){
                $taskV=[int]$taskPayload[$taskY*94+$taskX]
                $taskBitmap.SetPixel($taskX,$taskY,[Drawing.Color]::FromArgb($taskV,$taskV,$taskV))
            }
        }
        $taskBitmap.Save($taskStem+'.png',[Drawing.Imaging.ImageFormat]::Png)
        # Nearest-neighbor preview only; the .gray and original PNG remain 94x60.
        $taskPreview=[Drawing.Bitmap]::new(752,480)
        $taskGraphics=[Drawing.Graphics]::FromImage($taskPreview)
        try {
            $taskGraphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
            $taskGraphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::Half
            $taskGraphics.DrawImage($taskBitmap,[Drawing.Rectangle]::new(0,0,752,480))
            $taskPreview.Save($taskStem+'.preview.png',[Drawing.Imaging.ImageFormat]::Png)
        } finally { $taskGraphics.Dispose(); $taskPreview.Dispose() }
    } finally { $taskBitmap.Dispose() }
    Write-Output ('PNG='+$taskStem+'.png')
    Write-Output ('GRAY='+$taskStem+'.gray')
    Write-Output ('PREVIEW='+$taskStem+'.preview.png')
} finally {
    if($taskSerial.IsOpen){try{$taskSerial.Write('t')}catch{};$taskSerial.Close()}
    $taskSerial.Dispose()
}
