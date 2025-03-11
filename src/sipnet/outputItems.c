/* outputItems: structures and functions to output single variables to files

   Author: Bill Sacks
   Creation date: 6/4/07
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "outputItems.h"
#include "common/util.h"

// Private/helper functions: not defined in outputItems.h:

/* Allocate space for a new singleOutputItem structure, return a pointer to it
   Name and ptr will have the given values; f and nextItem will be NULL
*/
SingleOutputItem *newSingleOutputItem(char *name, double *ptr)  {
  SingleOutputItem *singleOutputItem; 

  if (strlen(name) >= OUTPUT_ITEMS_MAXNAME)  {
    printf("ERROR in newSingleOutputItem: name '%s' exceeds maximum length of %d\n", name, OUTPUT_ITEMS_MAXNAME);
    exit(1);
  }

  singleOutputItem = (SingleOutputItem *)malloc(sizeof(SingleOutputItem));

  strcpy(singleOutputItem->name, name);
  singleOutputItem->ptr = ptr;
  singleOutputItem->f = NULL;
  singleOutputItem->nextItem = NULL;

  return singleOutputItem;
}


/* Open the file associated with this output file
    - This file will be named <filenameBase>.<name>
 */ 
void openOutputItemFile(SingleOutputItem *singleOutputItem, char *filenameBase)  {
  char *filename;

  filename = (char *)malloc((strlen(filenameBase) + strlen(singleOutputItem->name) + 2) * sizeof(char));
  strcpy(filename, filenameBase);
  strcat(filename, ".");
  strcat(filename, singleOutputItem->name);
  singleOutputItem->f = openFile(filename, "w");
  
  free(filename);
}


// close the file associated with this output item
void closeOutputItemFile(SingleOutputItem *singleOutputItem)  {
  fclose(singleOutputItem->f);
  singleOutputItem->f = NULL;
}



/*************************************************/

// Public functions: defined in outputItems.h

/* Allocate space for a new outputItems structure, return a pointer to it
   filenameBase is the base name of the files to which we'll output
    - we'll output to <filenameBase>.<name> for each output item
   separator is the character separating values in the output files (e.g. space, tab, or comma)
 */
OutputItems *newOutputItems(char *filenameBase, char separator)  {
  OutputItems *outputItems;

  outputItems = (OutputItems *)malloc(sizeof(OutputItems));
  
  outputItems->head = newSingleOutputItem("", NULL);  // we'll keep a dummy item at the head of the list
  outputItems->tail = outputItems->head;
  outputItems->count = 0;
 
  outputItems->filenameBase = (char *)malloc((strlen(filenameBase) + 1) * sizeof(char));
  strcpy(outputItems->filenameBase, filenameBase);

  outputItems->separator = separator;

  return outputItems;
}


/* Add a new singleOutputItem to the end of the list given by outputItems
   strlen(name) must be < OUTPUT_ITEMS_MAXNAME
   ptr must be a pointer to the variable holding this item (double)
   
   After calling this function, the file associated with this output item will be open for writing
 */
void addOutputItem(OutputItems *outputItems, char *name, double *ptr)  {
  SingleOutputItem *singleOutputItem;

  singleOutputItem = newSingleOutputItem(name, ptr);
  openOutputItemFile(singleOutputItem, outputItems->filenameBase);

  outputItems->tail->nextItem = singleOutputItem;
  outputItems->tail = singleOutputItem;
  outputItems->count++;
}


/* For each output item, write a label, followed by a separator
   This is intended for writing a single label at the start of each line
   - For example, could write the location for spatial output, or the value of some variable for a sensitivity test
 */
void writeOutputItemLabels(OutputItems *outputItems, char *label)  {
  SingleOutputItem *singleOutputItem;

  singleOutputItem = outputItems->head->nextItem;
  while(singleOutputItem != NULL)  {
    fprintf(singleOutputItem->f, "%s%c", label, outputItems->separator);
    singleOutputItem = singleOutputItem->nextItem;
  }
}


// For each output item, write its current value, followed by a separator
void writeOutputItemValues(OutputItems *outputItems)  {
  SingleOutputItem *singleOutputItem;

  singleOutputItem = outputItems->head->nextItem;
  while (singleOutputItem != NULL)  {
    fprintf(singleOutputItem->f, "%f%c", *(singleOutputItem->ptr), outputItems->separator);
    singleOutputItem = singleOutputItem->nextItem;
  }
}


/* For each output item, write a newline
   This is intended to be called at the end of each run
 */
void terminateOutputItemLines(OutputItems *outputItems)  {
  SingleOutputItem *singleOutputItem;

  singleOutputItem = outputItems->head->nextItem;
  while(singleOutputItem != NULL)  {
    fprintf(singleOutputItem->f, "\n");
    singleOutputItem = singleOutputItem->nextItem;
  }
}


/* Free up space used by outputItems
   Also, close all files associated with the individual output items
 */
void deleteOutputItems(OutputItems *outputItems)  {
  SingleOutputItem *curr, *temp;

  curr = outputItems->head;
  while (curr != NULL)  {
    temp = curr;
    curr = curr->nextItem;
    
    if (temp->f != NULL)  {
      closeOutputItemFile(temp);
      free(temp);
    }
  }

  free(outputItems->filenameBase);
  free(outputItems);
}
