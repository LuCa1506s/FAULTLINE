param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)
$ErrorActionPreference = 'Stop'
$faultlineRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $faultlineRoot
try {
    . ./tools/activate_toolchain.ps1
    $faultlineBuild = if ($Configuration -eq 'Debug') { 'build' } else { 'build-release' }
    cmake -S . -B $faultlineBuild -G Ninja "-DCMAKE_BUILD_TYPE=$Configuration" -DCMAKE_CXX_COMPILER=g++
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    cmake --build $faultlineBuild
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    ctest --test-dir $faultlineBuild --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally {
    Pop-Location
}
