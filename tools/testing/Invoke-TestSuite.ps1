[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string[]]$Suite,
    [string]$Variant,
    [ValidateSet('msvc','gcc')][string]$Compiler='msvc',
    [ValidateSet('optimized','asan')][string]$Mode='optimized',
    [string]$VcVars,
    [string]$UV4,
    [string]$ArmBin
)
& (Join-Path $PSScriptRoot 'Invoke-TestPlan.ps1') @PSBoundParameters
exit $LASTEXITCODE
