/* namelistInput: structure and functions to provide some of the functionality
   provided by FORTRAN's NAMELISTs

   That is, read a bunch of input items from a file, where each item is given in
   one of the forms: name = value name: value name value

   Author: Bill Sacks
   Creation date: 5/18/07
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
//#include <stdarg.h>
#include "namelistInput.h"  // includes definition of structures
#include "util.h"

// Private/helper functions: not defined in namelistInput.h

// Allocate space for a new namelistInputItem structure, return a pointer to it
// Name, type, ptr and maxlen will have the given values; nextItem will be null
// Note that if type is anything other than STRING_TYPE, maxlen is unused (but
// still must be supplied)
NamelistInputItem *newNamelistInputItem(char *name, int type, void *ptr,
                                        int maxlen) {
  NamelistInputItem *namelistInputItem;

  if (strlen(name) >= NAMELIST_INPUT_MAXNAME) {
    printf("ERROR in newNamelistInputItem: parameter name '%s' exceeds maximum "
           "length of %d\n",
           name, NAMELIST_INPUT_MAXNAME);
    exit(1);
  }

  namelistInputItem = (NamelistInputItem *)malloc(sizeof(NamelistInputItem));

  strcpy(namelistInputItem->name, name);
  namelistInputItem->type = type;
  namelistInputItem->ptr = ptr;
  namelistInputItem->maxlen = maxlen;
  namelistInputItem->wasRead = 0;

  namelistInputItem->nextItem = NULL;

  return namelistInputItem;
}

// find input item with given name in namelistInputs list
// return a pointer to this input item
// if not found, return NULL
NamelistInputItem *locateNamelistInputItem(NamelistInputs *namelistInputs,
                                           char *name) {
  NamelistInputItem *namelistInputItem;

  namelistInputItem = namelistInputs->head;

  while ((namelistInputItem != NULL) &&
         (strcmpIgnoreCase(namelistInputItem->name, name) != 0)) {
    namelistInputItem = namelistInputItem->nextItem;
  }

  return namelistInputItem;
}

/*************************************************/

// Public functions: defined in namelistInput.h

// allocate space for a new namelistInputs structure, return a pointer to it
NamelistInputs *newNamelistInputs() {
  NamelistInputs *namelistInputs;

  namelistInputs = (NamelistInputs *)malloc(sizeof(NamelistInputs));

  namelistInputs->head = newNamelistInputItem("", -1, NULL, 0);  // we'll keep a
                                                                 // dummy item
                                                                 // at the head
                                                                 // of the list
  namelistInputs->tail = namelistInputs->head;
  namelistInputs->count = 0;

  return namelistInputs;
}

/* Add a new namelistInputItem to the end of the list given by namelistInputs
   strlen(name) must be < NAMELIST_INPUT_MAXNAME
   type must be one of the types given by INPUT_TYPES
   ptr must be a pointer to the variable holding this parameter value
   maxlen is ignored if type is anything other than STRING_TYPE, but it still
   must be supplied
   - it specifies the maximum string length of the variable pointed to by ptr
   (including the trailing '\0')
*/
void addNamelistInputItem(NamelistInputs *namelistInputs, char *name, int type,
                          void *ptr, int maxlen) {
  NamelistInputItem *namelistInputItem;

  namelistInputItem = newNamelistInputItem(name, type, ptr, maxlen);
  namelistInputs->tail->nextItem = namelistInputItem;
  namelistInputs->tail = namelistInputItem;
  namelistInputs->count++;
}

/* Read all namelist inputs from file given by fileName, put them in the
   locations pointed to by each inputs' ptr variable

   Structure of input file: Each line contains:
       name value
   where the following are valid separators: space, tab, =, :

   A value of 'none' for a string type is translated into the empty string

   The order of input items in the input file does not matter

   ! is a comment character: anything after a ! on a line is ignored
 */
