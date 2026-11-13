# Third-party licenses and attributions

**Inventory status:** Initial source-tree and dependency review recorded; provenance and binary-distribution review remain open.

**Reviewed revision:** `434acac6184b0cacf9e64b599c465743cb9df7e3` (2026-11-13).

FAULTLINE's original code and technical documentation are licensed under Apache-2.0, subject to [LICENSING.md](LICENSING.md). Third-party materials retain their own terms. This inventory distinguishes material distributed in the source tree from tools obtained separately.

## 1. Distributed third-party material and algorithm references

| Component | Version / authors | Upstream | License / terms | Location and use |
|---|---|---|---|---|
| doctest | 2.4.11; Viktor Kirilov | [Source](https://github.com/doctest/doctest/tree/v2.4.11) | MIT | [Vendored test header](FAULTLINE_GAME_V1/tests/vendor/doctest.h); [original license](FAULTLINE_GAME_V1/tests/vendor/LICENSE.txt) |
| xoshiro256** | 1.0; David Blackman and Sebastiano Vigna (2018) | [Reference implementation](https://prng.di.unimi.it/xoshiro256starstar.c) | Public-domain dedication and permissive fallback grant in the current upstream reference | [Deterministic RNG](FAULTLINE_GAME_V1/src/core/rng/deterministic_rng.cpp) |
| SplitMix64 | Sebastiano Vigna (2015 reference implementation) | [Reference implementation](https://prng.di.unimi.it/splitmix64.c) | Public-domain dedication and permissive fallback grant in the current upstream reference | Seed expansion in deterministic_rng.cpp |
| ColorBrewer Set1 | Cynthia Brewer, Mark Harrower, and The Pennsylvania State University | [ColorBrewer project](https://github.com/axismaps/colorbrewer), [license](https://github.com/axismaps/colorbrewer/blob/master/LICENCE.txt) | Apache-2.0 in current upstream repository | Debug-map faction palette matches Set1; attribution included conservatively, exact original source/revision not recorded |

### Attribution notices

- doctest: Copyright (c) 2016-2023 Viktor Kirilov. The original MIT license and copyright notice are retained in `FAULTLINE_GAME_V1/tests/vendor/LICENSE.txt` and the header. Preserve them with source distributions and applicable test binary distributions.
- xoshiro256**: reference algorithm by David Blackman and Sebastiano Vigna, 2018. SplitMix64 reference implementation by Sebastiano Vigna, 2015. See upstream references above. The FAULTLINE C++ implementation uses these algorithms within its own RNG interface; preserve attribution.
- ColorBrewer: Copyright (c) 2002 Cynthia Brewer, Mark Harrower, and The Pennsylvania State University. This product includes color specifications and designs developed by Cynthia Brewer (https://colorbrewer2.org/). The debug map's five faction colors match the Set1 palette. The root [Apache-2.0 license](LICENSE) supplies the license text for the current Apache-2.0 upstream distribution; confirm original palette provenance and preserve any additional applicable notices before release.

## 2. External build and development tools

These tools are not vendored in the tracked source tree. Their own licenses apply to separately downloaded tools and to any runtime components redistributed with future binaries.

| Tool | Version in project configuration | Upstream / licensing |
|---|---|---|
| CMake | 4.4.4 (Windows bootstrap); minimum CMake 3.20 | [CMake licensing](https://cmake.org/licensing/); BSD-3-Clause for CMake, with additional bundled components |
| Ninja | 1.13.2 (Windows bootstrap) | [Ninja project and license](https://github.com/ninja-build/ninja); consult the release license and bundled notices |
| w64devkit | 2.10.1 (GCC 16.2.0 observed on Windows) | [Release](https://github.com/skeeto/w64devkit/releases/tag/v2.10.1); toolchain bundle with multiple component licenses |
| Python | 3.12 in CI; 3.12.10 observed in clean Windows verification | [Python license](https://docs.python.org/3/license.html); external interpreter, standard-library scripts |
| GitHub Actions | checkout v6, setup-python v6, upload-artifact v4 at reviewed revision | [checkout](https://github.com/actions/checkout), [setup-python](https://github.com/actions/setup-python), [upload-artifact](https://github.com/actions/upload-artifact); external CI actions, not bundled |
| Codex CLI | External optional development tool; version not inventoried | [Codex CLI](https://github.com/openai/codex); not required for ordinary CMake build or tests |

The Windows bootstrap obtains CMake and Ninja wheels via PyPI, and w64devkit via its upstream GitHub release, under `.tools/`. It verifies wheel hashes and the w64devkit release digest when supplied. Linux CI uses its runner toolchain. No external package manager or Git submodule is required for the declared C++ build graph; doctest is used by tests only.

Before distributing executable packages, inspect the actual compiler/runtime libraries linked or included (including GCC, libstdc++, MinGW-w64 and system libraries where applicable), their exceptions, and any required license notices. Do not infer binary redistribution compliance from the source-tree inventory alone.

## 3. Project-generated material and provenance

| Material | Evidence | Remaining check |
|---|---|---|
| 13 historical `.flsave` fixtures | [Fixture provenance](FAULTLINE_GAME_V1/tests/fixtures/README.md) documents formats, seeds, historical writers, SHA-256 values and recovery procedures. | Recompute hashes against the recorded values; original creation command for `save_v7_phase13.flsave` is not recorded. Preserve fixtures as compatibility evidence. |
| `map_raw.svg`, `map_runtime.svg`, `map_save.svg` | [Map2D viewer](FAULTLINE_GAME_V1/docs/MAP2D_DEBUG_VIEWER.md) and [audit](FAULTLINE_GAME_V1/docs/MAP2D_AUDIT.md) describe procedural SVG output in raw, runtime and load modes. | Confirm the exact tracked files were produced by the project tool, inspect embedded resources, and preserve applicable palette attribution. |
| Original game design, branding and creative assets | [Licensing scope](LICENSING.md) excludes reserved creative material from default Apache-2.0 licensing. | Confirm ownership and permissions for any future publication; no raster art, audio, font or 3D model files were identified in the reviewed tracked-file inventory. |

## 4. Review scope and release requirements

The initial review covered the tracked-file inventory, license/copyright searches, CMake link declarations, external downloads, vendored test dependency, and selected RNG and visualization sources at the reviewed revision. No additional third-party runtime library was identified in the declared CMake graph. This is not a complete line-by-line origin audit or a legal certification.

Open checks:

- Confirm ownership and provenance of project-authored files, including automation scripts and technical documentation.
- Verify historical commits for third-party content and notices; deleting a file from the current tree does not remove it from Git history.
- Confirm exact SVG provenance and historical fixture hashes.
- Preserve upstream notices, review all new dependencies and assets, and check binary redistribution obligations before publishing a release.
- Update this inventory whenever dependencies, build tools, assets, or licensing scope change.

The root [LICENSE](LICENSE) does not replace third-party licenses or grant rights to material that FAULTLINE's contributors do not own.
