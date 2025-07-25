# Command Line Options

These instructions explain how to add new command-line options to the SIPNET executable. Three types of command-line 
options are enabled: flags, integer-valued options, and string-valued options.

## Background

The **`Context` struct**, defined in `common/context.h`, holds the values of all command-line options.

When adding a new command-line option, you will need to add a corresponding member to the `Context` struct that will hold the value of that option.

## Important Notes

### Naming

Each command-line option must have a corresponding entry in the `Context` struct. SIPNET uses a mapping system to 
link the command-line option (e.g. `print_header`) to its `Context` member (in this case, `printHeader`).

Some care must be taken to allow SIPNET to map between the option and the `Context` member that holds the options value. 
The mapping is created using the `nameToKey` function in `common/context.c` that:
- removes all non-alphanumeric characters.
- converts alpha characters to lowercase. 

For the mapping to work, the option name (e.g., `print_header`) and `Context` member name (in this case, 
`printHeader`) must generate the same key (e.g. `print_header` and `printHeader` both generate `printheader`).

Each key must be unique across the `Context` member names.

### Precedence

SIPNET options can be specified in either (or both) a config file or a command-line option. Command-line options are
have precedence over config file options.

However, because the command line is parsed first (due to the chicken-and-egg problem that the command line specifies where the config file is!), the code must keep track of - and maintain - the source of the option value. 
This means that:
- Options must be set via the macros `CREATE_INT_CONTEXT`/`CREATE_CHAR_CONTEXT` when they are created.
- The functions `updateIntContext`/`updateCharContext` must be used when updating a value.

See the Function/Macro Reference at the end for more information.

## Steps to Add Each Type of Option

For each type of option, the steps are similar:

1. Add a member to the `Context` struct in `common/context.h`.
2. Initialize the member in `initContext()` in `common/context.c`.
3. Update the `NUM_FLAG_OPTIONS` in `src/sipnet/cli.h` (for flags only).
4. Update the `long_options` struct in `src/sipnet/cli.c` to include the new option.
5. Add tests for the new option's functionality.
6. Update documentation.

But the specific details of steps 1-4 vary depending on the type of option, as described below.

### Flag options

Flags are boolean on/off options, such as `print_header` or `events` (and their corresponding 
negations, `no_print_header` and `no_events`). The code is setup to make it easy to add both
halves of the pair.

Here are the steps to add a flag:

1. In `common/context.h`: Add a member to the `Context` struct to hold the param's value
2. In `commont/context.c`: Initialize the default value in initContext() using the `CREATE_INT_CONTEXT` macro
3. In `src/sipnet/cli.h`: increment the value of `NUM_FLAG_OPTIONS`
4. In `src/sipnet/cli.c`: 
   1. Add the new option to `struct long_options` using the `DECLARE_FLAG` macro AT THE END OF THE LIST OF CURRENT FLAGS; all flags must be at the top of the `long_options` struct
   2. Add the new option to `argNameMap` using the `DECLARE_ARG_FOR_MAP` macro AT THE END OF THE LIST OF CURRENT FLAGS; the order of these declarations must match the order in `long_options`
   3. Update `usage()` to document the new option in the help text

Note: the function that parses the command line options (`parseCLIArgs()` in `src/sipnet/cli.c`) automatically handles
flag options. There is nothing to add there for new flags.

### Integer-valued options

Integer-valued options expect an integer value after the option (e.g., `--num_carbon_pools 3`). 
If the option has a short-form version (with or without a long-form version), follow the steps below. 
For long-form only options, see the notes at the end of this section.

Here are the steps to add an int-value option:

1. In `common/context.h`: Add a member to the `Context` struct to hold the param's value
2. In `commont/context.c`: Initialize the default value in initContext() using the `CREATE_INT_CONTEXT` macro
3. In `src/sipnet/cli.c`:
    1. Add the new option to `struct long_options`... [details TBD]
    2. Update `parseCLIArgs()`... [details TBD]
    3. Update `usage()` to document the new option in the help text

### String-valued options

String-valued options expect a string value after the option (e.g., `-input_file sipnet.in`).
If the option has a short-form version (with or without a long-form version), follow the steps below.
For long-form only options, see the notes at the end of this section.

Here are the steps to add a string-valued option:

1. In `common/context.h`: Add a member to the `Context` struct to hold the param's value
2. In `commont/context.c`: Initialize the default value in initContext() using the `CREATE_CHAR_CONTEXT` macro 
3. In `src/sipnet/cli.c`:
   1. Add the new option to `struct long_options`... [details TBD]
   2. Update `parseCLIArgs()`... [details TBD]
   3. Update `usage()` to document the new option in the help text

