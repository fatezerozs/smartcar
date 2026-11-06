param(
    [string]$Uv4 = 'E:\ElecTronic\Keil\UV4\UV4.exe',
    [switch]$MotionReviewed,
    [ValidateRange(10,1800)][int]$BuildTimeoutSeconds = 180
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not (Test-Path -LiteralPath $Uv4)) { throw "Keil not found: $Uv4" }
$buildRoot = Join-Path $repoRoot '.build\recorder-train'
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$buildLock = $null
$buildProcess = $null
Push-Location $repoRoot
try {
    # Serialize preparation and compilation together; parallel calls otherwise
    # race over SDK copies, the target, Objects, and build-evidence.json.
    try {
        $buildLock = [System.IO.File]::Open((Join-Path $buildRoot 'build.lock'), [System.IO.FileMode]::OpenOrCreate, [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    } catch {
        throw 'Cannot acquire recorder build lock; another invocation may be active. No build was started.'
    }
    $prepareArgs = @('tools/prepare_recorder.py')
    if ($MotionReviewed) { $prepareArgs += '--motion-reviewed' }
    & python @prepareArgs
    if ($LASTEXITCODE -ne 0) { throw 'Recorder preparation failed' }
    $projectPath = Join-Path $repoRoot 'demo\mdk\training\recorder_train.uvprojx'
    $logPath = Join-Path $buildRoot 'build.log'
    # A unique log prevents a failed Keil launch from reusing yesterday's PASS.
    $runLogPath = Join-Path $buildRoot ('build-' + (Get-Date -Format 'yyyyMMddTHHmmssfff') + '-' + $PID + '.log')
    $buildProcess = Start-Process -FilePath $Uv4 -ArgumentList @('-b', ('"'+$projectPath+'"'), '-j0', '-o', ('"'+$runLogPath+'"')) -WindowStyle Hidden -PassThru
    $buildTimer = [System.Diagnostics.Stopwatch]::StartNew()
    while (-not $buildProcess.WaitForExit(1000)) {
        if ($buildTimer.Elapsed.TotalSeconds -ge $BuildTimeoutSeconds) {
            throw "Recorder build timed out after $BuildTimeoutSeconds seconds; log=$runLogPath"
        }
    }
    $buildProcess.Refresh()
    if (-not (Test-Path -LiteralPath $runLogPath)) { throw "Keil did not create this run's log: $runLogPath" }
    $logText = Get-Content -LiteralPath $runLogPath -Raw
    Copy-Item -LiteralPath $runLogPath -Destination $logPath -Force
    Write-Output ($logText -split "`r?`n" | Select-Object -Last 12)
    if ($buildProcess.ExitCode -ne 0 -or $logText -notmatch '0 Error\(s\), 0 Warning\(s\)') {
        throw "Keil recorder build did not pass: exit=$($buildProcess.ExitCode), log=$logPath"
    }
    $elfPath = Join-Path $buildRoot 'Objects\recorder_train.axf'
    $binPath = Join-Path $buildRoot 'Objects\recorder_train.bin'
    $fromElf = Join-Path (Split-Path (Split-Path $Uv4 -Parent) -Parent) 'ARM\ARMCLANG\bin\fromelf.exe'
    & $fromElf '--bincombined' '--output' $binPath $elfPath
    if ($LASTEXITCODE -ne 0) { throw 'fromelf conversion failed' }
    $sources = Get-Content (Join-Path $buildRoot 'adapted\provenance.json') -Raw | ConvertFrom-Json
    $mapJson = & python tools/prepare_recorder.py --check-map (Join-Path $buildRoot 'Listings\recorder_train.map')
    if ($LASTEXITCODE -ne 0) { throw 'Recorder map isolation check failed' }
    $mapEvidence = $mapJson | ConvertFrom-Json
    $record = [ordered]@{
        build = 'RECORDER_TRAIN_1'; training_only = $true; motion_reviewed = [bool]$MotionReviewed
        git_head = $sources.git_head; git_dirty = $sources.git_dirty
        input_tree_sha256 = $sources.firmware_input_tree_sha256
        axf_sha256 = (Get-FileHash -LiteralPath $elfPath -Algorithm SHA256).Hash.ToLower()
        bin_sha256 = (Get-FileHash -LiteralPath $binPath -Algorithm SHA256).Hash.ToLower()
        log_sha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash.ToLower()
        built_at = (Get-Date).ToString('o'); hardware_validated = $false; flashed = $false
        map = $mapEvidence
    }
    $record | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $buildRoot 'build-evidence.json') -Encoding utf8
    Write-Output "PASS training build; motion_reviewed=$MotionReviewed; no flash/hardware operation."
} finally {
    # Only clean up the process tree started by this invocation, including on
    # cancellation/timeout. Never close other Keil IDE sessions by process name.
    try {
        if ($null -ne $buildProcess) {
            try {
                if (-not $buildProcess.HasExited) {
                    & "$env:SystemRoot\System32\taskkill.exe" /PID ([string]$buildProcess.Id) /T /F | Out-Null
                    if (-not $buildProcess.WaitForExit(5000)) { Write-Warning 'Owned Keil process did not exit after termination.' }
                }
            } finally { $buildProcess.Dispose() }
        }
    } finally {
        if ($null -ne $buildLock) { $buildLock.Dispose() }
        Pop-Location
    }
}
