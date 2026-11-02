$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'Test.Common.psm1') -Force
$code=Invoke-TestingPython -Script 'run_tests.py' -Arguments @('--validate')
exit $code
