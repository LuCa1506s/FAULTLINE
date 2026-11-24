# 0.1.0b2 release qualification follow-up

For the publication candidates and independent preflight, see the
[final maintainer review](FINAL_REVIEW.md). This page records the earlier
qualification artifacts and their measured results.

The Phase 14 ledger ends with portable Linux distribution and real-model
execution open. This page records the release follow-up separately. The initial
artifacts remain in `build-scientific-distribution/kit/` for comparison.

| Phase | Gate | Evidence |
| --- | --- | --- |
| 15 | `DISTRIBUTION_MANYLINUX_PASS` | `auditwheel` validated two x64 `manylinux_2_39` wheels. Both passed clean-install tests on glibc 2.39. |
| 16 | `REAL_LLM_EVALUATION_PASS` | Local SmolLM2 inference and offline replay on the CPython 3.12 manylinux candidate. |
| 17 | `PUBLIC_DOCS_PASS` | Installed-wheel quick start, exported benchmark and standalone trace replay exercised. |
| 18 | `RELEASE_KIT_VERIFIED` | Four wheels and sdist with matching package metadata, verified internal wheel hashes, SBOM and kit manifest. |
| 19 | `PUBLICATION_RUNBOOK_AND_DOSSIER_READY` | Maintainer commands and four updated program drafts. |

## Distribution evidence

The four wheels were built from the same refreshed source archive. Its SHA-256
is `a48b0edc4ec67667064e568fb2bdd9d6812433d9b095eaf5ca42376a9ff43340`.
The runtime source fingerprint matches the initial 0.1.0b2 kit. The archive
change is documentation. The input commit is still the Git base recorded in
the manifest. The maintainer reviews and tags the complete release tree.

| Environment | CPython | Wheel tag | Isolated test result |
| --- | --- | --- | --- |
| Ubuntu Base 24.04.5 chroot under WSL2 | 3.12.16 | `manylinux_2_39_x86_64` | 37 passed in 9.54 s |
| Ubuntu Base 24.04.5 chroot under WSL2 | 3.14.10 | `manylinux_2_39_x86_64` | 37 passed in 9.34 s |
| Windows x64 | 3.12.10 | `win_amd64` | 37 passed in 15.18 s |
| Windows x64 | 3.14.7 | `win_amd64` | 37 passed in 17.44 s |

The Ubuntu root archive checksum was verified against the official SHA-256
list before extraction. The build used glibc 2.39, GCC 13.3.0, CMake 3.28.3 and
Ninja 1.11.1. `auditwheel` 6.8.2 rewrote the tags and then validated both final
wheels. `pip check`, isolated native import, and the standalone Python consumer
passed on the four installed wheels. The consumer exported three benchmark
episodes and reverified the custom and idle policy traces in separate processes.
The quick start ran seed 42 and replayed its 464-tick successful planning
episode with hash `07ab6e72d991024d`.

`twine check --strict` passed for all five packages. Each wheel's platform tag,
metadata and long description were compared with the expected tag and sdist.
Each wheel `RECORD` digest was independently recomputed. The release SBOM binds
the wheel hashes to the packaged native components and qualification inventory.
The hosted `manylinux_2_28` job did not run in this session. Ubuntu 22.04,
Debian 12 and other systems below glibc 2.39 need a compatible source build or
a separately qualified wheel.

## Real model and replay

SmolLM2-135M-Instruct Q4_K_M ran locally in llama.cpp b8500 (`342d6125b`) on CPU
with two threads, one slot, a 2,048-token context, temperature zero and server
seed 42. The GGUF SHA-256 matched the published LFS digest
`2e8040ceae7815abe0dcb3540b9995eaa1fa0d2ca9e797d0a635ae4433c68c2d`.
The experiment used the `manylinux_2_39` CPython 3.12 candidate with CPython
3.12.16, Gymnasium 1.4.0 and NumPy 2.5.5. The model endpoint was loopback.

Both runs used FL-Navigation EASY, development seed 42. The unstructured HTTP
run completed 16 real requests and generated 512 output
tokens from 2,976 input tokens. The adapter rejected all 16 outputs. The
episode reached the 120-decision limit with zero stops and hash
`fc16f80b433ed0cb`. Episode time was 29.24 s, with 29.18 s spent in the policy.
The 16-call limit is below the 58-decision reference path for this seed. This
run exercises rejection and offline replay. It provides no route success
estimate.

The separate request-level JSON schema run allowed all 120 decisions. The
model used 59,997 input and 842 output tokens. All 120 responses passed the
action validator and chose East. The episode exhausted its budget at tick 960
with zero stops and hash `caab0a4d37955d04`. Episode time was 243.91 s, with
243.78 s spent in the policy. This run exercises valid model actions and a
replayable failure on a single seed. The two runs use different decision
budgets and serve distinct integration checks. There is no model ranking.

Each replay ran in a separate process after its server stopped. The archive
contains the ordered trace, per-inference outcomes, server request counts,
checkpoint hashes and exact model, executable and wheel digests.

## Review kit

The local publication directory is `build-release-qualification/kit/`. It
contains four platform wheels, the source archive, CycloneDX SBOM, release
notes, evidence ZIP, manifest and SHA-256 checksums. Earlier artifacts and
historical results retain their own paths. The source archive is the wheel
build input. The current review evidence and final status ship separately in
`review-evidence.zip`.

The release archive can be verified with the commands in the
[publication procedure](PUBLICATION.md). Hosted CI and publication have no
execution evidence in this follow-up.

## Maintainer approval

Public read-only checks on 2026-11-24 returned HTTP 404 for the package name,
the specific release version, and the GitHub tag. Repeat these checks after
review and before publishing. The maintainer needs an approved clean commit,
GitHub write access and a PyPI upload credential. Release and application
submissions are maintainer actions.
