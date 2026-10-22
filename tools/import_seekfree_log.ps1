param([string]$InputLog, [string]$PythonPath, [string]$OutputRoot)
$ErrorActionPreference = 'Stop'
if (-not $InputLog) {
    Add-Type -AssemblyName System.Windows.Forms
    $taskDialog = [System.Windows.Forms.OpenFileDialog]::new()
    $taskDialog.Title = 'Select a saved SeekFree TEXT log (stop saving first)'
    $taskDialog.Filter = 'Serial logs (*.txt;*.log)|*.txt;*.log|All files (*.*)|*.*'
    try {
        if ($taskDialog.ShowDialog() -ne 'OK') { exit 0 }
        $InputLog = $taskDialog.FileName
    } finally { $taskDialog.Dispose() }
}
if (-not $PythonPath) {
    $taskBundledPython = Join-Path $env:USERPROFILE '.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
    if (Test-Path -LiteralPath $taskBundledPython) { $PythonPath = $taskBundledPython }
    else {
        $taskPythonCommand = Get-Command python -ErrorAction SilentlyContinue
        if (-not $taskPythonCommand) { throw 'Python 3.9+ required. Specify -PythonPath.' }
        $PythonPath = $taskPythonCommand.Source
    }
}
$taskArguments = @((Join-Path $PSScriptRoot 'import_seekfree_log.py'), $InputLog)
if ($OutputRoot) { $taskArguments += @('--output-root', $OutputRoot) }
& $PythonPath @taskArguments
if ($LASTEXITCODE -ne 0) { throw 'Log import failed; see the error above.' }
