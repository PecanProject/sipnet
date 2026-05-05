# AGENTS.md

## Project map and execution model
- `src/sipnet/frontend.c` is the only production entrypoint (`main`): init context, parse CLI, read `sipnet.in`, validate options, derive file names, then run model.
- Runtime core is in `src/sipnet/sipnet.c`; each timestep follows `updateState()`:
  1) `calculateFluxes()` + `processEvents()` -> 2) `updatePoolsAndBalance()` -> 3) tracker updates.
- Keep flux math pure: mutate pools only in pool-update functions (`updateMainPools`, `updatePoolsForSoil`, `updatePoolsForEvents`) as documented in `docs/developer-guide/code-structure.md`.
- Events are translated into `fluxes.event*` in `src/sipnet/events.c`; they do not directly mutate pools (except tillage tracker decay path).
- Global state lives in `src/sipnet/state.c` (`envi`, `trackers`, `fluxes`, `climate` linked list), so side effects are process-global.

## Build, test, and docs workflows
- Authoritative build is GNU Make, not CMake:
  - `make` (default target `sipnet`)
  - `make help` for all targets
- `CMakeLists.txt` is CLion assistance only (explicitly non-buildable for full project).
- Unit tests: `make testbuild` then `make unit` (wrapper `tools/run_unit_tests.sh` walks `tests/sipnet/*`).
- Smoke tests: `make smoke` (script `tests/smoke/run_smoke.sh` runs each subdir, compares tracked outputs with `git diff`, and `git restore`s `sipnet.config` timestamp-only diffs).
- Full local verification path used by CI intent: `make test`.
- Docs build path: `make document` (`docs/api/` via Doxygen + `site/` via MkDocs).

## Codebase-specific conventions to preserve
- Logging: prefer `logInfo/logWarning/logError/logTest` from `src/common/logging.h`; avoid introducing raw `printf` in runtime code.
- Config naming is forgiving by design: `src/common/context.c` normalizes keys by stripping non-alphanumerics and lowercasing; keep new keys unique after normalization.
- Context precedence is strict: default < input file < CLI < calculated (`ContextSource` in `src/common/context.h`).
- Feature coupling is enforced in `validateContext()`: `nitrogen-cycle` requires both `litter-pool` and `anaerobic`; `anaerobic` requires `water-hresp`; `soil-phenol` and `gdd` are mutually exclusive.
- When adding model flags, update both `NUM_CONTEXT_MODEL_FLAGS` (`src/common/context.h`) and restart serialization checks (`src/sipnet/restart.c`).

## Integration points and brittle edges
- Restart checkpoints are strict schema contracts in `src/sipnet/restart.c`; schema/layout drift requires coordinated updates to code, tests, and docs (`docs/developer-guide/restart-checkpoint.md`).
- Event I/O contract (`events.in` / `events.out`) is part of smoke and restart tests; changing event output formatting breaks regression checks.
- Test pattern: each suite under `tests/sipnet/*` has its own `Makefile` with `TEST_CFILES`; shared helpers in `tests/utils/tUtils.h` and exit stubbing in `tests/utils/exitHandler.c`.
- Existing docs worth trusting first: `docs/README.md`, `docs/developer-guide/*.md`; `src/README.md` is marked TODO/stale.
