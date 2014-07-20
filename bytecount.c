/*
 * Outputs Byte count per line of the input file.
 *
 * Compile:
 * gcc bytecount.c -o bytecount
 *
 * ./bytecount input_file
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

int main(int argc,char *argv[]){
	FILE *input_file;
	char * line = NULL;
	size_t len = 0;
	ssize_t read_nchars;
	char errormsg[256];
	
	if(argc!=2){
		printf("Usage:\n");
		printf("$0  input_file\n");
		exit(-1);
	}
	
	sprintf(errormsg,"\nERROR:Cannot open input file for reading:%s",input_file);
	
	input_file = fopen(argv[1], "r+");
	
	if (input_file == NULL) {
		error(0,errno,errormsg);
		exit(-1);
	}
	fseek(input_file, 0L, SEEK_SET);

	while ((read_nchars = getline(&line, &len, input_file)) != -1) {
		printf("%d\n", read_nchars);
	}
	free(line);
	fclose(input_file);
	return 0;
}
