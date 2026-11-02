param([Parameter(Mandatory=$true)][string]$RunJson)
$ErrorActionPreference='Stop'
$python=Get-Command python -ErrorAction Stop
& $python.Source -B -c 'import sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); from reporter import write_report; write_report(Path(sys.argv[2]))' $PSScriptRoot $RunJson
exit $LASTEXITCODE
