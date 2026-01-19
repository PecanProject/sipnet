// Definition of global context struct
//
// Used to encapsulate program options defined in context file and command
// options
#ifndef CONTEXT_H
#define CONTEXT_H

#define CONTEXT_CHAR_MAXLEN 256
// For convenience
#define FILENAME_MAXLEN CONTEXT_CHAR_MAXLEN

#include <stdio.h>

#include "common/uthash.h"

typedef enum ContextSource {
  // These are in order of precedence, higher value wins. CALCULATED should be
  // orthogonal to CONTEXT_FILE and COMMAND_LINE.
  CTX_DEFAULT = 0,
  CTX_CONTEXT_FILE = 1,
  CTX_COMMAND_LINE = 2,
  CTX_CALCULATED = 3,
  CTX_TEST = 4  // set by a test, always wins
} context_source_t;

typedef enum ContextType { CTX_INT = 0, CTX_CHAR = 1 } context_type_t;

struct context_metadata {
  char keyName[CONTEXT_CHAR_MAXLEN];  // hash key
  char printName[CONTEXT_CHAR_MAXLEN];
  context_source_t source;
  context_type_t type;
  void *value;  // pointer to the member of Context that holds the value
  int isFlag;  // flags are handled slightly different than non-flag ints
  UT_hash_handle hh;  // makes this structure hashable
};

// See docs/developer-guide/cli-options.md for details on how to add a new
// Context entry
struct Context {
  // Flags
  // * Model options
  int events;
  int gdd;
  int growthResp;
  int leafWater;
  int litterPool;
  int snow;
  int soilPhenol;
  int waterHResp;
  int nitrogenCycle;

  // * I/O
  int doMainOutput;
  int doSingleOutputs;
  int dumpConfig;
  int printHeader;
  int quiet;

  // Files
  char paramFile[CONTEXT_CHAR_MAXLEN];
  char climFile[CONTEXT_CHAR_MAXLEN];
  char outFile[CONTEXT_CHAR_MAXLEN];
  char outConfigFile[CONTEXT_CHAR_MAXLEN];
  char inputFile[CONTEXT_CHAR_MAXLEN];

  // Other
  // File prefix for climate and param files
  char fileName[CONTEXT_CHAR_MAXLEN];

  // Temp space for handling command line flag args; we do not write directly
  // the params since we want to do a precedence check first. If the new source
  // has higher precedence than the current, we will copy this value to the
  // respective param.
  int tmpFlag;

  // Hash map storing metadata for context values
  struct context_metadata *metaMap;
};

// The one and only Context struct
extern struct Context ctx;

/*!
 * Initialize the global context struct with default values
 */
void initContext(void);

struct context_metadata *getContextMetadata(const char *name);

void createContextMetadata(const char *name, const char *printName,
                           context_source_t source, context_type_t type,
                           void *value, int isFlag);

void updateIntContext(const char *name, int value, context_source_t source);

void updateCharContext(const char *name, const char *value,
                       context_source_t source);

int hasSourcePrecedence(struct context_metadata *s, context_source_t newSource);

char *getContextSourceString(context_source_t src);

void validateFilename(void);

void validateContext(void);

void printConfig(FILE *outFile);

#define CREATE_INT_CONTEXT(name, printName, value, flag)                       \
  do {                                                                         \
    ctx.name = value;                                                          \
    createContextMetadata(#name, printName, CTX_DEFAULT, CTX_INT, &ctx.name,   \
                          flag);                                               \
  } while (0)

#define CREATE_CHAR_CONTEXT(name, printName, value)                            \
  do {                                                                         \
    strncpy(ctx.name, value, CONTEXT_CHAR_MAXLEN);                             \
    createContextMetadata(#name, printName, CTX_DEFAULT, CTX_CHAR, ctx.name,   \
                          0);                                                  \
  } while (0)

#endif  // CONTEXT_H
