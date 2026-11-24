# Changelog

## 0.1.0b2 publication qualification

- Built and tested CPython 3.12 and 3.14 x64 wheels tagged `manylinux_2_39`.
- Ran local SmolLM2 inference through the route policy and replayed its actions
  after shutting down the model server.
- Rebuilt the Windows wheels from the refreshed source archive and verified
  the installed matrix, package metadata and release checksums.
- Updated the developer quick start, maintainer publishing procedure and
  open-source program drafts. See the [release evidence](docs/agent_framework/RELEASE_QUALIFICATION.md).

## 0.1.0b2 local qualification

- Added four versioned route challenge levels and measured 4,800 held-out episodes.
- Added typed model actions, a bounded HTTP transport and inference-free replay.
- Qualified Windows and Linux on CPython 3.12 and 3.14 with a source archive and SBOM.
- Added a standalone consumer, performance probes, and snapshot and trace fuzz harnesses.
- Recorded the local GLIBC portability limit and separate real-model evaluation status.

## 0.1.0b1

- Added the CPU-only C++ simulation SDK and CPython bindings to the existing engine.
- Added Gymnasium Navigation, Continuity and Adaptation plus random, heuristic,
  planning and offline text policies.
- Added canonical replay and a benchmark CLI with split seeds, per-episode
  exports and result comparison.
- Qualified the original game engine, native consumer and local Windows/Linux
  wheels. See the [measured evidence](docs/agent_framework/TEST_EVIDENCE.md).
