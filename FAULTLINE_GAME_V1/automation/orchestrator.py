#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, os, re, shutil, subprocess, sys, time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent
AUTO = ROOT / "automation"
CONFIG_PATH = AUTO / "config.json"
STATE_PATH = AUTO / "state.json"
RUNS = AUTO / "runs"
SCHEMAS = AUTO / "schemas"
PROMPTS = AUTO / "prompts"
PHASES = AUTO / "phases"

RATE_PATTERNS = [
    "rate limit", "usage limit", "quota", "too many requests",
    "limit reached", "you've reached", "you have reached",
    "try again later", "usage cap", "reset time"
]

def now() -> str:
    return datetime.now(timezone.utc).isoformat()

def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8-sig"))

def save_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(obj, indent=2, ensure_ascii=False), encoding="utf-8")
    tmp.replace(path)

def load_config() -> dict[str, Any]:
    return load_json(CONFIG_PATH)

def load_state(cfg: dict[str, Any]) -> dict[str, Any]:
    if STATE_PATH.exists():
        return load_json(STATE_PATH)
    state = {
        "current_phase": cfg["phase_start"],
        "status": "ready",
        "completed_phases": [],
        "last_update": now(),
        "last_message": ""
    }
    save_json(STATE_PATH, state)
    return state

def update_state(state: dict[str, Any], **changes: Any) -> None:
    state.update(changes)
    state["last_update"] = now()
    save_json(STATE_PATH, state)

def run_process(argv: list[str], *, cwd: Path, input_text: str | None = None,
                timeout: int | None = None) -> subprocess.CompletedProcess[str]:
    resolved = shutil.which(argv[0]) or argv[0]
    args = [resolved] + argv[1:]
    if os.name == "nt" and str(resolved).lower().endswith((".cmd", ".bat")):
        return subprocess.run(
            subprocess.list2cmdline(args), cwd=str(cwd), input=input_text,
            text=True,
            encoding="utf-8", errors="replace", capture_output=True, timeout=timeout, shell=True, check=False
        )
    return subprocess.run(
        args, cwd=str(cwd), input=input_text, text=True,
        encoding="utf-8", errors="replace",
        capture_output=True, timeout=timeout, check=False
    )

def git(args: list[str], timeout: int = 120) -> subprocess.CompletedProcess[str]:
    return run_process(["git", *args], cwd=ROOT, timeout=timeout)

def ensure_preflight(cfg: dict[str, Any], state: dict[str, Any]) -> None:
    if not (ROOT / "AGENTS.md").exists():
        raise SystemExit("AGENTS.md missing. Run install.ps1 first.")
    if not shutil.which(cfg["codex_command"]):
        raise SystemExit(f"Cannot find '{cfg['codex_command']}' in PATH.")
    if not (ROOT / ".git").exists():
        raise SystemExit("Git repo missing. Run automation/prepare_repo.ps1.")
    if cfg.get("require_clean_git_before_phase", True):
        cp = git(["status", "--porcelain"])
        if cp.returncode != 0:
            raise SystemExit(cp.stderr or "git status failed")
        if cp.stdout.strip():
            resumable = state.get("status") in {
                "implementing", "repairing", "reviewing", "paused_rate_limit"
            }
            if resumable:
                print("[RESUME] Working tree is dirty from an interrupted active phase; continuing that phase.")
            else:
                raise SystemExit("Git tree is not clean:\n" + cp.stdout)

def phase_title_and_text(phase: int) -> tuple[str, str]:
    path = PHASES / f"phase-{phase:02d}.md"
    if not path.exists():
        raise SystemExit(f"Missing phase contract: {path}")
    text = path.read_text(encoding="utf-8")
    title = re.sub(r"^#\s*Phase\s+\d+\s*[â€”-]\s*", "", text.splitlines()[0]).strip()
    return title, text

def is_rate_limited(cp: subprocess.CompletedProcess[str]) -> bool:
    blob = (cp.stdout + "\n" + cp.stderr).lower()
    return any(p in blob for p in RATE_PATTERNS)

