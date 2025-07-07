#ifndef SIPNET_CLI_H
#define SIPNET_CLI_H

#include <getopt.h>
#include <stdio.h>

#include "common/context.h"

// This file encapsulates management of the cli - that is, parsing the command
// line options, and managing what those options are

// The struct 'option' is defined in getopt.h, and is expected by getopt_long()
// extern struct option long_options[];

// The run-time option names do not match their corresponding fields in Context,
// so we need a way to get from one to the other.
#define NUM_FLAG_OPTIONS 9
extern char *argNameMap[2 * NUM_FLAG_OPTIONS];

/*!
 * Verify that the cli's arg-name map is valid
 *
 * Verifies that everything in argNameMap maps to a valid context metadata.
 * Does not verify that we failed to add a flag to the map.
 * Call this after initContext()
 */
void checkCLINameMap(void);

// Print the help message when requested
void usage(char *progName);

// Print the version when requested
void version(void);

// Parses command-line options using getopt_long
void parseCommandLineArgs(int argc, char *argv[]);

#endif  // SIPNET_CLI_H
