// header file for namelistInput.c
// includes definitions of NamelistInputItem, namelistInputs structures

#ifndef NAMELIST_INPUT_H
#define NAMELIST_INPUT_H

#define NAMELIST_INPUT_MAXNAME 64  // maximum length of a parameter name, including the trailing '\0'

// define named constants for the different possible input types
enum INPUT_TYPES { INT_TYPE, LONG_TYPE, DOUBLE_TYPE, STRING_TYPE };

// structure to hold a namelist input item, and a pointer to the next (for a linked list)
typedef struct NamelistInputItemStruct {
  char name[NAMELIST_INPUT_MAXNAME];  // name of parameter (must match name given in input file)
  int type;  // type of parameter (one of INPUT_TYPES)
  void *ptr;  /* pointer to the variable holding this parameter
		 (in the case of a string, ptr will be of type *char) */
  int maxlen;  /* for STRING_TYPE, holds the maximum string length (including the trailing '\0'); 
		  ignored for other types */
  int wasRead;  // 1 if this input item was read from the input file, 0 if not

  struct NamelistInputItemStruct *nextItem;
} NamelistInputItem;


// structure to hold a bunch of NamelistInputItems
// implemented as a linked list
typedef struct NamelistInputsStruct {
  NamelistInputItem *head;
  NamelistInputItem *tail;

  int count;  // number of items in the list, not counting the dummy item at the head
} NamelistInputs;


// allocate space for a new namelistInputs structure, return a pointer to it
NamelistInputs *newNamelistInputs();


/* Add a new namelistInputItem to the end of the list given by namelistInputs
   strlen(name) must be < NAMELIST_INPUT_MAXNAME
   type must be one of the types given by INPUT_TYPES
   ptr must be a pointer to the variable holding this parameter value
   maxlen is ignored if type is anything other than STRING_TYPE, but it still must be supplied
   - it specifies the maximum string length of the variable pointed to by ptr (including the trailing '\0')
*/
void addNamelistInputItem(NamelistInputs *namelistInputs, char *name, int type, void *ptr, int maxlen);


/* Read all namelist inputs from file given by fileName, put them in the locations pointed to by each inputs' ptr variable

   Structure of input file: Each line contains:
       name value
   where the following are valid separators: space, tab, =, :

   A value of 'none' for a string type is translated into the empty string

   The order of input items in the input file does not matter

   ! is a comment character: anything after a ! on a line is ignored
 */
void readNamelistInputs(NamelistInputs *namelistInputs, const char *fileName);


/* If the input item with the given name wasn't read,
   then kill the program and print an error message
 */
void dieIfNotRead(NamelistInputs *namelistInputs, char *name);


/* For string types only:
   Check to make sure the given item:
   (1) was read from the input file
   (2) was set to a non-empty value (i.e. wasn't set to 'none')
   Kill the program and print an error message if either of these conditions are not met
 */ 
void dieIfNotSet(NamelistInputs *namelistInputs, char *name);


// free up space used by namelistInputs
void deleteNamelistInputs(NamelistInputs *namelistInputs);

#endif
