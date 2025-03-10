// header file for outputItems.c

#ifndef OUTPUT_ITEMS_H
#define OUTPUT_ITEMS_H

#define OUTPUT_ITEMS_MAXNAME 64  // maximum length of output item name, including the trailing '\0'


// structure to hold an output item, and a pointer to the next (for a linked list)
typedef struct SingleOutputItemStruct  {
  char name[OUTPUT_ITEMS_MAXNAME];  // name of output item
  double *ptr;  // pointer to the variable holding this item
  FILE *f;  // output file for this output item
  
  struct SingleOutputItemStruct *nextItem;
} SingleOutputItem;


// structure to hold a bunch of SingleOutputItems
// implemented as a linked list
typedef struct OutputItemsStruct  {
  SingleOutputItem *head;
  SingleOutputItem *tail;

  int count;  // number of items in the list, not counting the dummy item at the head
  char *filenameBase;
  char separator;  // character separating values in the output files (e.g. space, tab, or comma)
} OutputItems;



/* Allocate space for a new outputItems structure, return a pointer to it
   filenameBase is the base name of the files to which we'll output
    - we'll output to <filenameBase>.<name> for each output item
   separator is the character separating values in the output files (e.g. space, tab, or comma)
 */
OutputItems *newOutputItems(char *filenameBase, char separator);


/* Add a new singleOutputItem to the end of the list given by outputItems
   strlen(name) must be < OUTPUT_ITEMS_MAXNAME
   ptr must be a pointer to the variable holding this item (double)
   
   After calling this function, the file associated with this output item will be open for writing
 */
void addOutputItem(OutputItems *outputItems, char *name, double *ptr);


/* For each output item, write a label, followed by a separator
   This is intended for writing a single label at the start of each line
   - For example, could write the location for spatial output, or the value of some variable for a sensitivity test
 */
void writeOutputItemLabels(OutputItems *outputItems, char *label);


// For each output item, write its current value, followed by a separator
void writeOutputItemValues(OutputItems *outputItems);


/* For each output item, write a newline
   This is intended to be called at the end of each run
 */
void terminateOutputItemLines(OutputItems *outputItems);


/* Free up space used by outputItems
   Also, close all files associated with the individual output items
 */
void deleteOutputItems(OutputItems *outputItems);

#endif
