# Adding New Command Line Options

Follow these instructions to add new command-line options to the sipnet executable, based on what type 
of option is being added. There are three possible types that are enabled: flags, int-valued 
options, and string-valued options.

## Important Notes

A note on naming: each option will have a corresponding entry in the `Context` struct, and some care must be 
taken to allow SIPNET to map between the option 
and the `Context` member that holds the options value. The key that is used for that map is generated from the 
`nameToKey` function in `common/context.c`; that functions removes all non-alphanumeric characters
and converts alpha characters to lowercase. The option name (e.g., `print_header`) and `Context` 
member name (in this case, `printHeader`) must generate the same key. Also, that key must be unique
across the `Context` member names.

A note on precedence: SIPNET options can be specified in either (or both) a config file or as a 
command option. Options specified via the command line have precedence over the values in the 
config file. However, the command line is parsed first (due to the chicken-and-egg problem that the
command line specifies where the config file is!). Care must be taken to make sure that the source 
of an option's value is maintained. In terms of the actual programming, this means all options 
should be set via the macros `CREATE_INT_CONTEXT`/`CREATE_CHAR_CONTEXT` on creation and the functions
`updateIntContext`/`updateCharContext` when updating a value. In particular, the `long_options` 
struct used by`getops_long` should not have the `value` entry set to the option's actual location 
in the `Context` struct (this is handled automatically for flag-type options).

## Adding a new flag

Flags are boolean on/off options, such as `print_header` or `events` (and their corresponding 
negations, `no_print_header` and `no_events`). The code is setup to make it easy to add both
halves of the pair. 

Here are the steps for flags:

1. In `common/context.h`: Add a member to the `Context` struct to hold the param's value
2. In `commont/context.c`: Initialize the default value in initContext() using the `CREATE_INT_CONTEXT` macro
3. In `src/sipnet/cli.h`: increment the value of `NUM_FLAG_OPTIONS`
4. In `src/sipnet/cli.c`: 
   1. Add the new option to `struct long_options` using the `DECLARE_FLAG` macro AT THE END OF THE LIST OF CURRENT FLAGS; all flags must be at the top of the `long_options` struct
   2. Add the new option to `argNameMap` using the `DECLARE_ARG_FOR_MAP` macro AT THE END OF THE LIST OF CURRENT FLAGS; the order of these declarations must match the order in `long_options`
   3. Update `usage()` to document the new option in the help text

Note: the function that parses the command line options (`parseCLIArgs()` in `src/sipnet/cli.c`) automatically handles
flag options. There is nothing to add there for new flags.

## Adding a new integer-valued option

Integer-valued options expect an integer value after the option (e.g., `--num_carbon_pools 3`). 
These are much easier to add if there is a short-form version of the new option (with or without a long-form version); easier to
the point that this documentation only covers this case.

Here are the steps for int-value options:

1. In `common/context.h`: Add a member to the `Context` struct to hold the param's value
2. In `commont/context.c`: Initialize the default value in initContext() using the `CREATE_INT_CONTEXT` macro
3. In `src/sipnet/cli.c`:
    1. Add the new option to `struct long_options`... [details TBD]
    2. Update `parseCLIArgs()`... [details TBD]
    3. Update `usage()` to document the new option in the help text

## Adding a new string-valued option

String-valued options expect a string value after the option (e.g., `-i sipnet.in`).
These are much easier to add if there is a short-form version of the new option (with or without a long-form version); easier to
the point that this documentation only covers this case.

Here are the steps for string-valued options:

1. In `common/context.h`: Add a member to the `Context` struct to hold the param's value
2. In `commont/context.c`: Initialize the default value in initContext() using the `CREATE_CHAR_CONTEXT` macro 
3. In `src/sipnet/cli.c`:
   1. Add the new option to `struct long_options`... [details TBD]
   2. Update `parseCLIArgs()`... [details TBD]
   3. Update `usage()` to document the new option in the help text


## Last Steps

Anything to here to make sure this works in `sipnet.in`?
Mention that testing is great?