def codex_call(cfg: dict[str, Any], state: dict[str, Any], *, prompt: str,
               schema: Path, answer: Path, run_dir: Path, sandbox: str) -> dict[str, Any]:
    run_dir.mkdir(parents=True, exist_ok=True)
    (run_dir / "prompt.txt").write_text(prompt, encoding="utf-8")
    answer.unlink(missing_ok=True)

    args = [
        cfg["codex_command"],
        "--ask-for-approval", cfg.get("ask_for_approval", "never"),
        "exec",
        "--sandbox", sandbox,
        "--output-schema", str(schema),
        "--output-last-message", str(answer),
    ]
    model = str(cfg.get("model", "")).strip()
    if model:
        args += ["--model", model]
    args += ["-"]

    waits = cfg.get("rate_limit_wait_schedule_seconds", [1800, 3600, 7200])
    max_retries = int(cfg.get("max_rate_limit_retries_per_call", 8))
    attempt = 0

    while True:
        attempt += 1
        cp = run_process(
            args, cwd=ROOT, input_text=prompt,
            timeout=int(cfg.get("codex_timeout_seconds", 21600))
        )
        (run_dir / f"stdout.attempt-{attempt}.txt").write_text(cp.stdout, encoding="utf-8")
        (run_dir / f"stderr.attempt-{attempt}.txt").write_text(cp.stderr, encoding="utf-8")
        save_json(run_dir / f"process.attempt-{attempt}.json", {
            "finished": now(), "returncode": cp.returncode
        })

        if cp.returncode == 0 and answer.exists():
            result = load_json(answer)
            time.sleep(max(0, int(cfg.get("min_seconds_between_codex_calls", 0))))
            return result

        if is_rate_limited(cp):
            msg = f"Codex usage/rate limit detected on attempt {attempt}."
            update_state(state, status="paused_rate_limit", last_message=msg)
            if not cfg.get("auto_wait_on_rate_limit", True):
                raise RuntimeError(msg + " Auto-wait disabled; rerun later.")
            if attempt > max_retries:
                raise RuntimeError(msg + " Maximum respectful retries reached.")
            delay = int(waits[min(attempt - 1, len(waits) - 1)])
            print(f"\n[RATE LIMIT] {msg} Waiting {delay//60} min before one retry. Ctrl+C is safe.")
            time.sleep(delay)
            continue

        raise RuntimeError(
            f"Codex exited with {cp.returncode}. See {run_dir}.\n"
            f"stderr tail:\n{cp.stderr[-4000:]}"
        )

def tail(text: str, n: int = 4000) -> str:
    return text[-n:] if len(text) > n else text

def local_gate(cfg: dict[str, Any], phase_dir: Path) -> dict[str, Any]:
    if not cfg.get("local_fast_gate", True):
        return {"enabled": False, "passed": True, "steps": []}
    phase_dir.mkdir(parents=True, exist_ok=True)
    steps = []
    for name, cmd in [("build", cfg["build_command"]), ("fast_tests", cfg["fast_test_command"])]:
        cp = run_process(list(cmd), cwd=ROOT, timeout=7200)
        (phase_dir / f"{name}-stdout.txt").write_text(cp.stdout, encoding="utf-8")
        (phase_dir / f"{name}-stderr.txt").write_text(cp.stderr, encoding="utf-8")
        steps.append({
            "name": name, "command": cmd, "returncode": cp.returncode,
            "stdout_tail": tail(cp.stdout), "stderr_tail": tail(cp.stderr)
        })
        if cp.returncode != 0:
            return {"enabled": True, "passed": False, "steps": steps}
    return {"enabled": True, "passed": True, "steps": steps}

def render_template(path: Path, **kwargs: Any) -> str:
    return path.read_text(encoding="utf-8").format(**kwargs)

