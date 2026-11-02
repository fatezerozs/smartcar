Set-StrictMode -Version Latest
function Invoke-TestingPython {
    param([string]$Script, [string[]]$Arguments)
    $ErrorActionPreference = 'Stop'
    $python = Get-Command python -ErrorAction Stop
    & $python.Source -B (Join-Path $PSScriptRoot $Script) @Arguments | Out-Host
    return [int]$LASTEXITCODE
}
Export-ModuleMember -Function Invoke-TestingPython
