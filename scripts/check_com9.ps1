$ErrorActionPreference = "Stop"

$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
if (-not (Test-Path -LiteralPath $pio)) {
    throw "PlatformIO was not found at $pio"
}

$deviceOutput = & $pio device list
$deviceOutput
if ($deviceOutput -notmatch "(?m)^COM9$") {
    throw "COM9 is not listed by PlatformIO."
}

Write-Host "COM9 is present."
