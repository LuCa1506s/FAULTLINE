param(
    [Parameter(Mandatory=$true)]
    [string]$RepoPath
)
$ErrorActionPreference = "Stop"
$Repo = (Resolve-Path $RepoPath).Path
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Payload = Join-Path $Here "payload"

Write-Host "Installing FAULTLINE Codex Autopilot into:" -ForegroundColor Cyan
Write-Host "  $Repo"

$AutoSrc = Join-Path $Payload "automation"
$AutoDst = Join-Path $Repo "automation"
if (Test-Path $AutoDst) {
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $backupRoot = Join-Path $Repo ".autopilot_backups"
    New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
    $backup = Join-Path $backupRoot "automation.$stamp"
    Copy-Item $AutoDst $backup -Recurse
    Write-Host "Existing automation directory backed up to $backup"
}
New-Item -ItemType Directory -Force -Path $AutoDst | Out-Null
Copy-Item (Join-Path $AutoSrc "*") $AutoDst -Recurse -Force

$AgentsDst = Join-Path $Repo "AGENTS.md"
$Block = Get-Content (Join-Path $Payload "AGENTS.autopilot.md") -Raw
if (Test-Path $AgentsDst) {
    $Existing = Get-Content $AgentsDst -Raw
    if ($Existing -notmatch "FAULTLINE_AUTOPILOT_BEGIN") {
        Add-Content $AgentsDst "`r`n$Block"
        Write-Host "Appended FAULTLINE block to existing AGENTS.md"
    } else {
        Write-Host "AGENTS.md already contains the FAULTLINE autopilot block."
    }
} else {
    Set-Content $AgentsDst $Block -Encoding UTF8
    Write-Host "Created AGENTS.md"
}

Copy-Item (Join-Path $Payload "AUTOPILOT_README.md") (Join-Path $Repo "AUTOPILOT_README.md") -Force

$GitIgnore = Join-Path $Repo ".gitignore"
$IgnoreBlock = @"

# FAULTLINE Codex Autopilot runtime
automation/state.json
automation/runs/
automation/*.tmp
.autopilot_backups/
"@
if (Test-Path $GitIgnore) {
    $g = Get-Content $GitIgnore -Raw
    if ($g -notmatch "FAULTLINE Codex Autopilot runtime") {
        Add-Content $GitIgnore $IgnoreBlock
    }
} else {
    Set-Content $GitIgnore $IgnoreBlock.TrimStart() -Encoding UTF8
}

Write-Host ""
Write-Host "Install complete." -ForegroundColor Green
Write-Host "Next:"
Write-Host "  cd `"$Repo`""
Write-Host "  powershell -ExecutionPolicy Bypass -File .\automation\prepare_repo.ps1"
