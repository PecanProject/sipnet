// Bill Sacks
// 7/8/02

// front end for sipnet - set up environment and call the appropriate run
// function

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/context.h"
#include "common/exitCodes.h"
#include "common/logging.h"
#include "common/modelParams.h"
#include "common/util.h"

#include "cli.h"
#include "events.h"
#include "sipnet.h"
#include "outputItems.h"

void checkRuntype(const char *runType) {
  if (strcasecmp(runType, "standard") != 0) {
    // Make sure this is not an old config with a different RUNTYPE set
    printf("SIPNET only supports the standard runtype mode; other options are "
           "obsolete and were last supported in v1.3.0\n");
    printf("Please fix %s and re-run\n", ctx.inputFile);
    exit(EXIT_CODE_BAD_PARAMETER_VALUE);
  }
}

void readInputFile(void) {
  // First, make sure the filename is valid
  validateFilename();
  printf("Reading config from file %s\n", ctx.inputFile);

  const char *SEPARATORS = " \t=:";  // characters that can separate names from
                                     // values in input file
  const char *COMMENT_CHARS = "!";  // comment characters (ignore everything
                                    // after this on a line)
  const char *EMPTY_STRING = "none";  // if this is the value of a string type,
                                      // we take it to mean the empty string

  FILE *infile;

  char line[1024];
  char allSeparators[16];
  char *inputName;  // name of input item just read in
  char *inputValue;  // value of input item just read in, as a string
  char *errc = "";
  int isComment;

  strcpy(allSeparators, SEPARATORS);
  strcat(allSeparators, "\n\r");
  infile = openFile(ctx.inputFile, "r");

  while (fgets(line, sizeof(line), infile) != NULL) {  // while not EOF or error
    // remove trailing comments:
    isComment = stripComment(line, COMMENT_CHARS);

    if (!isComment) {  // if this isn't just a comment line or blank line
      // tokenize line:
      inputName = strtok(line, SEPARATORS);
      inputValue = strtok(NULL, allSeparators);

      // Handle RUNTYPE as an obsolete param; if it isn't set, consider it to be
      // set to "standard"; and make sure it is that if set
      if (strcasecmp(inputName, "runtype") == 0) {
        checkRuntype(inputValue);
        continue;
      }

      // Find the metadata so we know what to do with this param
      struct context_metadata *ctx_meta = getContextMetadata(inputName);
      if (ctx_meta == NULL) {
        logWarning("ignoring input file parameter %s\n", inputName);
        continue;
      }

      if (inputValue == NULL) {
        printf("Error in input file: No value given for input item %s\n",
               inputName);
        printf("Please fix %s and re-run\n", ctx.inputFile);
        exit(EXIT_CODE_BAD_PARAMETER_VALUE);
      }

      switch (ctx_meta->type) {
        case CTX_INT: {
          int intVal = strtol(inputValue, &errc, 0);  // NOLINT
          if (strlen(errc) > 0) {  // invalid character(s) in input string
            printf("ERROR in input file: Invalid value for %s: %s\n", inputName,
                   inputValue);
            printf("Please fix %s and re-run\n", ctx.inputFile);
            exit(EXIT_CODE_BAD_PARAMETER_VALUE);
          }
          updateIntContext(inputName, intVal, CTX_CONTEXT_FILE);
        } break;
        case CTX_CHAR: {
          if (strcmp(inputValue, EMPTY_STRING) == 0) {
            // if the value is specified as the value which signifies the
            // empty string
            updateCharContext(inputName, "", CTX_CONTEXT_FILE);
          } else {
            if (strlen(inputValue) >= CONTEXT_CHAR_MAXLEN) {
              printf("ERROR in input file: value '%s' exceeds maximum "
                     "length for %s (%d)\n",
                     inputValue, inputName, CONTEXT_CHAR_MAXLEN);
              printf(
                  "Please fix %s, or change the maximum length, and re-run\n",
                  ctx.inputFile);
              exit(EXIT_CODE_BAD_PARAMETER_VALUE);
            }
            updateCharContext(inputName, inputValue, CTX_CONTEXT_FILE);
          }
        } break;
        default:
          printf("ERROR in readInputFile: Unrecognized type for %s: %d\n",
                 inputName, ctx_meta->type);
          exit(EXIT_CODE_INTERNAL_ERROR);
      }
    }  // if (!isComment)
  }  // while not EOF or error

  fclose(infile);
}

int main(int argc, char *argv[]) {

  FILE *out, *outConfig;

  ModelParams *modelParams;  // the parameters used in the model (possibly
                             // spatially-varying)
  OutputItems *outputItems;  // structure to hold information for output to
                             // single-variable files (if doSingleOutputs is
                             // true)

  // char fileName[FILENAME_MAXLEN - 8];
  char outFile[FILENAME_MAXLEN], outConfigFile[FILENAME_MAXLEN];
  char paramFile[FILENAME_MAXLEN], climFile[FILENAME_MAXLEN];

  // 1. Initialize Context with default values
  initContext();
  // Also need to verify the CLI name map, now that the context has been init'd
  checkCLINameMap();

  // 2. Parse command line args
  parseCommandLineArgs(argc, argv);

  // 3. Read input config file
  // Note: command-line args have precedence
  // read from input file
  readInputFile();

  // 4. Run some checks
  validateContext();

  // 5. Set calculated parameters
  strcpy(paramFile, ctx.fileName);
  strcat(paramFile, ".param");
  updateCharContext("paramFile", paramFile, CTX_CALCULATED);
  strcpy(climFile, ctx.fileName);
  strcat(climFile, ".clim");
  updateCharContext("climFile", climFile, CTX_CALCULATED);
  if (ctx.doMainOutput) {
    strcpy(outFile, ctx.fileName);
    strcat(outFile, ".out");
    updateCharContext("outFile", outFile, CTX_CALCULATED);
    out = openFile(outFile, "w");
  } else {
    out = NULL;
  }

  // Lastly - do after all other config processing
  if (ctx.dumpConfig) {
    strcpy(outConfigFile, ctx.fileName);
    strcat(outConfigFile, ".config");
    updateCharContext("outConfigFile", outConfigFile, CTX_CALCULATED);
    outConfig = openFile(outConfigFile, "w");
    printConfig(outConfig);
    fclose(outConfig);
  } else {
    outConfig = NULL;
  }

  // 6. Initialize model, events, outputItems
  initModel(&modelParams, paramFile, climFile);

  if (ctx.events) {
    initEvents(EVENT_IN_FILE, ctx.printHeader);
  }

  if (ctx.doSingleOutputs) {
    outputItems = newOutputItems(ctx.fileName, ' ');
    setupOutputItems(outputItems);
  } else {
    outputItems = NULL;
  }

  // 7. Do the run!
  runModelOutput(out, outputItems, ctx.printHeader);

  // 8. Cleanup
  if (ctx.doMainOutput) {
    fclose(out);
  }

  cleanupModel();
  if (outputItems != NULL) {
    deleteOutputItems(outputItems);
  }

  return EXIT_CODE_SUCCESS;
}
