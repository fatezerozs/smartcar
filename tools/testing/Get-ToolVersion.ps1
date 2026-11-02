param([Parameter(Mandatory=$true)][string]$Path)
$ErrorActionPreference='Stop'
(Get-Item -LiteralPath $Path).VersionInfo.FileVersion
