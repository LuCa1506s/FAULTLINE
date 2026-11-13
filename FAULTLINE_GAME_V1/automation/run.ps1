param(
    [switch]$StopAfterPhase,
    [switch]$NoAutoWait,
    [int]$Phase = 0,
    [switch]$Status
)
$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent $PSScriptRoot
Set-Location $Repo
$argsList = @("$PSScriptRoot\orchestrator.py")
if ($StopAfterPhase) { $argsList += "--stop-after-phase" }
if ($NoAutoWait) { $argsList += "--no-auto-wait" }
if ($Phase -gt 0) { $argsList += @("--phase", "$Phase") }
if ($Status) { $argsList += "--status" }
python @argsList
exit $LASTEXITCODE
