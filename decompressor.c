/*
 * decompressor.c
 *
 *  Created on: Dec 18, 2014
 *      Author: spoofy
 */

//libraries
#include "decompressor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools.h"
//#define DEBUG
#define BUFFER_SIZE 8192
//Do not exceed 4096 or risk of buffer overflows might occur when a match of length longer than 8192 or index = 8192
#define LOOKAHEADBUFFER 4096 	//Maximum defined lookahead buffer
#define LOOKBEHINDBUFFER 4096 	//Maximum defined lookbehind buffer
#define LA 0		//Index of the buffer array for LookAhead

//main function. It's only necessary to provide the file name
int decode(char * input_file)
{

#ifdef DEBUG
	fprintf(stderr, "############Begin Decompression############\n");
#endif

	//allocation of the file pointer
	FILE *infile;
	//open file for reading (in binary mode)
	if ((infile = fopen(input_file, "rb")) != NULL)
	{

		//file is accessible
#ifdef DEBUG
		fprintf(stderr, "FILE IS ACCESSIBLE\n");
#endif

		//retrieves the file length
		//---that's how many bytes we can read---
		fseek(infile, 0, SEEK_END);
		long file_len = ftell(infile);
		fseek(infile, 0, SEEK_SET);
		while (file_len)
		{
#ifdef DEBUG
			fprintf(stderr, "file length: %ld\n", file_len);
#endif

			//We read block by block and send them over to the decompressor

			//the first 2 bytes read are the number of flags used
			unsigned char * used_flags_buffer = malloc(2 * sizeof(unsigned char*));
			file_len -= fread(used_flags_buffer, 1, 2, infile);

			//from this block we calculate how many flags are used for this specific block
			short used_flags = 0;
			used_flags |= used_flags_buffer[0] << 8;
			used_flags |= used_flags_buffer[1];

			//free the memory
			free(used_flags_buffer);

#ifdef DEBUG
			fprintf(stderr, "Used flags: %d\n", used_flags);
#endif

			//Since we flag any amount of symbols, we probably waste some flags while encoding due to the fact that we write 8 bits per time (8 flags per time)
			//we simply calculate how many wasted flags we will find in the beginning of the message
			int wasted_flags = (used_flags % 8 != 0) ? 8 - (used_flags % 8) : 0;

			//it's also handy to know how many bytes we used for the flags
			int bytes_used_by_flags = (used_flags + wasted_flags) / 8;

#ifdef DEBUG
			fprintf(stderr, "Wasted flags: %d\n", wasted_flags);
			fprintf(stderr, "Bytes used by the flags: %d\n", bytes_used_by_flags);
#endif

			//we read the flags from the file
			unsigned char * flags = malloc(bytes_used_by_flags * sizeof *flags);
			int test = fread(flags, 1, bytes_used_by_flags, infile);
			file_len -= test;

#ifdef DEBUG
			fprintf(stderr, "Bytes read/expected: %d/%d\n", test, bytes_used_by_flags);
			fprintf(stderr, "Flags:\n");
			for (int i = 0; i < test; i++)
			print_bin(flags[i], 8);
#endif

			//we need to read exactly what we need
			//flag bit = 0 -> not compressed -> 8 bits
			//flag bit = 1 -> compressed -> 26 bits
			//we read 8 bits per time, rounding up
			//a function does the math and returns the bits to read
			int bits_to_read = get_block_size(flags, wasted_flags, used_flags, bytes_used_by_flags);
			//we also want to know how many bytes it takes
			int bytes_to_read = (bits_to_read % 8 == 0) ? bits_to_read / 8 : bits_to_read / 8 + 1;

			//we allocate enough memory to read the compressed block
			unsigned char * compressed_block = malloc(bytes_to_read);

#ifdef DEBUG
			fprintf(stderr, "Block size is: %d Bytes\n", bytes_to_read);
#endif

			//we read and verify how much we've actually read
			int block_size = fread(compressed_block, 1, bytes_to_read, infile);
			file_len -= block_size;
#ifdef DEBUG
			fprintf(stderr, "Read bytes: %d\nExepcted max: %d\n", block_size, bytes_to_read);
#endif

			//allocate a variable where the decompressor will store the length of the message decompressed (it's unknown at this point)
			unsigned int decompressed_length = 0;

			//decompress the block and point the pointer
			unsigned char * message = decompress(flags, compressed_block, block_size, wasted_flags, used_flags, &decompressed_length);

			//free the memory
			free(compressed_block);
			free(flags);

			//prepare the name for the decompressed file
			char *pCh = strstr(input_file, ".niko");
			pCh[0] = 0;

			//initialize the file
			FILE *outfile;
			if ((outfile = fopen(input_file, "wab")) != 0)
			{
#ifdef DEBUG
				fprintf(stderr, "length: %d\n", decompressed_length);
				for (int i = 0; i < decompressed_length; i++)
				fprintf(stderr, "%c", message[i]);
#endif
				//write out the message
				fwrite(message, 1, decompressed_length, outfile);

				//and close the file
				fclose(outfile);
			}
			else
				fprintf(stderr, "Could not write to output\n");

			//close the input file and free the memory
			fclose(infile);
			free(message);

#ifdef DEBUG
			fprintf(stderr, "file length: %ld\n", file_len);
#endif
		}
		return 0;
	}
	else
	{
		fprintf(stderr, "The file is not accessible!\n");
		return 1;
	}
}

