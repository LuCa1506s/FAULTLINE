# Files changed in Phases 08–14

## Packaging and entry points

- `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`, `pyproject.toml`
- `.github/workflows/distribution-review.yml`
- `requirements/qualification.txt`

## Runtime additions

- `python/faultline_sim/qualification.py` and `python/faultline_sim/scientific.py`
- `python/faultline_sim/model.py` and `python/faultline_sim/model_worker.py`
- `examples/model_agent.py`
- `examples/consumer_project/README.md` and `examples/consumer_project/review.py`
- `tests/python/test_scientific.py` and `tests/python/test_model.py`

## Reproduction and release tools

- `tools/audit_rc.py` and `tools/qualify_scientific.py`
- `tools/distribution_report.py`
- `tools/performance_native.cpp`, `tools/profile_framework.py` and `tools/fuzz_replays.py`
- `tools/plot_scientific.py`

## Documentation

- Added `docs/agent_framework/{RC_AUDIT,SCIENTIFIC_STATUS,SCIENTIFIC_PROTOCOL,SCIENTIFIC_RESULTS}.md`
- Added `docs/agent_framework/{MODEL_INTEGRATION,DISTRIBUTION,PERFORMANCE,EXTERNAL_CONSUMER}.md`
- Added `docs/agent_framework/RELEASE_ANNOUNCEMENT.md` and four drafts under `docs/agent_framework/funding/`
- Updated `docs/agent_framework/{BENCHMARK_RESULTS,DEMO,NEXT_ACTIONS,OPEN_SOURCE_DOSSIER,QUICK_START,RELEASE,TEST_EVIDENCE}.md`

The original game source and tests retain their starting contents. Build,
benchmark and audit outputs live under the documented ignored `build-*`
directories. See [qualification status](SCIENTIFIC_STATUS.md).
