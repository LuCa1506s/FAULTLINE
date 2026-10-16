$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
Set-Location $Repo

Write-Host "FAULTLINE Autopilot preflight" -ForegroundColor Cyan
foreach ($cmd in @("python","git","codex")) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        throw "$cmd not found in PATH."
    }
}
python --version
git --version
codex --version

if (-not (Test-Path ".git")) {
    git init
}
$name = git config user.name
$email = git config user.email
if (-not $name) {
    $name = Read-Host "Git user.name for local phase commits"
    git config user.name "$name"
}
if (-not $email) {
    $email = Read-Host "Git user.email for local phase commits"
    git config user.email "$email"
}

git add -A
git diff --cached --quiet
if ($LASTEXITCODE -ne 0) {
    git commit -m "Baseline before FAULTLINE autopilot"
    if ($LASTEXITCODE -ne 0) { throw "Baseline Git commit failed." }
}

$dirty = git status --porcelain
if ($dirty) { throw "Git tree is dirty after baseline:`n$dirty" }

Write-Host ""
Write-Host "Preflight PASS." -ForegroundColor Green
Write-Host "Smoke test:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -StopAfterPhase"
Write-Host "Then full run:"
Write-Host "  powershell -ExecutionPolicy Bypass -File .\automation\run.ps1"