def commit_phase(phase: int, title: str) -> None:
    cp = git(["add", "-A"], timeout=300)
    if cp.returncode != 0:
        raise RuntimeError(cp.stderr)
    staged = git(["diff", "--cached", "--quiet"])
    if staged.returncode == 0:
        raise RuntimeError("Reviewer passed but there are no staged changes to commit.")
    cp = git(["commit", "-m", f"Phase {phase}: {title}"], timeout=600)
    if cp.returncode != 0:
        raise RuntimeError("Git commit failed:\n" + cp.stderr)

def run_phase(cfg: dict[str, Any], state: dict[str, Any], phase: int) -> bool:
    title, phase_text = phase_title_and_text(phase)
    phase_dir = RUNS / f"phase-{phase:02d}" / datetime.now().strftime("%Y%m%d-%H%M%S")
    phase_dir.mkdir(parents=True, exist_ok=True)
    print(f"\n=== Phase {phase}: {title} ===")
    # Resume a phase after Codex implementation completed but a later local
    # gate/reviewer/orchestrator step crashed. This avoids spending quota by
    # re-running the same implementation request.
    _resume_impl = None
    _resume_phase_dir = None
    _phase_root = RUNS / f"phase-{phase:02d}"
    _phase_root.mkdir(parents=True, exist_ok=True)

    for _candidate in sorted(
        (p for p in _phase_root.iterdir() if p.is_dir()),
        key=lambda p: p.name,
        reverse=True,
    ):
        _result_candidates = [
            _candidate / "implementation-result.json",
            _candidate / "implementation.json",
        ]
        for _result_path in _result_candidates:
            if not _result_path.exists():
                continue
            try:
                _candidate_impl = load_json(_result_path)
            except Exception:
                continue
            if _candidate_impl.get("phase") != phase:
                continue
            if _candidate_impl.get("status") not in ("completed", "failed", "blocked"):
                continue
            _resume_impl = _candidate_impl
            _resume_phase_dir = _candidate
            break
        if _resume_impl is not None:
            break

    if _resume_impl is not None:
        impl = _resume_impl
        phase_dir = _resume_phase_dir
        print(
            f"[RESUME] Reusing completed Codex implementation from "
            f"{phase_dir.name}; skipping duplicate implementation call."
        )
        update_state(
            state,
            status="verifying",
            current_phase=phase,
            last_message=f"Resuming Phase {phase} at local gate/review",
        )
    else:
        update_state(state, status="implementing", current_phase=phase,
                     last_message=f"Implementing Phase {phase}: {title}")

        prompt = render_template(
            PROMPTS / "implementer.txt",
            phase=phase, title=title, repo_root=ROOT, specs_root=ROOT.parent,
            next_phase=phase+1, phase_text=phase_text
        )
        impl = codex_call(
            cfg, state, prompt=prompt,
            schema=SCHEMAS / "implementation.json",
            answer=phase_dir / "implementation.json",
            run_dir=phase_dir / "implementation",
            sandbox=cfg.get("implementation_sandbox", "workspace-write")
        )
        save_json(phase_dir / "implementation-result.json", impl)

    latest_impl = impl
    if latest_impl["status"] == "blocked":
        update_state(state, status="blocked", last_message=latest_impl["summary"])
        print("[BLOCKED]", latest_impl["summary"])
        return False

    repairs = 0
    while True:
        gate = local_gate(cfg, phase_dir / f"gate-{repairs}")
        save_json(phase_dir / f"gate-{repairs}.json", gate)

        if not gate["passed"]:
            review = {
                "phase": phase, "decision": "REPAIR",
                "summary": "Local build/fast-test gate failed.",
                "issues": [{
                    "severity": "critical", "category": "local_gate",
                    "description": "The orchestrator local gate failed.",
                    "required_fix": "Inspect gate logs, fix build/tests, rerun phase gates."
                }],
                "evidence_checked": ["local gate"],
                "canonical_conflicts": []
            }
        elif cfg.get("review_enabled", True):
            update_state(state, status="reviewing",
                         last_message=f"Reviewing Phase {phase}: {title}")
            rprompt = render_template(
                PROMPTS / "reviewer.txt",
                phase=phase, title=title, repo_root=ROOT, specs_root=ROOT.parent,
                implementation_json=json.dumps(latest_impl, indent=2, ensure_ascii=False),
                gate_summary=json.dumps(gate, indent=2, ensure_ascii=False)
            )
            review = codex_call(
                cfg, state, prompt=rprompt,
                schema=SCHEMAS / "review.json",
                answer=phase_dir / f"review-{repairs}.json",
                run_dir=phase_dir / f"review-{repairs}",
                sandbox=cfg.get("review_sandbox", "read-only")
            )
        else:
            review = {
                "phase": phase, "decision": "PASS",
                "summary": "Independent reviewer disabled.",
                "issues": [], "evidence_checked": ["local gate"],
                "canonical_conflicts": []
            }
        save_json(phase_dir / f"review-result-{repairs}.json", review)

        if review["decision"] == "PASS":
            if cfg.get("auto_commit", True):
                commit_phase(phase, title)
            completed = sorted(set(state.get("completed_phases", []) + [phase]))
            update_state(
                state,
                status="ready" if phase < cfg["phase_end"] else "complete",
                current_phase=phase+1, completed_phases=completed,
                last_message=f"Phase {phase} PASS: {title}"
            )
            print(f"[PASS] Phase {phase}: {title}")
            return True

        if review["decision"] == "BLOCKED":
            update_state(state, status="blocked", last_message=review["summary"])
            print("[BLOCKED BY REVIEW]", review["summary"])
            return False

        if repairs >= int(cfg.get("max_repairs_per_phase", 2)):
            update_state(state, status="blocked",
                         last_message=f"Phase {phase} exhausted repair budget.")
            print("[STOP] Repair budget exhausted.")
            return False

        repairs += 1
        update_state(state, status="repairing",
                     last_message=f"Repair {repairs} for Phase {phase}")
        rprompt = render_template(
            PROMPTS / "repair.txt",
            phase=phase, title=title, repo_root=ROOT, specs_root=ROOT.parent,
            review_json=json.dumps(review, indent=2, ensure_ascii=False),
            gate_summary=json.dumps(gate, indent=2, ensure_ascii=False)
        )
        latest_impl = codex_call(
            cfg, state, prompt=rprompt,
            schema=SCHEMAS / "implementation.json",
            answer=phase_dir / f"repair-{repairs}.json",
            run_dir=phase_dir / f"repair-{repairs}",
            sandbox=cfg.get("implementation_sandbox", "workspace-write")
        )
        save_json(phase_dir / f"repair-result-{repairs}.json", latest_impl)
        if latest_impl["status"] == "blocked":
            update_state(state, status="blocked", last_message=latest_impl["summary"])
            return False

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--phase", type=int)
    ap.add_argument("--stop-after-phase", action="store_true")
    ap.add_argument("--no-auto-wait", action="store_true")
    ap.add_argument("--status", action="store_true")
    args = ap.parse_args()

    cfg = load_config()
    if args.no_auto_wait:
        cfg["auto_wait_on_rate_limit"] = False
    state = load_state(cfg)

    if args.status:
        print(json.dumps(state, indent=2, ensure_ascii=False))
        return 0

    if args.phase is not None:
        if not (cfg["phase_start"] <= args.phase <= cfg["phase_end"]):
            raise SystemExit("Phase outside configured range.")
        state["current_phase"] = args.phase

    while int(state["current_phase"]) <= int(cfg["phase_end"]):
        ensure_preflight(cfg, state)
        phase = int(state["current_phase"])
        if not run_phase(cfg, state, phase):
            return 2
        if args.stop_after_phase:
            return 0

    update_state(state, status="complete", last_message="All configured phases completed.")
    print("\n=== FAULTLINE AUTOPILOT COMPLETE ===")
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nInterrupted safely. Rerun the same command to resume.")
        raise SystemExit(130)

