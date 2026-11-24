# Security policy

## Reporting

Use GitHub's private vulnerability reporting form when it is available on the
repository. Include the affected version, platform, entry point, expected
boundary, observed effect and a minimal safe reproduction. Remove credentials,
personal information and unpublished experiment data from attachments.
[Private reporting form](https://github.com/LuCa1506s/FAULTLINE/security/advisories/new)

## Scope and trust boundaries

FAULTLINE is a local C++20 simulation library, Python wheel and benchmark CLI.
Review action validation in `sdk/` and `python/faultline_sim/`, snapshot parsing
in `FAULTLINE_GAME_V1/src/persistence/`, trace JSON parsing, and any boundary
where the simulation or evaluator hands observations to a policy. Game saves,
checkpoints and traces originate from the host or from an explicitly opened
file. Policy code and text-provider callbacks execute with the host's privileges.
Isolation for untrusted executables belongs to the host deployment.

## Required properties

- Reject malformed or out-of-range actions before changing engine authority.
- Keep policy observations detached from engine memory, RNG, reward state and
  evaluator data. An action value cannot trigger filesystem access.
- Reject invalid snapshot and trace input with bounded parsing and clear errors.
  Rejected restore operations preserve the current session.
- Preserve state isolation between sessions. Bind one session to its host thread.
- Keep local experiments and package operations free of mandatory telemetry.

## Severity and verification scope

Assess impact from reachable inputs, especially memory corruption, arbitrary
filesystem access, cross-session state changes or disclosure through a policy
observation. A performance or determinism symptom warrants security review when
it breaks a resource or isolation property. Trace checksums provide corruption
detection. Trust in the producer requires a separate provenance mechanism.

The release runs bounded-input tests and a native ASan/UBSan integration test.
See the [verification scope and known issues](docs/agent_framework/KNOWN_ISSUES.md).
