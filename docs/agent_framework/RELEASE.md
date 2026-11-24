# Release review

See the [publication procedure](PUBLICATION.md) for the current artifact
allowlist, the source review and the steps a maintainer runs after approval.

The 0.1.0b2 publication candidates are in `build-release-qualification/kit/`.
They include the refreshed sdist, two Windows x64 wheels, two
`manylinux_2_39_x86_64` wheels, the CycloneDX SBOM and the evidence archive.
See the [release qualification](RELEASE_QUALIFICATION.md) for current results.

## Initial local kits

The 0.1.0b2 follow-up kit is under `build-scientific-distribution/kit/` with
qualified CPython 3.12 and 3.14 wheels and an independently exercised consumer.
See [b2 gate status](SCIENTIFIC_STATUS.md), [distribution](DISTRIBUTION.md) and
[scientific results](SCIENTIFIC_RESULTS.md). The separate b1 kit below supplies
the frozen audit input.

The 0.1.0b1 review kit lives in `build-framework-release/0.1.0b1/`. It contains
Windows and Linux wheels, installation and example logs, benchmark results,
sample traces and a dependency advisory audit. The manifest records the
source fingerprint, Git base, artifact sizes and SHA-256 digests.

```sh
python tools/package_manifest.py build-framework-release/0.1.0b1 --verify
```

The Windows wheel targets CPython 3.12 x64. The Linux wheel targets CPython 3.14
x64 and was built and tested on Ubuntu 26.04.1. Its portable Linux coverage is
a separate release task. The [quick start](QUICK_START.md) describes how to
build a wheel for a compatible local Python and toolchain. The workflow in
`.github/workflows/framework.yml` is configured to build and test on Windows
and Linux. Local results and hosted workflow results have separate provenance.

The kit remains local for maintainer review. It includes the existing project
licenses and the upstream pybind11 license. See the
[known issues](KNOWN_ISSUES.md) and [test evidence](TEST_EVIDENCE.md).
