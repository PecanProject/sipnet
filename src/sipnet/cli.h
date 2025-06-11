#ifndef SIPNET_CLI_H
#define SIPNET_CLI_H

#include <getopt.h>
#include <stdio.h>

#include "context.h"

// This file encapsulates management of the cli - that is, parsing the command
// line options, and managing what those options are

// The struct 'option' is defined in getopt.h, and is expected by getopt_long()
extern struct option long_options[];

// The run-time option names do not match their corresponding fields in Context,
// so we need a way to get from one to the other. There is a check in
// initContext() that verifies that these all work. The #define here is for
// that check.
#define NUM_FLAG_OPTIONS 4
extern char *argNameMap[NUM_FLAG_OPTIONS];

// Print the help message when requested
void usage(char *progName);

// Print the version when requested
void version(void);

// Parses command-line options using getopt_long
void parseCommandLineArgs(int argc, char *argv[]);

#endif  // SIPNET_CLI_H
