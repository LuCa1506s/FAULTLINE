FAULTLINE CODEX AUTOPILOT — FIRST STEPS

1) Extract this ZIP anywhere.

2) Run from the extracted folder:

powershell -ExecutionPolicy Bypass -File .\install.ps1 `
  -RepoPath "C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1"

3) Then:

cd C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1
powershell -ExecutionPolicy Bypass -File .\automation\prepare_repo.ps1

4) Recommended smoke test — Phase 11 only:

powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -StopAfterPhase

5) If Phase 11 passes, let it continue Phase 12→35:

powershell -ExecutionPolicy Bypass -File .\automation\run.ps1

Full instructions are copied into the repository as AUTOPILOT_README.md.
