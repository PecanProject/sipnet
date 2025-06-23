# Logging

SIPNET's logger is defined in `common/logger.h` and implemented in `common/logger.c`. 

It provides a simple interface for logging messages at different levels (e.g., debug, info, warning, error).

The use of logger functions is preferred over `printf` because ... 
It is appropriate to use printf when ...

## Logging Levels 

- **logDebug**: Information useful during development or debugging.
- **logInfo**: General information about the program's execution, such as successful initialization or key milestones.
- **logWarning**: Non-critical issues that might require attention but do not stop execution. Example: deprecated parameters or ignored input.
- **logError**: Critical issues that prevent the program from continuing correctly. Example: missing required parameters or internal errors.


## Usage

1. Include the logger header in your source file:
   ```c
   #include "common/logger.h"
   ```

2. Use the logging functions to log messages at different levels:
   ```c
   // Log messages at different levels
   logDebug("This is a debug message");
   logInfo("This is an info message");
   logWarning("This is a warning message");
   logError("This is an error message");
   ```
