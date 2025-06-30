#include "cli.h"

#include "common/exitCodes.h"
#include "common/logging.h"

#include "version.h"

// C preprocessor shenanigans to get stringizing and concatenation to work
// for NO_(x) macro, needed by DECLARE_FLAG macro
#define CONCAT(x, y) x##y
#define STRINGIZE_IMPL(x) #x
#define STRINGIZE(x) STRINGIZE_IMPL(x)
#define NO_(x) STRINGIZE(CONCAT(no_, x))

#define DECLARE_FLAG(name)                                                     \
  {#name, no_argument, &ctx.tmpFlag, 1},                                       \
      {NO_(name), no_argument, &ctx.tmpFlag, 0}

#define DECLARE_ARG_FOR_MAP(x) #x, #x

// The struct 'option' is defined in getopt.h, and is expected by getopt_long()
// See docs/developer-guide/cli-options.md for details on how to add a new
// option
static struct option long_options[] = {  // NOLINT
    // These options set a flag (and they need to be at the top here for
    // indexing purposes). The DECLARE_FLAG macro declares both <flag> and
    // <no_flag> versions of the option.
    DECLARE_FLAG(do_main_output),
    DECLARE_FLAG(do_single_outputs),
    DECLARE_FLAG(events),
    DECLARE_FLAG(litter_pool),
    DECLARE_FLAG(microbes),
    DECLARE_FLAG(print_header),
    DECLARE_FLAG(dump_config),
    DECLARE_FLAG(quiet),

    // clang-format off
    // These options don’t set a flag. We distinguish them by their indices
    // name                         has_arg           flag  val (val is the index)
    {"input_file",                  required_argument, 0,   'i'},
    {"help",                        no_argument,       0,   'h'},
    {"num_carbon_soil_pools",       required_argument, 0,   'n'},
    {"version",                     no_argument,       0,   'v'},
    // clang-format on
    {0, 0, 0, 0}};

// See cli.h
char *argNameMap[] = {
    // Must follow same order as in long_options above; only need flag opts here
    // Gives corresponding name in Context struct (that is, the argument to the
    // DECLARE_ARG_FOR_MAP macro needs to be the name of the corresponding field
    // in Context)
    DECLARE_ARG_FOR_MAP(doMainOutput), DECLARE_ARG_FOR_MAP(doSingleOutputs),
    DECLARE_ARG_FOR_MAP(events),       DECLARE_ARG_FOR_MAP(litterPool),
    DECLARE_ARG_FOR_MAP(microbes),     DECLARE_ARG_FOR_MAP(printHeader),
    DECLARE_ARG_FOR_MAP(dumpConfig),   DECLARE_ARG_FOR_MAP(quiet)};

// Print the help message when requested
void usage(char *progName) {
  // clang-format off
  printf("Usage: %s [OPTIONS]", progName);
  printf("\n");
  printf("Run SIPNET model for one site with configured options.\n");
  printf("\n");
  printf("Options: (defaults are shown in parens at end)\n");
  printf("  -i, --input_file             Name of input config file (sipnet.in)\n");
  printf("  -n, --num_carbon_soil_pools  Number of carbon soil pools (1)\n");
  printf("\n");
  printf("Flag options: (prepend flag with 'no_' to force off, eg '--no_print_header')\n");
  printf("  --events         Enable event handling (1)\n");
  printf("  --litter_pool    Enable litter pool in addition to single soil carbon pool (0)\n");
  printf("  --microbes       Enable microbe modeling (0)\n");
  printf("  --[TBD]   \n");
  printf("  --dump_config    Print final config to <input_file>.config (0)\n");
  printf("  --print_header   Whether to print header row in output files (1)\n");
  printf("  --quiet          Suppress info and warning message (0)\n");
  printf("  --[TBD]   \n");
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
  while ((shortIndex = getopt_long(argc, argv, "hi:n:v", long_options,
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
      case 'n': {
        char *errc;
        int intVal = strtol(optarg, &errc, 0);  // NOLINT
        if (strlen(errc) > 0) {  // invalid character(s) in input string
          printf("Unknown value for num_soil_carbon_pools: %s\n", optarg);
          exit(EXIT_CODE_BAD_CLI_ARGUMENT);
        }
        if (intVal < 1 || intVal > 3) {  // 3? Do we need an upper bound?
          printf("num_soil_carbon_pools must be 1, 2, or 3\n");
          exit(EXIT_CODE_BAD_CLI_ARGUMENT);
        }
        updateIntContext("numSoilCarbonPools", intVal, CTX_COMMAND_LINE);
      } break;
      case 'v':
        version();
        exit(EXIT_CODE_SUCCESS);
      default:
        usage(argv[0]);
        exit(EXIT_CODE_BAD_CLI_ARGUMENT);
    }
  }
}

// Safety check: make sure the elements of the cli argNameMap are actually
// valid members - that is, that we can successfully find a metadata for each
// one. This protects against changing a field name here and forgetting to
// update that map.
void checkCLINameMap(void) {
  int numFlags = 0;
  struct context_metadata *s;

  // Make sure everything in argNameMap maps to a valid metadata
  for (int ind = 0; ind < 2 * NUM_FLAG_OPTIONS; ++ind) {
    s = getContextMetadata(argNameMap[ind]);
    if (s == NULL) {
      logInternalError("mismatched argNameMap and Context contents; "
                       "no metadata for %s\n",
                       argNameMap[ind]);
      exit(EXIT_CODE_INTERNAL_ERROR);
    }
  }
  // Make sure the number of flag metadata structs equals NUM_FLAG_OPTIONS
  for (s = ctx.metaMap; s != NULL;
       s = (struct context_metadata *)(s->hh.next)) {
    if (s->isFlag) {
      ++numFlags;
    }
  }
  if (numFlags != NUM_FLAG_OPTIONS) {
    logInternalError("mismatched argNameMap and Context contents; "
                     "%d metaMap entries, NUM_FLAG_OPTIONS=%d\n",
                     numFlags, NUM_FLAG_OPTIONS);
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
}
