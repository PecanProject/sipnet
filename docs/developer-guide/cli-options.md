# Command Line Options

These instructions explain how to add new command-line options to the SIPNET executable. Three types of command-line 
options are enabled: flags, integer-valued options, and string-valued options.

## Background

The **`Context` struct**, defined in `common/context.h`, holds the values of all command-line options.

When adding a new command-line option, you will need to add a corresponding member to the `Context` struct that will hold the value of that option.

## Important Notes

**Naming:** 

Each command-line option must have a corresponding entry in the `Context` struct. SIPNET uses a mapping system to 
link the command-line option (e.g. `print_header`) to its `Context` member (in this case, `printHeader`).

Some care must be taken to allow SIPNET to map between the option and the `Context` member that holds the options value. 
The mapping is created using the `nameToKey` function in `common/context.c` that:
- removes all non-alphanumeric characters.
- converts alpha characters to lowercase. 

For the mapping to work, the option name (e.g., `print_header`) and `Context` member name (in this case, 
`printHeader`) must generate the same key (e.g. `printheader` and `printHeader` both generate `printheader`).

Each key must be unique across the `Context` member names.

**Precedence:** 

SIPNET options can be specified in either (or both) a config file or a command-line option. Command-line options are
have precedence over config file options.

However, because the command line is parsed first (due to the chicken-and-egg problem that the command line specifies where the config file is!), the code must keep track of - and maintain - the source of the option value. 
This means that:
- Options must be set via the macros `CREATE_INT_CONTEXT`/`CREATE_CHAR_CONTEXT` when they are created.
- The functions `updateIntContext`/`updateCharContext` must be used when updating a value.

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

## Long-Form Only Options

[TBD: fold this in above? This isn't that complicated; let's see if anything changes in the next round]

When adding to the `long_options` struct in step 3(i), give an int value as the last element of the the new
struct. This is the index that should be used in the `parseCLIArgs()` switch statement for handling the new 
option. The int value must be unique among the other case labels.
