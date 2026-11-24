# Distribution qualification for 0.1.0b2

## Publication candidates

The release kit uses `win_amd64` and `manylinux_2_39_x86_64`. The Linux
policy requires x86-64 with glibc 2.39 or newer and a compatible system C++
runtime. The verified runtime is Ubuntu Base 24.04.5 in a WSL2 chroot. The
Windows builds use x64 MSVC. See the
[release qualification](RELEASE_QUALIFICATION.md) for the per-ABI results.

For a local Linux replica, use Ubuntu Base 24.04.5 x86-64 with `g++`, `cmake`,
`ninja-build` and the qualified interpreters. Build from the reviewed source
archive with the command below. Use `auditwheel` 6.8.2 and `patchelf` on the
resulting wheels. The command applies the same Linux policy as the local kit.
[Ubuntu Base images and checksums](https://cdimage.ubuntu.com/ubuntu-base/releases/24.04/release/) · [auditwheel](https://github.com/pypa/auditwheel)

```sh
python3.12 -m pip wheel --no-deps --wheel-dir raw faultline_sim-0.1.0b2.tar.gz
python3.14 -m pip wheel --no-deps --wheel-dir raw faultline_sim-0.1.0b2.tar.gz
for wheel in raw/*.whl; do
  auditwheel show "$wheel"
  auditwheel repair --plat manylinux_2_39_x86_64 --wheel-dir portable "$wheel"
done
for wheel in portable/*.whl; do
  auditwheel show "$wheel"
done
```

The `manylinux_2_28` workflow remains configured for broader
coverage. The local experiment establishes `manylinux_2_39_x86_64` coverage.
Ubuntu 22.04 and Debian 12 require a build with a lower policy or a local
source build with a compatible C++20 compiler.

## Building and testing

Build the source archive with `python -m build --sdist`. The build backend and
binding headers are pinned in `pyproject.toml`. Use
`requirements/qualification.txt` when installing the wheel with `gym,test`
for the qualified environment.

```sh
python -m pip wheel --no-deps -w new-wheels path/to/faultline_sim-0.1.0b2.tar.gz
python -m twine check --strict path/to/faultline_sim-0.1.0b2.tar.gz new-wheels/*
```

Use an x64 MSVC developer shell for the tested Windows build. The Linux build
uses GCC, with platform policy determined separately by `auditwheel show` and
`auditwheel repair`. The hosted review workflow configures cibuildwheel for
future container builds. The local audit and hosted CI have distinct evidence.
[auditwheel project](https://github.com/pypa/auditwheel) · [cibuildwheel options](https://cibuildwheel.pypa.io/en/stable/options/)

Copy test modules into a temporary directory outside the source tree. Install
the correct wheel in a fresh venv and run `python -I -m pytest -q` there.
`pip inspect` supplies the installed environment inventory for the CycloneDX
report. The package manifest binds each artifact to its SHA-256 digest.

The source archive uses an allowlist. The first build exposed a glob-pruning
issue that omitted nested SDK headers and game tool sources. The corrected
archive contains 239 entries and passed the required-path check. Local probes,
developer tool caches, GDD and visual demo assets are outside the package.

For the final artifact allowlist and maintainer checklist, see the
[publication procedure](PUBLICATION.md).

## Initial 0.1.0b2 investigation

`DISTRIBUTION_PARTIAL` qualifies the tested x64 matrix and keeps the portable
Linux artifact open. All four wheels came from the same sdist.

| Host | CPython | Installed tests |
| --- | --- | --- |
| Windows 11 | 3.12 | 37/37 in 14.34 s |
| Windows 11 | 3.14 | 37/37 in 18.22 s |
| Ubuntu 26.04.1 under WSL2 | 3.12.16 | 37/37 in 18.52 s |
| Ubuntu 26.04.1 under WSL2 | 3.14.4 | 37/37 in 6.89 s |

The outside-checkout consumers and `pip check` passed on the matrix. Both
versions are represented in the package metadata. Python 3.11 and 3.13 remain
outside the qualified matrix. The qualification extra pins NumPy 2.5.5 and
Gymnasium 1.4.0. `twine check --strict` passed for the sdist and all four wheels.
Windows `pip inspect` needed `-X utf8` to export Unicode license metadata.

`auditwheel show` reported libm symbols at GLIBC 2.43 and required the generic
`linux_x86_64` tag for both Linux artifacts. Docker and Podman were unavailable
on the observed paths. An older build image is needed for a portable tag. The
hosted matrix and release-review workflow are configured for future authorized
use. Local auditwheel, metadata, dependency and test logs are under
`build-scientific-distribution/`. The initial kit includes the sdist, wheels,
CycloneDX inventory, manifest and SHA-256 checksums.
