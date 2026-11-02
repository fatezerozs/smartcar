param(
    [Parameter(Mandatory=$true)][string]$Path,
    [Parameter(Mandatory=$true)][string]$Schema
)
$ErrorActionPreference = 'Stop'
try {
    if (-not (Test-Json -Json (Get-Content -LiteralPath $Path -Raw) -SchemaFile $Schema -ErrorAction Stop)) {
        exit 2
    }
} catch {
    [Console]::Error.WriteLine($_.ToString())
    exit 2
}
