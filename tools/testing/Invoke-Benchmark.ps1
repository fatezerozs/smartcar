param(
    [string]$Manifest='tests/benchmarks/manifests/vision-host.json',
    [string]$Measurements,
    [string]$Baseline
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Test.Common.psm1') -Force
$arguments=@('--manifest',$Manifest)
if($Measurements) { $arguments+=@('--measurements',$Measurements) }
if($Baseline) { $arguments+=@('--baseline',$Baseline) }
$code=Invoke-TestingPython -Script 'benchmark.py' -Arguments $arguments
exit $code
