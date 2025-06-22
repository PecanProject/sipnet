#include "context.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common/exitCodes.h"

#define DEFAULT_INPUT_FILE "sipnet.in"
#define RUN_TYPE_STANDARD "standard"

struct Context ctx;

// Temp space for name-to-key conversions
static char keyName[CONTEXT_CHAR_MAXLEN];

// Default values for all context fields
void initContext(void) {
  // Init hash map to NULL before adding anything to it
  ctx.metaMap = NULL;

  // Init the params
  // Flags, default on
  CREATE_INT_CONTEXT(doMainOutput, "DO_MAIN_OUTPUT", 1, CTX_DEFAULT);
  CREATE_INT_CONTEXT(events, "EVENTS", 1, CTX_DEFAULT);
  CREATE_INT_CONTEXT(printHeader, "PRINT_HEADER", 1, CTX_DEFAULT);

  // Flags, default off
  CREATE_INT_CONTEXT(doSingleOutputs, "DO_SINGLE_OUTPUT", 0, CTX_DEFAULT);
  CREATE_INT_CONTEXT(dumpConfig, "DUMP_CONFIG", 0, CTX_DEFAULT);
  CREATE_INT_CONTEXT(quiet, "QUIET", 0, CTX_DEFAULT);

  // Files
  CREATE_CHAR_CONTEXT(paramFile, "PARAM_FILE", "", CTX_DEFAULT);
  CREATE_CHAR_CONTEXT(climFile, "CLIM_FILE", "", CTX_DEFAULT);
  CREATE_CHAR_CONTEXT(outFile, "OUT_FILE", "", CTX_DEFAULT);
  CREATE_CHAR_CONTEXT(outConfigFile, "OUT_CONFIG_FILE", "", CTX_DEFAULT);
  CREATE_CHAR_CONTEXT(inputFile, "INPUT_FILE", DEFAULT_INPUT_FILE, CTX_DEFAULT);

  // Other
  // Would like to rename as SITE_NAME, if that wasn't breaking
  CREATE_CHAR_CONTEXT(fileName, "FILENAME", "", CTX_DEFAULT);
  // For compatibility
  CREATE_CHAR_CONTEXT(runType, "RUN_TYPE", RUN_TYPE_STANDARD, CTX_DEFAULT);
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

context_source_t getContextSource(const char *name) {
  return getContextMetadata(name)->source;
}

struct context_metadata *getContextMetadata(const char *name) {
  struct context_metadata *s;
  nameToKey(name);
  HASH_FIND_STR(ctx.metaMap, keyName, s);
  if (s == NULL) {
    printf("Internal error: context metadata for param %s not found\n", name);
    exit(EXIT_CODE_INTERNAL_ERROR);
  }
  return s;
}

void createContextMetadata(const char *name, const char *printName,
                           context_source_t source, context_type_t type,
                           void *value) {
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

void printConfig(FILE *outFile) {
  // Sort alphabetically for consistency
  HASH_SORT(ctx.metaMap, by_name);  // NOLINT

  struct context_metadata *s;

  // Header
  if (ctx.printHeader) {
    char timestamp[100];
    time_t current_time;
    time(&current_time);
    struct tm *utc_time = gmtime(&current_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S UTC", utc_time);
    fprintf(outFile, "Final config for SIPNET run at %s\n", timestamp);
    fprintf(outFile, "%20s %20s %30s\n", "Name", "Source", "Value");
  }

  // Config
  for (s = ctx.metaMap; s != NULL;
       s = (struct context_metadata *)(s->hh.next)) {

    // don't print RUN_TYPE, it's only in the Context for compatibility
    nameToKey("runType");
    if (strcmp(s->keyName, keyName) == 0) {
      continue;
    }

    if (s->type == CTX_INT) {
      fprintf(outFile, "%20s %20s %30d\n", s->printName,
              getContextSourceString(s->source), *(int *)s->value);
    } else if (s->type == CTX_CHAR) {
      fprintf(outFile, "%20s %20s %30s\n", s->printName,
              getContextSourceString(s->source), (char *)s->value);
    } else {
      // The height of paranoia
      printf("Internal error, unknown found for context param\n");
      exit(EXIT_CODE_INTERNAL_ERROR);
    }
  }
}
