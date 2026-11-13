# FAULTLINE Codex Autopilot

This harness automates Phase 11 through Phase 35 with your **local, already-authenticated Codex CLI**.

It does not make this exact ChatGPT conversation control your computer. Instead it creates the same workflow locally:

```text
phase contract
   ↓
Codex implementation agent (workspace-write)
   ↓
local build + fast test gate
   ↓
Codex independent reviewer (read-only)
   ├─ PASS    → Git commit → next phase
   ├─ REPAIR  → Codex repair → gates → review again
   └─ BLOCKED → stop for a canonical/human decision
```

Codex officially recognizes `AGENTS.md` as persistent repository guidance. The harness uses `codex exec` with structured output, following OpenAI's documented automation pattern.

## Rate limits

Default configuration:

```json
"auto_wait_on_rate_limit": true,
"rate_limit_wait_schedule_seconds": [1800, 3600, 7200, 7200],
"max_rate_limit_retries_per_call": 8
```

So it does not retry aggressively. It waits 30 minutes, then 60 minutes, then uses two-hour intervals. `Ctrl+C` is safe: rerun the same command later.

To stop immediately instead of waiting:

```powershell
powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -NoAutoWait
```

The script cannot increase, bypass or predict your Codex plan quota. If the platform limit is exhausted for longer than the retry window, the run stops and can be resumed later.

## Safety

Implementation calls use:

```text
codex exec --sandbox workspace-write --ask-for-approval never
```

The independent review call uses a read-only sandbox.

This is intentionally powerful. Only run it in the FAULTLINE repository. Each passing phase becomes a Git commit, giving you a rollback point.

Canonical specifications live in the parent directory and AGENTS.md explicitly forbids editing them.

## Installed files

```text
AGENTS.md
AUTOPILOT_README.md
automation/
├── config.json
├── orchestrator.py
├── prepare_repo.ps1
├── run.ps1
├── prompts/
├── schemas/
└── phases/
    ├── phase-11.md
    ├── ...
    └── phase-35.md
```

Ignored runtime files:

```text
automation/state.json
automation/runs/
```

`automation/runs/` contains the exact prompts, stdout/stderr, local gate logs and structured reports for every call.

# Setup

## 1. Verify that Codex works

In the same PowerShell environment:

```powershell
codex --version
```

If needed, launch `codex` once and finish the ChatGPT sign-in before starting automation.

## 2. Run the installer from the extracted bundle

```powershell
powershell -ExecutionPolicy Bypass -File .\install.ps1 `
  -RepoPath "C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1"
```

## 3. Prepare Git and create the Phase-10 baseline

```powershell
cd C:\Users\admin\Documents\FAULTLINE\FAULTLINE_GAME_V1
powershell -ExecutionPolicy Bypass -File .\automation\prepare_repo.ps1
```

If Git does not have a local identity, the script asks for `user.name` and `user.email` once.

It then commits the current repository + harness as:

```text
Baseline before FAULTLINE autopilot
```

# Running

## Recommended first run: Phase 11 only

```powershell
powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -StopAfterPhase
```

This gives you one chance to verify that the harness works correctly on your machine. If Phase 11 passes, it is committed and the state advances to Phase 12.

Check:

```powershell
powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -Status
git log -5 --oneline
```

## Then run Phase 12 through 35 unattended

```powershell
powershell -ExecutionPolicy Bypass -File .\automation\run.ps1
```

You can leave it running. It will stop rather than guess when:
- a canonical conflict is found;
- the reviewer returns `BLOCKED`;
- the configured repair budget is exhausted;
- a non-rate-limit Codex process failure occurs;
- Git cannot checkpoint a passing phase.

If you close the terminal or press `Ctrl+C`, run the same command again.

# Useful commands

Status:

```powershell
powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -Status
```

Run one specified phase:

```powershell
powershell -ExecutionPolicy Bypass -File .\automation\run.ps1 -Phase 17 -StopAfterPhase
```

Only use a phase override deliberately; normal operation should use `state.json`.

Inspect recent logs:

```powershell
Get-ChildItem .\automation\runs -Recurse |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 30
```

Inspect current state:

```powershell
Get-Content .\automation\state.json
```

# Configuration

Edit:

```text
automation/config.json
```

### Model

```json
"model": ""
```

Empty intentionally means: use whatever model your Codex CLI is currently configured to use. This avoids pinning a model name that may change.

### Independent review

```json
"review_enabled": true
```

Keep this on for autonomous development.

### Repair budget

```json
"max_repairs_per_phase": 2
```

Prevents endless agent loops.

### Git checkpoint

```json
"auto_commit": true
```

A phase is committed only after local gates and independent review pass.

### Local gate

```json
"local_fast_gate": true
```

The orchestrator independently runs:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1
ctest --test-dir build -C Debug --output-on-failure -L fast
```

Codex itself remains responsible for the heavier phase-specific stress, persistence, determinism and long-run gates required by each phase contract.

# If BLOCKED

Do **not** force the next phase.

Inspect:

```text
automation/state.json
automation/runs/phase-XX/<latest>/
docs/PHASEXX_VERIFICATION.md
```

If it is a canonical/design conflict, bring that blocker back to ChatGPT. Resolve it, commit the resolution/code changes, and rerun the same phase.

# Rollback

Every passing phase is a Git commit:

```powershell
git log --oneline
```

To safely undo a bad phase:

```powershell
git revert <commit>
```

Prefer `git revert` over destructive history rewriting.

# Important limitation

The reviewer/architect role is another local Codex invocation constrained by the same canonical specs, phase contract and AGENTS.md. It is not literally this exact ChatGPT conversation.

That is intentional: there is currently no native channel from this chat into the Codex process on your Windows machine.
