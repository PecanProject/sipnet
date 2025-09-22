# Logging

SIPNET's logger is a small wrapper around `printf` that adds standard prefixes and, for internal errors, the source file and line number. It is defined in `common/logging.h` and implemented in `common/logging.c`.

## Levels

- 0: Quiet-able (suppressed by `--quiet`).
- 1: Always on (not suppressed).
- 2: Always on and includes `file:line`.


## Usage

- logInfo: Level 0; routine progress, configuration summaries, expected state changes.
- logWarning: Level 0; Recoverable issues or surprises; fallbacks, deprecated/ignored inputs.
- logTest: Level 1; Deterøinistic messages for tests/CI; not user-facing.
- logError: Level 1; Non-recoverable problems preventing correct operation; abort/exit or skip major task.
- logInternalError: Level 2; Errors that should never happen; include details and ask to report.

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


## Notes:

- Each log prints a fixed prefix (e.g., `[INFO   ]`, `[WARNING]`, `[ERROR  ]`).
- Messages use `printf`-style formatting. Include `\n` yourself if you want a newline.
- Level 2 (`logInternalError`) prints file:line; levels 0–1 print just the prefix.
