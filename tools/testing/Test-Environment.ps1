param(
    [ValidateSet('msvc','gcc')][string]$Compiler='msvc',
    [string]$VcVars,
    [string]$UV4,
    [string]$ArmBin,
    [switch]$Target
)
$arguments=@{Suite=@('compiler-environment');Compiler=$Compiler}
foreach($name in @('VcVars','UV4','ArmBin')) { if($PSBoundParameters.ContainsKey($name)) { $arguments[$name]=$PSBoundParameters[$name] } }
if($Target) { $arguments.Suite+=@('target-environment','arm-environment') }
& (Join-Path $PSScriptRoot 'Invoke-TestPlan.ps1') @arguments
exit $LASTEXITCODE
