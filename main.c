/*
 * main.c
 *
 *  Created on: Dec 11, 2014
 *      Author: spoofy
 */

#include "main.h"
#include <stdlib.h>
#include <string.h>
#include "lzss.h"
#include "decompressor.h"

//#define DEBUG
#define BUFFER_SIZE 8192

int main(int argc, char *argv[])
{

#ifdef DEBUG
	fprintf(stderr, "############Begin Main############\n");
#endif

	//Checks the number of arguments
	if (argc != 3)
	{
		fprintf(stderr, "Usage: lzss <c/d> <input/output file>\n");
		return 1;
	}

	if (strcmp("c", argv[1]) == 0)
	{
		//input and output initialization
		FILE *infile, *outfile;
		char *out_file_name = calloc((strlen(argv[2]) + 5),1);
		strcat(out_file_name, argv[2]);
		strcat(out_file_name, ".niko"); //fancy extension

		//open outfile in write binary append
		if (!(outfile = fopen(out_file_name, "wba")))
		{
			fprintf(stderr,"Error opening output file! : %s\n",out_file_name);
			return -1;
		}
		free(out_file_name);

		if (!(infile = fopen(argv[2], "rb")))
		{
			fprintf(stderr,"Error opening input file!: %s\n",argv[2]);
			return -1;
		}

		//variables needed
		long file_len = 0;
		long compressed_size = 0;

		//retrieves the file length
		//---that's how many bytes we can read---
		fseek(infile, 0, SEEK_END);
		file_len = ftell(infile);
		fseek(infile, 0, SEEK_SET);

		//read the data in chunk of BUFFER_SIZE
		//compress the chunk
		//write the compressed chunk
		while (file_len)
		{
			unsigned char * inbuffer = malloc(BUFFER_SIZE);
			int read_count = fread(inbuffer, sizeof(unsigned char), BUFFER_SIZE, infile);
			unsigned char *outbuffer = lzss(inbuffer, read_count, &compressed_size);
			fwrite(outbuffer, 1, compressed_size, outfile);

#ifdef DEBUG
			fprintf(stderr, "@@@@@@@@@@Writing data being written to file@@@@@@@@@@@@@\n");
			print_hex(outbuffer, compressed_size);
			for (int i = 0; i < compressed_size; i++)
			{
				print_bin(outbuffer[i], 8);
			}
			fprintf(stderr, "@@@@@@@@@@END Writing data being written to file@@@@@@@@@@@@@\n");
#endif

			free(outbuffer);
			free(inbuffer);
			file_len -= read_count;
		}

		fclose(infile);
		fclose(outfile);
	}
	else if (strcmp("d", argv[1]) == 0)
	{
		return decode(argv[2]);
	}
	else
		return -1;

#ifdef DEBUG
	fprintf(stderr, "############End Main############\n");
#endif

	return 0;
}

