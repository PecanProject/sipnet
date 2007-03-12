

#include <stdio.h>
#include <string.h>



  #define NUM_PARAMS 84
  
  

// If fileName.param is not created, then do so 
  
int main(int argc, char *argv[])
 { 
  	
  	char paramList[NUM_PARAMS][100];
  	char line[256];
  	int counter;
  	
  	char inputFile[256];
  	char outFile[256];
  	
  	strcpy(inputFile,argv[1]);
 	strcpy(outFile,argv[1]);
 
  	strcat(inputFile, ".parameters");
  	strcat(outFile, ".param");
  	
 	FILE *fileIn = fopen( inputFile, "r");

 	counter=0;
 	
 	while( (fgets(line,256,fileIn) != NULL) | (counter<NUM_PARAMS) )  { 
 	 
  		if ( (strncmp(line,"*",1)!=0) & (strncmp(line,"\n",1)!=0) ) {
  			strcpy(paramList[counter],line);
  			
  			//if (counter==NUM_PARAMETERS-1) strcat(niwots[counter],"\n");
			counter++;
  		}
  		
  	}
  	
	fclose(fileIn);

  	qsort((char *)paramList, sizeof(paramList)/sizeof(paramList[0]), sizeof(*paramList), strcmp );
  	
 	FILE *fileOut = fopen( outFile, "w");
    for (counter = 0; counter < NUM_PARAMS; counter++) fprintf(fileOut,"%s",paramList[counter]);
    
    fclose(fileOut);
    return(0);
 }