### Long-Form Only Options

When adding to the `long_options` struct in step 3(i), give an int value as the last element of the the new
struct. This is the index that should be used in the `parseCLIArgs()` switch statement for handling the new
option. The int value must be unique among the other case labels.

## Additional Steps [TBD]

1. Documentation is great:
  - `src/sipnet/cli.c`
    - Add new option to help message
    - Update `NUM_FLAG_OPTIONS`.
    - Anythiing else?
  - Update the documentation in `docs/user-guide/cli-options.md` [TBD] 
    (could this be automated by adding `sipnet -h > docs/user-guide/cli-options.md`) to `make document`?.
2. Testing is great.
  - Add tests for both the new option and its negation (if applicable) in [TBD].
  - Anything to here to make sure this works in `sipnet.in` [TBD]?

## Function/Macro Reference

When adding a new CLI option, these functions and macros should be used to create and update
the corresponding fields in the `Context` struct.

### `CREATE_INT_CONTEXT`

This macro is used to initialize the `Context` field as well as create the necessary 
metadata information for integer-based arguments, both flag and not.
This macro is used in the `initContext` function in `src/common/context.c`.

Syntax:
```c
CREATE_INT_CONTEXT(name, printName, value, flag)
```
Arguments:
- `name`: the exact name (no quotes) of the `Context` struct member used in `Context.h`
- `printName`: the name used for this option when printing via `--dump-config`
- `value`: the default integer value for this option; the defined values `ARG_ON` and `ARG_OFF` can be used for flag options
- `flag`: `FLAG_YES` or `FLAG_NO` to indicate whether this is a flag option

Examples:
<br>`CREATE_INT_CONTEXT(snow, "SNOW", ARG_ON, FLAG_YES);`
<br>`CREATE_INT_CONTEXT(numSoilCarbonPools, "NUM_SOIL_CARBON_POOLS", 3, FLAG_NO);`

### `CREATE_CHAR_CONTEXT`

This macro is used to initialize the `Context` field as well as create the necessary
metadata information for string-based arguments.
This macro is used in the `initContext` function in `src/common/context.c`.

Syntax:
```c
CREATE_CHAR_CONTEXT(name, printName, value)
```
Arguments:
- `name`: the exact name (no quotes) of the `Context` struct member used in `Context.h`
- `printName`: the name used for this option when printing via `--dump-config`
- `value`: the default string value for this option

Examples:
<br>`CREATE_CHAR_CONTEXT(outConfigFile, "OUT_CONFIG_FILE", NO_DEFAULT_FILE);`
<br>`CREATE_CHAR_CONTEXT(inputFile, "INPUT_FILE", DEFAULT_INPUT_FILE);`

### `updateIntContext`

This function is used to update an integer-based argument, both the stored value as well as the source of that value. 
This function is typically used in the `parseCommandLineArgs` function in `src/sipnet/cli.c`.
Note that flag options should not need this, as they are handled automatically.

Syntax:
```c
updateIntContext(const char *name, int value, context_source_t source)
```
Arguments:
- `name`: a string that can uniquely identify the option (see [Naming](#naming) above)
- `value`: the new integer value for the option 
- `source`: where the value is coming from, e.g. `CTX_CONTEXT_FILE` or `CTX_COMMAND_LINE`

Examples:
<br>`updateIntContext(inputName, intVal, CTX_CONTEXT_FILE);` (from parsing the input file)
<br>`updateIntContext("numSoilCarbonPools", intVal, CTX_COMMAND_LINE);` (from parsing the command line)

### `updateCharContext`

This function is used to update a string-based argument, both the stored value as well as the source of that value.
This function is typically used in the `parseCommandLineArgs` function in `src/sipnet/cli.c`.

Syntax:
```c
updateCharContext(const char *name, const char *value, context_source_t source)
```
Arguments:
- `name`: a string that can uniquely identify the option (see [Naming](#naming) above)
- `value`: the new string value for the option
- `source`: where the value is coming from, e.g. `CTX_CONTEXT_FILE` or `CTX_COMMAND_LINE`

Examples:
<br>`updateCharContext(inputName, inputValue, CTX_CONTEXT_FILE);` (from parsing the input file)
<br>`updateCharContext("inputFile", optarg, CTX_COMMAND_LINE);` (from parsing the command line)
