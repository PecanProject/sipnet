# Logging

SIPNET's logger is a small wrapper around `printf` that adds standard prefixes and,
for internal errors, the source file and line number. It is defined in `common/logging.h` 
and implemented in `common/logging.c`.

## Levels

- 0: Quiet-able (suppressed by `--quiet`).
- 1: Always on (not suppressed).
- 2: Always on and includes `file:line`.

## Functions

- `logInfo`: Level 0; routine progress, expected or supported behavior, configuration summaries, and transparency messages.
- `logWarning`: Level 0; conditions that may affect validity or reliability, or likely require user attention.
- `logTest`: Level 1; deterministic messages for tests/CI; not user-facing.
- `logError`: Level 1; non-recoverable problems: the run cannot continue, state is invalid, or output cannot be trusted.
- `logInternalError`: Level 2; errors that should never happen; include details and ask to report.

### Choosing a function

Rule of thumb:

- Use `logError` if the run should stop.
- Use `logWarning` if the user should probably stop and check the run.
- Use `logInfo` if the run can proceed and the message is mainly for transparency.

## Usage

1. Include the header:
   ```c
   #include "common/logging.h"
   ```
2. Log messages:
   ```c
   logInfo("Initialized OK\n");
   logWarning("Deprecated parameter: %s\n", name);
   logTest("Iteration %d\n", i);
   logError("Missing required parameter: %s\n", key);
   logInternalError("Unexpected state: %d\n", code);
   ```
3. Example outputs:
    ```
    [INFO   ] Initialized OK
    [WARNING] Deprecated parameter: foo
    [TEST   ] Iteration 12
    [ERROR  ] Missing required parameter: bar
    [ERROR (INTERNAL)] (myfile.c:123) Unexpected state: 5
    ```
## Notes

- Each log prints a fixed prefix (e.g., `[INFO   ]`, `[WARNING]`, `[ERROR  ]`).
- Messages use `printf`-style formatting. Include `\n` yourself if you want a newline.
- Level 2 (`logInternalError`) prints file:line; levels 0–1 print just the prefix.
