# Testing

## Summary

**What to Test**

- Core functionality of SIPNET, including parsing, event handling, and input processing.
- System behavior under various scenarios, including edge cases.


**Types of tests:**

- Unit tests in `tests/sipnet/*`: C tests for parsers, events, inputs, and infrastructure.
- Smoke tests in `tests/smoke/*`: short end‑to‑end runs that validate outputs against committed references.


## Unit Tests

- Build all unit tests only:
  ```bash
  make testbuild
  ```
- Run all unit tests (wrapper with summary):
  ```bash
  make unit
  # or: ./tools/run_unit_tests.sh
  # or: make testrun (per-directory runners)
  ```

What to expect:

- List of tests that have been run, with their individual results and exit code.
- A summary table of each test with its status (PASS/FAIL).

Run a single suite or test:

- By directory (common during development):
  ```bash
  make -C tests/sipnet/test_events_infrastructure
  make -C tests/sipnet/test_events_infrastructure run
  ```
- Directly (after building):
  ```bash
  cd tests/sipnet/test_events_types
  ./testEventIrrigation
  ```

Add a unit test:
- Add a `*.c` test file in the appropriate `tests/sipnet/<area>/` folder and list it in that folder’s `Makefile` (variable `TEST_CFILES`). The shared helpers live in `tests/utils/`.

## Smoke Tests

Run all smoke tests:
```bash
make smoke
# or: ./tests/smoke/run_smoke.sh
```

What it does:

- Runs SIPNET in each directory under `tests/smoke/`. (`cd tests/smoke/<dir> && ../../../sipnet -i sipnet.in`).
- Compares generated `sipnet.out`, `events.out`, and `sipnet.config` to the committed versions using `git diff`.
- Prints a summary of passes/fails and a skip count.

Updating references intentionally:

- If changes are expected and outputs are correct, review diffs and commit the updated `*.out` files (and any config changes). The script’s final message explains this workflow.

Notes:

- Some scenarios are temporarily skipped via a `skip` marker file.
- `events.out` is a debug artifact listing applied event deltas per timestep; authoritative model state is in `sipnet.out`.

## Logging in Tests

Use `logTest()`, `logError()`, and friends from `src/common/logging.h` for deterministic output captured by tests. Avoid `printf` in new code.

## CI and Docs

- The smoke tests are designed to be fast and run in CI.
- Run everything locally as CI would:
  ```bash
  make test
  ```
- API docs and site: `make document` builds Doxygen under `docs/api/` and the MkDocs site under `site/`.
