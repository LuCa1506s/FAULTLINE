# Final maintainer review for FAULTLINE 0.1.0b2

## Versions and scope

The public prerelease candidate is `0.1.0b2`. The original b2 source archive,
the qualification source archive and the current checkout agree on packaged
non-documentation source. The final kit preserves the exact qualification sdist
and Linux wheels. The Windows wheels were rebuilt from that archive with MSVC
path remapping. The final Git tree adds release documentation and removes local
profile paths from historical documentation and one automation configuration.
These files are outside the runtime build inputs. The pre-existing game CMake
cleanup is present in the frozen source and the reviewed release tree.

## Independent checks

- GitHub public `main` was `c88513b5dcd369d3605a2259e1ac207974d072e7`.
  The local base `c4851564f8df893d1fa27d8a713916353cfc21da` adds four
  community-template files and uses a non-no-reply Git identity. The reviewed
  release tree applies the template changes directly on the public base. The
  [maintainer runbook](PUBLICATION.md) verifies the staged tree and uses a
  GitHub no-reply address for the release commit and annotated tag.
  A separate Git index was used to rehearse the release commit. Its 199 packaged
  non-documentation files matched the sdist after text line-ending normalization.
- The PyPI project and 0.1.0b2 version endpoints, GitHub tag endpoint and public
  release endpoint returned HTTP 404 on 2026-11-24. This is a point-in-time
  check. PyPI determines name eligibility during registration.
- All four platform wheels passed integrity, metadata and installed native
  smoke checks. Linux CPython 3.12 and 3.14 produced the same tested
  snapshot/restore hash, `53aa7b167c9bc266`, on WSL2. The earlier installed
  qualification establishes the glibc 2.39 floor. The rebuilt Windows wheels
  passed 37 tests in 10.42 s on CPython 3.12 and 37 tests in 10.59 s on 3.14. The
  independent consumer exported its results and verified successful and failed
  trace replays.
- The first Windows build contained private workstation paths through C++
  `std::source_location`. The replacement extensions were built with MSVC
  `/experimental:deterministic` and `/pathmap`. Archive scans of both final
  Windows extensions found zero Windows profile paths in ASCII and UTF-16LE.
- A scoped release hygiene scan covered the proposed Git tree and expanded
  source, wheel and evidence archives. It checked known token patterns, private
  key markers, credentialed URLs, private profile paths and unrelated project
  references. The public pybind11 copyright attribution is retained. The
  repository's existing public Git history remains the parent history.
- The five package artifacts carry Apache-2.0 metadata where applicable and
  include the project notices and pybind11 BSD-3-Clause terms. The sdist also
  includes the vendored doctest MIT license. The source notice covers the
  ColorBrewer Set1 palette used by the separate game map tool. The installed
  native wheels link system and compiler runtime libraries and ship no model,
  GUI assets, CPython, or redistributable C++ runtime DLLs. Gymnasium and NumPy
  are optional dependencies installed separately. The CycloneDX inventory ties
  the final wheel SHA-256 values to the recorded qualification environment.
  [Licensing scope](../../LICENSING.md) · [Third-party inventory](../../THIRD_PARTY_LICENSES.md) · [Upstream ColorBrewer license](https://github.com/axismaps/colorbrewer/blob/master/LICENCE.txt)
- The scientific audit re-counted 4,800 existing CSV rows in 48 cells with
  100 seeds and a successful replay flag per row. The graph planner saturates
  these route tasks and random stays at the floor. The route-budget field may
  correlate with hidden route costs. The real-model run is scoped to one
  development seed and the model failed to reach the goal. See the
  [scientific results](SCIENTIFIC_RESULTS.md) and
  [local-model evidence](RELEASE_QUALIFICATION.md).
- `actionlint` 1.7.12 accepted the three workflow configurations. The exact
  README benchmark and replay commands passed using the rebuilt Windows wheel.

## Publication boundary

The reviewed local kit is `build-publication-preflight/kit/`. The older kit
stays available as the frozen qualification input. GitHub receives the four
wheels, sdist, release notes, evidence ZIP, SBOM, artifact manifest and SHA-256
list. PyPI receives the five package files. Local probes, raw wheel builds,
model weights, application drafts and unpublished commit history stay local.

The hosted game and framework workflows are configured for push or pull
request. The release checks must observe their successful runs on the exact
published commit. The distribution review workflow prepares artifacts for
review, while the [publication procedure](PUBLICATION.md) runs the explicit
authenticated release operations. The glibc 2.39 minimum is documented for
Linux. Hosted checks and publication remain maintainer actions.
