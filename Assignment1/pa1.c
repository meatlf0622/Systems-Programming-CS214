#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 100

void errorExi(const char *message) {
   fprintf(stderr, "%s\n", message);
   exit(-1);
}

int main(int argc, char *argv[]) {
   if (argc != 4) {
       fprintf(stderr, "Usage: %s <filename> <r/c> <index>\n", argv[0]);
       return -1;
   }
    const char *fileName = argv[1];
    char mode = argv[2][0];
    int index = atoi(argv[3]);

    FILE *file = fopen(fileName, "r");
    if(!file){
        errorExi("Error opening file");
    }
    unsigned int data[MAX_SIZE][MAX_SIZE];
    int rows =0;
    int cols = 0; 
    char line[1024];
//read file
    while(fgets(line, sizeof(line), file) && rows < MAX_SIZE){
        char *token = strtok(line, ",");
        int col = 0;
        while(token && col < MAX_SIZE){
            data[rows][col] = (unsigned int)atoi(token) ;
            token = strtok(NULL, ",");
            col++; 
        }
        if(rows ==0){
            cols = col; 
        }
        rows++;
    }
    fclose(file);

if ((mode == 'r' && index >=rows) || (mode == 'c' && index >=cols)){
    printf("error in input format at line %d\n", index);
    exit(-1);
}
    double mean = 0; 
    unsigned int max = 0;
    unsigned int min = ~0; 
    int count = 0; 

    if(mode == 'r'){
        for(int i = 0; i < cols; i++){
            unsigned int currentVal = data [index][i];
            mean += currentVal; 
            if(currentVal > max){
                max = currentVal; 
            }
            if(currentVal < min ){
                min = currentVal; 
            }
            count ++;
        }
    } else if( mode == 'c'){
        for(int i = 0; i < rows; i++){
            unsigned int currentVal = data[i][index]; 
            mean += currentVal; 
            if(currentVal > max){
                max = currentVal; 
            }
            if(currentVal < min ){
                min = currentVal; 
            }
            count ++; 
        }
    }
    mean/=count;
    if(mode == 'r'){
        printf("%s %s %.2f %u %u\n", fileName, "row", mean, max, min );
    } else if (mode == 'c'){
        printf("%s %s %.2f %u %u\n", fileName, "column", mean, max, min );
    }
   return 0;
}
