#include "cli.h"

#include "common/exitCodes.h"
#include "common/logging.h"

#include "version.h"

#define DECLARE_FLAG(name)                                                     \
  {#name, no_argument, &ctx.tmpFlag, 1},                                       \
      {"no-" #name, no_argument, &ctx.tmpFlag, 0}

#define DECLARE_ARG_FOR_MAP(x) #x, #x

// The struct 'option' is defined in getopt.h, and is expected by getopt_long()
// See docs/developer-guide/cli-options.md for details on how to add a new
// option
static struct option long_options[] = {  // NOLINT
    // These options set a flag (and they need to be at the top here for
    // indexing purposes). The DECLARE_FLAG macro declares both <flag> and
    // <no_flag> versions of the option.
    // clang-format off
    DECLARE_FLAG(events),
    DECLARE_FLAG(gdd),
    DECLARE_FLAG(growth-resp),
    DECLARE_FLAG(leaf-water),
    DECLARE_FLAG(litter-pool),
    DECLARE_FLAG(microbes),
    DECLARE_FLAG(snow),
    DECLARE_FLAG(soil-phenol),
    DECLARE_FLAG(water-hresp),

    DECLARE_FLAG(do-main-output),
    DECLARE_FLAG(do-single-outputs),
    DECLARE_FLAG(dump-config),
    DECLARE_FLAG(print-header),
    DECLARE_FLAG(quiet),

    // These options don’t set a flag. We distinguish them by their indices.
    // name                         has_arg           flag  val (val is the
    // index)
    {"input-file", required_argument, 0, 'i'},
    {"file-name", no_argument, 0, 'f'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'v'},
    {0, 0, 0, 0}};

// See cli.h
char *argNameMap[] = {
    // Must follow same order as in long_options above; only need flag opts here
    // Gives corresponding name in Context struct (that is, the argument to the
    // DECLARE_ARG_FOR_MAP macro needs to be the name of the corresponding field
    // in Context)
    // Model options
    DECLARE_ARG_FOR_MAP(events), DECLARE_ARG_FOR_MAP(gdd),
    DECLARE_ARG_FOR_MAP(growthResp), DECLARE_ARG_FOR_MAP(leafWater),
    DECLARE_ARG_FOR_MAP(litterPool), DECLARE_ARG_FOR_MAP(microbes),
    DECLARE_ARG_FOR_MAP(snow), DECLARE_ARG_FOR_MAP(soilPhenol),
    DECLARE_ARG_FOR_MAP(waterHResp),

    // I/O
    DECLARE_ARG_FOR_MAP(doMainOutput), DECLARE_ARG_FOR_MAP(doSingleOutputs),
    DECLARE_ARG_FOR_MAP(dumpConfig), DECLARE_ARG_FOR_MAP(printHeader),
    DECLARE_ARG_FOR_MAP(quiet)};
// clang-format on

// Print the help message when requested
void usage(char *progName) {
  // clang-format off
  printf("Usage: %s [OPTIONS]", progName);
  printf("\n");
  printf("Run SIPNET model for one site with configured options.\n");
  printf("\n");
  printf("Options: (defaults are shown in parens at end)\n");
  printf("  -i, --input-file <input-file>      Name of input config file ('sipnet.in')\n");
  printf("  -f, --file-name  <name>            Prefix of climate and parameter files ('sipnet')\n");
  printf("\n");
  printf("Model flags: (prepend flag with 'no-' to force off, eg '--no-events')\n");
  printf("  --events             Enable event handling (1)\n");
  printf("  --gdd                Use growing degree days to determine leaf growth (1)\n");
  printf("  --growth-resp        Explicitly model growth resp, rather than including with maint resp (0)\n");
  printf("  --leaf-water         Calculate leaf pool and evaporate from that pool (0)\n");
  printf("  --litter-pool        Enable litter pool in addition to single soil carbon pool (0)\n");
  printf("  --microbes           Enable microbe modeling (0)\n");
  printf("  --snow               Keep track of snowpack, rather than assuming all precipitation is liquid (1)\n");
  printf("  --soil-phenol        Use soil temperature to determine leaf growth (0)\n");
  printf("  --water-hresp        Whether soil moisture affects heterotrophic respiration (1)\n");
  printf("\n");
  printf("Output flags: (prepend flag with 'no-' to force off, eg '--no-print-header')\n");
  printf("  --do-main-output     Print time series of all output variables to <file-name>.out (1)\n");
  printf("  --do-single-outputs  Print outputs one variable per file (e.g. <file-name>.NEE)\n");
  printf("  --dump-config        Print final config to <file-name>.config (0)\n");
  printf("  --print-header       Whether to print header row in output files (1)\n");
  printf("  --quiet              Suppress info and warning message (0)\n");
  printf("\n");
  printf("Info options:\n");
  printf("  -h, --help           Print this message and exit\n");
  printf("  -v, --version        Print version information and exit\n");
  printf("\n");
  printf("Configuration options are read from <input_file>. Other options specified on the command\n");
  printf("line override settings from that file.\n");
  printf("\n");
  printf("Note the following restrictions on these options:\n");
  printf(" --soil-phenol and --gdd may not both be turned on\n");
  printf(" --events and --microbes may not both be turned on\n");
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
      case 'f':
        if (strlen(optarg) >= FILENAME_MAXLEN) {
          printf("ERROR: filename %s exceeds maximum length of %d\n", optarg,
                 FILENAME_MAXLEN);
          printf("Either change the name or increase INPUT_MAXNAME in "
                 "frontend.c\n");
          exit(1);
        }
        updateCharContext("fileName", optarg, CTX_COMMAND_LINE);
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