//Structure of compressed message:
/*
 * 2 Bytes: numbers of flags used
 * N Bits: wasted flags (less than 8)
 * X bits: flags used (X+N is at least 1 Byte)
 * Z Bytes: compressed message
 */

//function returning a decompressed message
unsigned char *decompress(unsigned char* flags, unsigned char* compressed_block, unsigned int block_length, int wasted_flags, int used_flags, unsigned int* decompressed_length)
{
	//debug info to verify the incoming data
#ifdef DEBUG
	fprintf(stderr, "Flags:\n");
	for (int i = 0; i < (used_flags + wasted_flags) / 8; i++)
	print_bin(flags[i], 8);
	fprintf(stderr, "Compressed block:\n");
	for (int i = 0; i < block_length; i++)
	{
		fprintf(stderr, "#%d:\t", i);
		print_bin(compressed_block[i], 8);
	}
#endif

	//allocate enough memory to start decompressing
	unsigned char *decompressed_message = calloc(BUFFER_SIZE, 1); //This is the maximum size

	//variables supporting the decompression
	int byte_position = 0, bit_position = 0;

	//cycle through all the Bytes used by the flags
	for (int i = 0; i < (used_flags + wasted_flags) / 8; i++)
	{
		//cycle through each flag
		for (int j = 0; j < 8; j++)
		{
#ifdef DEBUG
			fprintf(stderr, "FLAG:\t");
			fprintf(stderr, "%d", bit_state(flags[i], j));
			fprintf(stderr, "\n");
#endif

			//skip the flags that we don't need
			if (i == 0 && j < wasted_flags)
				continue;

			//check the flag, if it's 0 it means that what follows is not compressed
			if (bit_state(flags[i], j) == 0)
			{
				//add up the character to the decompressed message and increment the length
				decompressed_message[(*decompressed_length)++] = get_raw_uchar(compressed_block, &byte_position, &bit_position);

#ifdef DEBUG
				for (int i = 0; i < *decompressed_length; i++)
				{
					fprintf(stderr, "%c", decompressed_message[i]);
				}
				fprintf(stderr, "\n");
#endif
			}
			//check the flag, if it's 1 it means that what follows is a compressed block that will return at least 3 chars
			else if (bit_state(flags[i], j) == 1)
			{
				//support variable
				int length = 0;

				//fix the window from where it can start reading the dictionary (the lookahead buffer is encoded already)
				int lookbehind_buffer = 0;
				if (*decompressed_length > LOOKBEHINDBUFFER)
					lookbehind_buffer++;

				//temporary message <- block decoded
				unsigned char * msg = get_compressed_uchar(compressed_block, &byte_position, &bit_position, &decompressed_message[lookbehind_buffer], (*decompressed_length), &length);

				//copy the message into the decompressed message
				memcpy(&decompressed_message[(*decompressed_length)], msg, length);

				//free the memory
				free(msg);

#ifdef DEBUG
				fprintf(stderr, "decompressed length: %d \t new length to add: %d\n", *decompressed_length, length);
#endif

				//fix the length of the decompressed message
				(*decompressed_length) += length;

#ifdef DEBUG
				fprintf(stderr, "Copy test:\n");
				for (int i = 0; i < *decompressed_length; i++)
				{
					fprintf(stderr, "%c", decompressed_message[i]);
				}
				fprintf(stderr, "\n");
#endif
			}
		}
	}

#ifdef DEBUG
	fprintf(stderr, "############End Decompression############\n");
#endif

	//returns the message
	return decompressed_message;
}

