[CmdletBinding()]
param(
    [ValidateSet('Host','PreFlash','Diagnostic')][string]$Profile='Host',
    [string[]]$Suite,
    [string]$Variant,
    [switch]$List,
    [ValidateSet('msvc','gcc')][string]$Compiler='msvc',
    [ValidateSet('optimized','asan')][string]$Mode='optimized',
    [ValidateRange(1,4)][int]$Jobs=1,
    [string]$VcVars,
    [string]$UV4,
    [string]$ArmBin
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Test.Common.psm1') -Force
$arguments=@('--profile',$Profile,'--compiler',$Compiler,'--mode',$Mode,'--jobs',[string]$Jobs)
foreach($name in $Suite) { foreach($id in $name.Split(',')) { $arguments+=@('--suite',$id.Trim()) } }
if($Variant) { $arguments+=@('--variant',$Variant) }
if($List) { $arguments+='--list' }
if($VcVars) { $arguments+=@('--vcvars',$VcVars) }
if($UV4) { $arguments+=@('--uv4',$UV4) }
if($ArmBin) { $arguments+=@('--arm-bin',$ArmBin) }
$code=Invoke-TestingPython -Script 'run_tests.py' -Arguments $arguments
exit $code