void readNamelistInputs(NamelistInputs *namelistInputs, const char *fileName) {
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
  NamelistInputItem *namelistInputItem;

  strcpy(allSeparators, SEPARATORS);
  strcat(allSeparators, "\n\r");
  infile = openFile(fileName, "r");

  while (fgets(line, sizeof(line), infile) != NULL) {  // while not EOF or error
    // remove trailing comments:
    isComment = stripComment(line, COMMENT_CHARS);

    if (!isComment) {  // if this isn't just a comment line or blank line
      // tokenize line:
      inputName = strtok(line, SEPARATORS);  // make inputName point to first
                                             // token
      inputValue = strtok(NULL, allSeparators);  // make inputValue point to
                                                 // next token (e.g. after the
                                                 // '=')

      // printf("%s: <%s>\n", inputName, inputValue);

      namelistInputItem = locateNamelistInputItem(namelistInputs, inputName);
      if (namelistInputItem == NULL) {
        printf("ERROR in readNamelistInputs: Read unexpected input item: %s\n",
               inputName);
        printf("Please fix %s and re-run\n", fileName);
        exit(1);
      }
      // otherwise, we have found the item with this name

      if (inputValue == NULL) {
        printf(
            "ERROR in readNamelistInputs: No value given for input item %s\n",
            inputName);
        printf("Please fix %s and re-run\n", fileName);
        exit(1);
      }

      switch (namelistInputItem->type) {
        case INT_TYPE:
          *(int *)(namelistInputItem->ptr) = strtol(inputValue, &errc, 0);
          if (strlen(errc) > 0) {  // invalid character(s) in input string
            printf("ERROR in readNamelistInputs: Invalid value for %s: %s\n",
                   inputName, inputValue);
            printf("Please fix %s and re-run\n", fileName);
            // printf("%d <%s>\n", (int)strlen(errc), errc);
            exit(1);
          }
          break;
        case LONG_TYPE:
          *(long *)(namelistInputItem->ptr) = strtol(inputValue, &errc, 0);
          if (strlen(errc) > 0) {  // invalid character(s) in input string
            printf("ERROR in readNamelistInputs: Invalid value for %s: %s\n",
                   inputName, inputValue);
            printf("Please fix %s and re-run\n", fileName);
            // printf("%d <%s>\n", (int)strlen(errc), errc);
            exit(1);
          }
          break;
        case DOUBLE_TYPE:
          *(double *)(namelistInputItem->ptr) = strtod(inputValue, &errc);
          if (strlen(errc) > 0) {  // invalid character(s) in input string
            printf("ERROR in readNamelistInputs: Invalid value for %s: %s\n",
                   inputName, inputValue);
            printf("Please fix %s and re-run\n", fileName);
            // printf("%d <%s>\n", (int)strlen(errc), errc);
            exit(1);
          }
          break;
        case STRING_TYPE:
          if (strcmp(inputValue, EMPTY_STRING) == 0) {
            // if the value is specified as the value which signifies the empty
            // string
            strcpy((char *)(namelistInputItem->ptr), "");
          } else {
            if (strlen(inputValue) >= namelistInputItem->maxlen) {
              printf("ERROR in readNamelistInputs: value '%s' exceeds maximum "
                     "length for %s (%d)\n",
                     inputValue, inputName, namelistInputItem->maxlen);
              printf(
                  "Please fix %s, or change the maximum length, and re-run\n",
                  fileName);
              exit(1);
            }
            strcpy((char *)(namelistInputItem->ptr), inputValue);
          }
          break;
        default:
          printf("ERROR in readNamelistInputs: Unrecognized type for %s: %d\n",
                 inputName, namelistInputItem->type);
          exit(1);
      }

      namelistInputItem->wasRead = 1;

    }  // if (!isComment)
  }  // while not EOF or error

  fclose(infile);
}

/* If the input item with the given name wasn't read,
   then kill the program and print an error message
 */
void dieIfNotRead(NamelistInputs *namelistInputs, char *name) {
  NamelistInputItem *namelistInputItem;

  namelistInputItem = locateNamelistInputItem(namelistInputs, name);
  if (namelistInputItem == NULL) {
    printf("ERROR in dieIfNotRead: Can't find input item '%s'\n", name);
    exit(1);
  }

  if (!(namelistInputItem->wasRead)) {
    printf("ERROR: '%s' must be present in input file\n", name);
    exit(1);
  }
}

/* For string types only:
   Check to make sure the given item:
   (1) was read from the input file
   (2) was set to a non-empty value (i.e. wasn't set to 'none')
   Kill the program and print an error message if either of these conditions are
   not met
 */
void dieIfNotSet(NamelistInputs *namelistInputs, char *name) {
  NamelistInputItem *namelistInputItem;

  namelistInputItem = locateNamelistInputItem(namelistInputs, name);
  if (namelistInputItem == NULL) {
    printf("ERROR in dieIfNotSet: Can't find input item '%s'\n", name);
    exit(1);
  }
  if (namelistInputItem->type != STRING_TYPE) {
    printf("ERROR in dieIfNotSet: %s not of string_type\n", name);
    printf(" Use dieIfNotRead instead\n");
    exit(1);
  }

  dieIfNotRead(namelistInputs, name);
  if (strcmp((char *)namelistInputItem->ptr, "") == 0) {
    printf("ERROR: '%s' must not be set to 'none' or the empty string\n", name);
    exit(1);
  }
}

// free up space used by namelistInputs
void deleteNamelistInputs(NamelistInputs *namelistInputs) {
  NamelistInputItem *curr, *temp;

  curr = namelistInputs->head;
  while (curr != NULL) {
    temp = curr;
    curr = curr->nextItem;
    free(temp);
  }

  free(namelistInputs);
}
