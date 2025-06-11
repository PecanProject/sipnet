#ifndef SIPNET_CLI_H
#define SIPNET_CLI_H

#include <getopt.h>
#include <stdio.h>

#include "context.h"

struct option long_options[] = {
    // These options set a flag
    // name              has_arg            flag         val
    {"print_header", no_argument, &ctx.tmpFlag, 1},
    {"no_print_header", no_argument, &ctx.tmpFlag, 0},
    {"dump_config", no_argument, &ctx.tmpFlag, 1},
    {"no_dump_config", no_argument, &ctx.tmpFlag, 0},
    // These options don’t set a flag. We distinguish them by their indices
    // name              has_arg            flag  val
    {"input_file", required_argument, 0, 'i'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}};

char *argNameMap[] = {
    // Must follow same order as long_options above; only need flag opts here
    // Gives corresponding name in Context struct
    "printHeader", "printHeader", "dumpConfig", "dumpConfig"};

void usage(char *progName) {
  // clang-format off
  printf("Usage: %s [OPTIONS]", progName);
  printf("\n");
  printf("Run SIPNET model for one site with configured options.\n");
  printf("\n");
  printf("Options: (defaults are shown in parens at end)\n");
  printf("  -i, --input_file     Name of input config file (sipnet.in)\n");
  printf("\n");
  printf("Flag options: (prepend flag with 'no_' to force off, eg '--no_print_header')\n");
  printf("\n");
  printf("  --dump_config    Print final config to <input_file>.config (0)\n");
  printf("  --print_header   Whether to print header row in output files (1)\n");
  printf("\n");
  printf("Info options:\n");
  printf("  -h, --help           Print this message and exit\n");
  printf("  -v, --version        Print version information and exit\n");
  printf("\n");
  printf("Configuration options are read from <input_file>. Other options specified on the command\n");
  printf("line override settings from that file.\n");
  printf("\n");
  //printf("\n");
  //printf("\n");
  // clang-format on
}

void version(void) { printf("SIPNET version 2.0.0\n"); }

void parseCommandLineArgs(int argc, char *argv[]) {
  /* getopt_long stores the option index here. */
  int longIndex = 0;
  int shortIndex;
  // get command-line arguments:
  while ((shortIndex = getopt_long(argc, argv, "hi:v", long_options,
                                   &longIndex)) != -1) {

    switch (shortIndex) {
      case 0:
        // long form option, flag == 0
        updateIntContext(argNameMap[longIndex], ctx.tmpFlag, CTX_COMMAND_LINE);
        break;
      case 'h':
        usage(argv[0]);
        exit(1);
      case 'i':
        if (strlen(optarg) >= FILENAME_MAXLEN) {
          printf("ERROR: input filename %s exceeds maximum length of %d\n",
                 optarg, FILENAME_MAXLEN);
          printf("Either change the name or increase INPUT_MAXNAME in "
                 "frontend.c\n");
          exit(1);
        }
        updateCharContext("inputFile", optarg, CTX_COMMAND_LINE);
        break;
      case 'v':
        version();
        break;
      default:
        usage(argv[0]);
        exit(1);
    }
  }
}

#endif  // SIPNET_CLI_H