//function that returns an uncompressed char
unsigned char get_raw_uchar(unsigned char * compressed_block, int * byte_position, int * bit_position)
{
	//variable needed to work out the char
	unsigned char byte = 0;

#ifdef DEBUG
	fprintf(stderr, "Byte position: %d\tBit position: %d\n", *byte_position, *bit_position);
#endif

	//the byte could be spread out in a single byte (if we start reading from bit 0), or two bytes if we have an offset
	byte |= compressed_block[(*byte_position)++] << (*bit_position);

	//if the position of the bit wasn't 0 then they byte is spread on 2 bytes
	if (*bit_position != 0)
		byte |= compressed_block[*byte_position] >> (8 - (*bit_position));

#ifdef DEBUG
	print_bin(byte, 8);
	fprintf(stderr, "%c\n", byte);
	fprintf(stderr, "____________________________________________\n");
#endif

	return byte;
}

//function that returns a decoded block of compressed chars
unsigned char * get_compressed_uchar(unsigned char * compressed_block, int *byte_position, int *bit_position, unsigned char * message_window, int window_position, int *msg_length)
{
	//we allocate 32 bits (we need 26 but we can only work with powers of 8)
	unsigned short index = 0, length = 0;

	//this is the amount of bits we need to parse for the index
	int bits_to_write = 13;

#ifdef DEBUG
	fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
	print_bin(compressed_block[*byte_position], 8);
	print_bin(((unsigned short) (compressed_block[(*byte_position)]) << (*bit_position)) << 5, 13);
#endif

	//parse the last remaining bits of the byte (these could range from 8 to 1)
	index |= ((unsigned short) (compressed_block[(*byte_position)++]) << (*bit_position)) << 5;

	//update the variables
	bits_to_write -= 8 - (*bit_position);
	//since we read all about this byte, we reset this one as well
	*bit_position = 0;

	//if there are less or equal to 8 bits to write, that means we don't need to read more than 1 byte!
	if (bits_to_write <= 8)
	{
#ifdef DEBUG
		fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
		print_bin(compressed_block[*byte_position], 8);
		print_bin((unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write), 13);
#endif
		index |= (unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write);

		//fix of the variables
		(*bit_position) += bits_to_write;
		if ((*bit_position) % 8 == 0 && (*bit_position) != 0)
		{
			(*bit_position) = 0;
			(*byte_position)++;
		}
	}
	//if more than 8 bits need to be written then we will have to parse 2 bytes
	else
	{
#ifdef DEBUG
		fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
		print_bin(compressed_block[*byte_position], 8);
		print_bin(((unsigned short) compressed_block[(*byte_position)]) << (bits_to_write % 8), 13);
#endif
		index |= ((unsigned short) compressed_block[(*byte_position)++]) << (bits_to_write % 8);

		//updating the variable (no need to touch the bit position as any place + 8 is still the same place)
		bits_to_write -= 8;

		//second byte
#ifdef DEBUG
		fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
		print_bin(compressed_block[*byte_position], 8);
		print_bin((unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write), 13);
#endif
		index |= (unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write);

		//fix of the variables
		(*bit_position) += bits_to_write;
		if ((*bit_position) % 8 == 0 && (*bit_position) != 0)
		{
			(*bit_position) = 0;
			(*byte_position)++;
		}
	}

#ifdef DEBUG
	fprintf(stderr, "The index is:\t");
	print_bin(index, 13);
#endif

	//we reset the variables and repeat for the length (same process!)
	//this is the amount of bits we need to parse for the index
	bits_to_write = 13;

#ifdef DEBUG
	fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
	print_bin(compressed_block[*byte_position], 8);
	print_bin(((unsigned short) (compressed_block[(*byte_position)]) << (*bit_position)) << 5, 13);
#endif

	//parse the last remaining bits of the byte (these could range from 8 to 1)
	length |= ((unsigned short) (compressed_block[(*byte_position)++]) << (*bit_position)) << 5;

	//update the variables
	bits_to_write -= 8 - (*bit_position);
	//since we read all about this byte, we reset this one as well
	*bit_position = 0;

	//if there are less or equal to 8 bits to write, that means we don't need to read more than 1 byte!
	if (bits_to_write <= 8)
	{
#ifdef DEBUG
		fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
		print_bin(compressed_block[*byte_position], 8);
		print_bin((unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write), 13);
#endif
		length |= (unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write);

		//fix of the variables
		(*bit_position) += bits_to_write;
		if ((*bit_position) % 8 == 0 && (*bit_position) != 0)
		{
			(*bit_position) = 0;
			(*byte_position)++;
		}
	}
	//if more than 8 bits need to be written then we will have to parse 2 bytes
	else
	{
#ifdef DEBUG
		fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
		print_bin(compressed_block[*byte_position], 8);
		print_bin(((unsigned short) compressed_block[(*byte_position)]) << (bits_to_write % 8), 13);
#endif
		length |= ((unsigned short) compressed_block[(*byte_position)++]) << (bits_to_write % 8);

		//updating the variable (no need to touch the bit position as any place + 8 is still the same place)
		bits_to_write -= 8;

		//second byte
#ifdef DEBUG
		fprintf(stderr, "Byte position: %d\tBit position: %d\tBits to write: %d\n", *byte_position, *bit_position, bits_to_write);
		print_bin(compressed_block[*byte_position], 8);
		print_bin((unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write), 13);
#endif
		length |= (unsigned short) (compressed_block[*byte_position]) >> (8 - bits_to_write);

		//fix of the variables
		(*bit_position) += bits_to_write;
		if ((*bit_position) % 8 == 0 && (*bit_position) != 0)
		{
			(*bit_position) = 0;
			(*byte_position)++;
		}
	}

#ifdef DEBUG
	fprintf(stderr, "The length is:\t");
	print_bin(length, 13);
#endif

	//this mask ensures that the 3 unused bits are at 0
	index &= 8191;
	length &= 8191;

#ifdef DEBUG
	fprintf(stderr, "allocating %d bytes\n", length);
#endif

	//allocate the exact memory needed to decode the message
	unsigned char * msg = calloc(length, sizeof msg);

	//parsing of the data
	for (int i = 0, j = 0; i < length; i++)
	{
		//if the dictionary exceeds the current position in the message then we need to read from the decoding-now-message itself
		if (i + index >= window_position)
		{
#ifdef DEBUG
			fprintf(stderr, "@%d: \t", i);
			fprintf(stderr, "%c\n", msg[j]);
#endif
			msg[i] = msg[j++];

		}
		else
		{
#ifdef DEBUG
			fprintf(stderr, "#@%d: \t", i);
			fprintf(stderr, "%c\n", message_window[index + i]);
#endif
			msg[i] = message_window[index + i];
		}
	}

	//we pass back the length of the message
	*msg_length = length;

#ifdef DEBUG
	for (int i = 0; i < length; i++)
	{
		fprintf(stderr, "%c", msg[i]);
	}
	fprintf(stderr, "\n");
	fprintf(stderr, "____________________________________________\n");
#endif

	//return the message
	//the programmer must remember to free the memory!!!
	return msg;
}

//this function does the math to calculate how long a single block is
int get_block_size(unsigned char * flags, int wasted_flags, int used_flags, int bytes_used_by_flags)
{
	int size = 0;

	//for each flags it sums up either a byte or 26 bits depending if the flag is 0 or 1
	for (int j = 0; j < bytes_used_by_flags; j++)
	{
		for (int i = 0; i < 8; i++)
		{
			if (!!(flags[j] & (1 << i)) == 0)
				size += 8;
			else
				size += 26;
		}
	}
	size -= wasted_flags * 26;
	return size;
}

