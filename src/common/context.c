#include "context.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/exitCodes.h"

#define DEFAULT_INPUT_FILE "sipnet.in"
#define DEFAULT_FILE_NAME "sipnet"
#define NO_DEFAULT_FILE ""
#define ARG_OFF 0
#define ARG_ON 1
#define FLAG_YES 1
#define FLAG_NO 0
#define DEFAULT_NUM_POOLS 1

struct Context ctx;

// Temp space for name-to-key conversions
static char keyName[CONTEXT_CHAR_MAXLEN];

// Default values for all context fields
void initContext(void) {
  // Init hash map to NULL before adding anything to it
  ctx.metaMap = NULL;

  // clang-format off
  // Init the params
  // Flags, model options
  CREATE_INT_CONTEXT(events,          "EVENTS",           ARG_ON,  FLAG_YES);
  CREATE_INT_CONTEXT(gdd,             "GDD",              ARG_ON,  FLAG_YES);
  CREATE_INT_CONTEXT(growthResp,      "GROWTH_RESP",      ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(leafWater,       "LEAF_WATER",       ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(litterPool,      "LITTER_POOL",      ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(microbes,        "MICROBES",         ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(snow,            "SNOW",             ARG_ON,  FLAG_YES);
  CREATE_INT_CONTEXT(soilPhenol,      "SOIL_PHENOL",      ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(soilQuality,     "SOIL_QUALITY"    , ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(waterHResp,      "WATER_HRESP",      ARG_ON,  FLAG_YES);

  // Flags, I/O
  CREATE_INT_CONTEXT(doMainOutput,    "DO_MAIN_OUTPUT",   ARG_ON,  FLAG_YES);
  CREATE_INT_CONTEXT(doSingleOutputs, "DO_SINGLE_OUTPUT", ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(dumpConfig,      "DUMP_CONFIG",      ARG_OFF, FLAG_YES);
  CREATE_INT_CONTEXT(printHeader,     "PRINT_HEADER",     ARG_ON,  FLAG_YES);
  CREATE_INT_CONTEXT(quiet,           "QUIET",            ARG_OFF, FLAG_YES);

  // Files
  CREATE_CHAR_CONTEXT(paramFile,      "PARAM_FILE",       NO_DEFAULT_FILE);
  CREATE_CHAR_CONTEXT(climFile,       "CLIM_FILE",        NO_DEFAULT_FILE);
  CREATE_CHAR_CONTEXT(outFile,        "OUT_FILE",         NO_DEFAULT_FILE);
  CREATE_CHAR_CONTEXT(outConfigFile,  "OUT_CONFIG_FILE",  NO_DEFAULT_FILE);
  CREATE_CHAR_CONTEXT(inputFile,      "INPUT_FILE",       DEFAULT_INPUT_FILE);
  // clang-format on

  // Other
  // Prefix for climate and parameter input files. We may want to rename this
  // to siteName or such, as 'fileName' implies an actual file, though that
  // would be a breaking change.
  CREATE_CHAR_CONTEXT(fileName, "FILE_NAME", DEFAULT_FILE_NAME);

  // Number of soil carbon pools being used in this run, should in [1,3] (I
  // don't think greater than three has been tested). Note that 1 has some
  // implications in the code, see soilMultiPool. Also note that this is NOT
  // a flag.
  CREATE_INT_CONTEXT(numSoilCarbonPools, "NUM_SOIL_CARBON_POOLS",
                     DEFAULT_NUM_POOLS, FLAG_NO);

  // Whether this is a soil carbon multipool run; literally defined as
  // (num_soil_carbon_pools > 1), but it's a nice convenience. Some model
  // options are only possible when this is true. Calculated quantity, not a
  // CLI arg. This is technically a flag, but we aren't looking to treat it as
  // such (it's not a command-line option, so we don't want the mechanics
  // associated with one).
  CREATE_INT_CONTEXT(soilMultiPool, "SOIL_MULTI_POOL", (DEFAULT_NUM_POOLS > 1),
                     FLAG_NO);
}

// With all the different permutations of spellings for config params, lets
// strip names down by removing chars like dashes and underscores, and
// converting to lowercase. This will allow different versions to still find
// the relevant metadata, while retaining uniqueness.
void nameToKey(const char *name) {
  int keyInd = 0;
  // Drop all non-alphanumeric chars (eg '-', '_'), and convert to lowercase
  for (int i = 0; name[i]; i++) {
    if (isalnum(name[i])) {
      keyName[keyInd] = tolower(name[i]);  // NOLINT
      ++keyInd;
    }
  }
  keyName[keyInd] = '\0';
}

struct context_metadata *getContextMetadata(const char *name) {
  struct context_metadata *s;
  nameToKey(name);
  HASH_FIND_STR(ctx.metaMap, keyName, s);
  return s;
}

void createContextMetadata(const char *name, const char *printName,
                           context_source_t source, context_type_t type,
                           void *value, int isFlag) {
  struct context_metadata *s;
  nameToKey(name);
  HASH_FIND_STR(ctx.metaMap, keyName, s);
  if (s == NULL) {
    s = (struct context_metadata *)malloc(sizeof *s);
    strcpy(s->keyName, keyName);
    HASH_ADD_STR(ctx.metaMap, keyName, s);
  } else {
    printf("Internal error: attempt to recreate context param %s\n", name);
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  strcpy(s->printName, printName);
  s->source = source;
  s->type = type;
  s->value = value;
  s->isFlag = isFlag;
}

void updateIntContext(const char *name, int value, context_source_t source) {
  struct context_metadata *s;
  nameToKey(name);
  HASH_FIND_STR(ctx.metaMap, keyName, s); /* name already in the hash? */
  if (s == NULL) {
    printf("Internal error: no context param %s found\n", name);
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  if (hasSourcePrecedence(s, source)) {
    *(int *)(s->value) = value;
    s->source = source;
  }
}

void updateCharContext(const char *name, const char *value,
                       context_source_t source) {
  struct context_metadata *s;
  nameToKey(name);
  HASH_FIND_STR(ctx.metaMap, keyName, s); /* name already in the hash? */
  if (s == NULL) {
    printf("Internal error: no context param %s found\n", name);
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  if (hasSourcePrecedence(s, source)) {
    strncpy((char *)s->value, value, CONTEXT_CHAR_MAXLEN);
    s->source = source;
  }
}

int hasSourcePrecedence(struct context_metadata *s,
                        context_source_t newSource) {
  // If newSource is greater (or equal) to old source, then it's good
  return (s->source < newSource);
}

// Get a printable version of source enum for dumpConfig()
char *getContextSourceString(context_source_t src) {
  switch (src) {
    case CTX_DEFAULT:
      return "DEFAULT";
    case CTX_CONTEXT_FILE:
      return "INPUT_FILE";
    case CTX_COMMAND_LINE:
      return "COMMAND_LINE";
    case CTX_CALCULATED:
      return "CALCULATED";
    case CTX_TEST:
      return "TEST";
    default:
      return "UNKNOWN";
  }
}

// Sort function for printConfig
int by_name(const struct context_metadata *a,
            const struct context_metadata *b) {
  return strcmp(a->keyName, b->keyName);
}

void validateContext(void) {
  // Make sure FILENAME is set and well-sized; everything else is optional (not
  // necessary or has a default)
  if (strcmp(ctx.fileName, "") == 0) {
    printf("Error: fileName must be set for SIPNET to run\n");
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
  if (strlen(ctx.fileName) > FILENAME_MAXLEN - 10) {
    // We need room to append .clim, .param, etc
    printf("Error: fileName is too long; max length is %d characters\n",
           FILENAME_MAXLEN - 10);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }

  // Check num carbon pools in [1,3]

  // Check inter-param requirements are not violated

  printf("*******************************\n");
  printf("VALIDATE CONTEXT NOT FINISHED!!\n");
  printf("*******************************\n");
}

void printConfig(FILE *outFile) {
  // Sort alphabetically for consistency
  HASH_SORT(ctx.metaMap, by_name);  // NOLINT

  struct context_metadata *s;

  // Calculate width of value column
  unsigned int width = 0;
  for (s = ctx.metaMap; s != NULL;
       s = (struct context_metadata *)(s->hh.next)) {
    if (s->type == CTX_CHAR) {
      unsigned int len = strlen(s->printName);
      width = len > width ? len : width;
    }
  }

  // Header
  if (ctx.printHeader) {
    char timestamp[100];
    time_t current_time;
    time(&current_time);
    struct tm *utc_time = gmtime(&current_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S UTC", utc_time);
    fprintf(outFile, "Final config for SIPNET run at %s\n", timestamp);
    fprintf(outFile, "%21s %13s %*s\n", "Name", "Source", width, "Value");
  }

  // Config
  for (s = ctx.metaMap; s != NULL;
       s = (struct context_metadata *)(s->hh.next)) {

    if (s->type == CTX_INT) {
      fprintf(outFile, "%21s %13s %*d\n", s->printName,
              getContextSourceString(s->source), width, *(int *)s->value);
    } else if (s->type == CTX_CHAR) {
      fprintf(outFile, "%21s %13s %*s\n", s->printName,
              getContextSourceString(s->source), width, (char *)s->value);
    } else {
      // The height of paranoia
      printf("Internal error, unknown found for context param\n");
      exit(EXIT_CODE_INTERNAL_ERROR);
    }
  }
}
