#include "cli.h"

#include "common/exitCodes.h"

#include "version.h"

// The struct 'option' is defined in getopt.h, and is expected by getopt_long()
struct option long_options[] = {
    // These options set a flag (and they need to be at the top here)
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

// See cli.h
#define NUM_FLAG_OPTIONS 4
char *argNameMap[NUM_FLAG_OPTIONS] = {
    // Must follow same order as long_options above; only need flag opts here
    // Gives corresponding name in Context struct
    "printHeader", "printHeader", "dumpConfig", "dumpConfig"};

// Print the help message when requested
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

// Print the version when requested
void version(void) { printf("SIPNET version %s\n", VERSION_STRING); }

// Parses command-line options using getopt_long
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
        exit(EXIT_CODE_SUCCESS);
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
        exit(EXIT_CODE_SUCCESS);
      default:
        usage(argv[0]);
        exit(EXIT_CODE_BAD_CLI_ARGUMENT);
    }
  }
}
