# Dot-source from the repository root: . ./tools/activate_toolchain.ps1
$faultlineRoot = Split-Path -Parent $PSScriptRoot
$faultlinePaths = @(
    (Join-Path $faultlineRoot '.tools/w64devkit/bin'),
    (Join-Path $faultlineRoot '.tools/python/cmake/data/bin'),
    (Join-Path $faultlineRoot '.tools/python/ninja-1.13.2.data/scripts')
)
foreach ($faultlinePath in $faultlinePaths) {
    if (-not (Test-Path -LiteralPath $faultlinePath)) {
        throw "Missing local toolchain: $faultlinePath. Run python tools/bootstrap_windows.py"
    }
}
$env:PATH = ($faultlinePaths -join ';') + ';' + $env:PATH
$faultlineTemp = Join-Path $faultlineRoot '.tools/tmp'
New-Item -ItemType Directory -Force -Path $faultlineTemp | Out-Null
$env:TEMP = $faultlineTemp
$env:TMP = $faultlineTemp
