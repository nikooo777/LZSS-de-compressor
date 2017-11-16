/*
 * lzss.c
 *
 *  Created on: Dec 11, 2014
 *      Author: spoofy
 */

//libs
#include <stdio.h>
#include "lzss.h"
#include <string.h>
#include <stdlib.h>
#include "tools.h"

//Do not exceed 4096 or risk of buffer overflows might occur when a match of length longer than 8192 or index = 8192
#define LOOKAHEADBUFFER 4096 	//Maximum defined lookahead buffer
#define LOOKBEHINDBUFFER 4096 	//Maximum defined lookbehind buffer
#define LA 0		//Index of the buffer array for LookAhead
#define LB 1		//Index of the buffer array for LookBehind
//#define DEBUG

//pattern struct used in lzss encoding
struct pattern
{
	char is_compressed :1; //flag
	short index :13;		//this stores the index of the found pattern
	short length :13;		//this is the length to copy starting from the index
	unsigned char c;		//when uncompressed, this is the Char

};

//Compressor that accepts a message and returns a compressed message
unsigned char * lzss(unsigned char original_message[], long message_length, long *compressed_length)
{
	unsigned char * compressed_message = malloc(2 * (LOOKAHEADBUFFER + LOOKBEHINDBUFFER)); //worst case scenario size
	unsigned char compression_flags[message_length];
	int flag_counter = 0;

	//temporary compressed message structure array
	Pattern **cm = malloc(2 * message_length * sizeof(Pattern *));
	for (int i = 0; i < 2 * message_length; i++)
	{
		cm[i] = malloc(sizeof(Pattern));
	}

	//buffer sizes for the search
	int search_buffer[2];

	//size when compressed
	*compressed_length = 0;

#ifdef DEBUG
	fprintf(stderr, "The size of the message to compress is %ld Bytes\n", message_length); //DBG
#endif

	// goes through all the length of the message
	for (int cur_msg_pos = 0; cur_msg_pos < message_length; cur_msg_pos++)
	{

#ifdef DEBUG
		fprintf(stderr, "------------------------------------------\n");
#endif

		//variables needed
		short match_length, index_of_match; //properties
		get_buffer(message_length, cur_msg_pos, search_buffer); //adjusts the buffer based on the actual position and max bounds
		find_match(original_message, cur_msg_pos, message_length, search_buffer, &match_length, &index_of_match); //finds the longest match starting from current position by looking back.
		if (index_of_match == -1) //if no substring matched, then
		{
			compression_flags[flag_counter++] = 0;  // no reference, therefore 0
			set_char(original_message[cur_msg_pos], cm[(*compressed_length)++]);

#ifdef DEBUG
			fprintf(stderr, "No match found for %c - Writing:\n", original_message[cur_msg_pos]);
			fprintf(stderr, "flag: %x\nChar: ", compression_flags[flag_counter - 1]);
			print_hex(&original_message[cur_msg_pos], 1);
#endif

		}
		else
		{
			compression_flags[flag_counter++] = 1; //a reference is found, therefore 1
			set_cpattern(index_of_match, match_length, cm[(*compressed_length)++]);

#ifdef DEBUG
			fprintf(stderr, "Found match for %d characters, writing:\n", match_length);
			fprintf(stderr, "flag: %x\n", compression_flags[flag_counter - 1]);
			fprintf(stderr, "index: %x\n", cm[(*compressed_length) - 1]->index);
			fprintf(stderr, "length: %x\n", cm[(*compressed_length) - 1]->length);
			fprintf(stderr, "Number of flags used: %d\n", flag_counter);
#endif

			cur_msg_pos += match_length - 1;
		}
	}

#ifdef DEBUG
	fprintf(stderr, "_________________________________________\n");
#endif

	//let's convert the structures in an array and copy it inside the right one
	int bytes_used = 0;
	unsigned char* merged_patterns = pattern_to_uchar(cm, *compressed_length, &bytes_used);
	*compressed_length = bytes_used;
	memcpy(compressed_message, merged_patterns, *compressed_length);
	//cm and the patterns are not needed anymore! We can free the memory
	free(merged_patterns);
	free(cm);

	//handling flags
	int modulo = (flag_counter % 8 != 0) ? 8 - (flag_counter % 8) : 0;
	int num_of_chars;
	num_of_chars = (modulo == 0) ? flag_counter / 8 : flag_counter / 8 + 1;
	if (flag_counter < 8)
	{
		modulo = 8 - flag_counter;
		num_of_chars = 1;
	}
	unsigned char new_flags[num_of_chars];
	int flags_index = 0;

#ifdef DEBUG
	fprintf(stderr, "num of arrays: %d - modulo: %d\n", num_of_chars, modulo);
#endif

	for (int i = 0; i < num_of_chars; i++)
	{
		new_flags[i] = 0;
		for (int j = 8; j > 0; j--)
		{
			//it might happen that we don't have a "power of 2" number of flags, therefore we just leave the first remaining flags at 1 which won't be harmful
			if (i == 0 && modulo > 0)
			{
				j -= modulo;
				while (modulo)
					new_flags[i] |= 1 << (8 - (modulo--));
			}
			//the remaining flags are stored (we know that the first one will be a 0 for sure (the dictionary is empty, no reference is found)
			new_flags[i] |= (compression_flags[flags_index++] & 1) << (j - 1);
		}

#ifdef DEBUG
		fprintf(stderr, "Flags are:\n");
		print_bin(new_flags[i], 8);
#endif
	}

	//Left Join flags in compressed_message
	// the 2 extra bytes are required for the "header" which adds as very FIRST thing the length of the message compressed
	unsigned char *temp = calloc((*compressed_length) + num_of_chars + 2, sizeof *temp);
	packshort(flag_counter, temp);

#ifdef DEBUG
	fprintf(stderr, "Header of the block (flags used): %d\n", flag_counter);
#endif


	//we start from temp[2] because the first 2 bytes are reserved for the length!
	joinstring(&temp[2], new_flags, num_of_chars, compressed_message, *compressed_length);
	*compressed_length += 2;	//header fix

#ifdef DEBUG
	fprintf(stderr, "Final Result is:\n");
	print_hex(temp, num_of_chars + *compressed_length);
	fprintf(stderr, "The size compressed: %ld Bytes\n", num_of_chars + *compressed_length);
	fprintf(stderr, "The size compressed: %ld bits\n", (num_of_chars + *compressed_length) * 8);
#endif

	memcpy(compressed_message, temp, num_of_chars + (*compressed_length) * sizeof(char));
	free(temp);
	*compressed_length += num_of_chars;

	return compressed_message;
}

//returns -1 if no match is found
//returns an index of the match and length of the match
void find_match(unsigned char message[], int cur_pos, int length, int buffer[], short* match_len, short int* index_found)
{
	//default values
	*match_len = 0;
	*index_found = -1;

	//fix the lookahead buffer to never cross the end of the stream
	get_buffer(length, cur_pos, buffer);

	//loops from left to right in order to find a matching result with the current index
	for (int sx = cur_pos - buffer[LB]; sx < cur_pos; sx++)
	{
		//we store the offset in a temporary variable to understand when we find a match and if it's any longer than the previous ones found
		int temp_offset = 0;

		//We loop until a match is no longer possible
		while (message[sx + temp_offset] == message[cur_pos + temp_offset])
		{
			temp_offset++; // increase the temp offset which is a candidate for the final offset
		}
		//Only the greatest offset may survive!
		if (temp_offset > *match_len)
		{
			*match_len = temp_offset;
			*index_found = sx;
		}
	}
	//Can't compress so few!
	if (*match_len < 3)
	{
		*match_len = 0;
		*index_found = -1;
	}
}

//fixes both the lookahead and lookbehind buffer
void get_buffer(int length, int position, int buffer[])
{
	buffer[LA] = LOOKAHEADBUFFER;
	buffer[LB] = LOOKBEHINDBUFFER;

	//lookahead fix
	while (length - position < buffer[LA])
		buffer[LA]--;
	//lookbehind fix
	while (position < buffer[LB])
		buffer[LB]--;
}

//sets the pattern and the flag in the structure
/**
 * index: index of the pattern
 * length: length of the pattern
 * pattern: pointer to the pattern struct
 */
void set_cpattern(unsigned short index, unsigned short length, Pattern *pattern)
{
	pattern->index = index;
	pattern->length = length;
	pattern->is_compressed = 1;
}

//when we find a char that we don't need to compress we write it and flag it
/**
 * c: character uncompressed
 * pattern: pointer to the pattern struct
 */
void set_char(unsigned char c, Pattern *pattern)
{
	pattern->c = c;
	pattern->is_compressed = 0;
}

//given a set of patterns returns an array of unsigned char filled with the bits of the structure IN ORDER
unsigned char *pattern_to_uchar(Pattern *p[], int count, int * bytes_used)
{

	int compressed = 0, plain = 0;

#ifdef DEBUG
	fprintf(stderr, "Count of patterns is: %d\n", count);
#endif
	//just calculates how many compressed/uncompressed patterns we have
	for (int i = 0; i < count; i++)
	{
		if (p[i]->is_compressed)
			compressed++;
		else
			plain++;
	}

	//temp stores the number of bits used in the pattern (13 for length and 13 for the index or 8 for a simple char)
	int tot_bits = compressed * (13 + 13) + plain * 8;

	//since we can only write 8 bits per array, we calculate how many arrays are needed to store the bits
	int nr_of_arrays = (tot_bits % 8 == 0) ? tot_bits / 8 : (tot_bits / 8) + 1;

	//we allocate the needed memory for the array of unsigned chars that will contain, concatenated, all the bits
	unsigned char* uc = (unsigned char*) calloc(nr_of_arrays, sizeof(unsigned char));

	//the array pointer is used to keep track of which array we're writing into (incremental only)
	int array_pointer = 0;

	//written bits is self-explanatory however it's used in modulo 8 to always position the bits in their right place
	int written_bits = 0;

	//update the count so that we can use this information in the lzss algorithm
	*bytes_used = nr_of_arrays;

#ifdef DEBUG
	fprintf(stderr, "Tot bits: %d\n", tot_bits);
	fprintf(stderr, "nr of arrays: %d\n", nr_of_arrays);
#endif

	//we are going to cycle through all the patterns and for each pattern we're going to copy bit per bit the content into our whole array accessing it
	//linearly
	for (int i = 0; i < count; i++)
	{
		//we have 2 possible situations: compressed - uncompressed
		if (p[i]->is_compressed)
		{
			//since both the index and the length are stored in 13 bits, we iterate them and manually copy them out one by one into the
			//uc array (the one we will be returning)
			//so for the index...
			for (int j = 0; j < 13; j++)
			{

				//ARRAY		!!<- to either get 0 or 1	shift 0-12			shift L-R (7-0)
				uc[array_pointer] |= (!!(p[i]->index & (1 << (12 - j)))) << (7 - (written_bits % 8));
				//each written bit must be accounted
				written_bits++;
				//and for each written bit, the array pointer might change, therefore we pass it ot our function
				fix_array_pointer(written_bits, &array_pointer);
			}
			//and this is for the length
			for (int j = 0; j < 13; j++)
			{
				uc[array_pointer] |= (!!(p[i]->length & (1 << (12 - j)))) << (7 - (written_bits % 8));
				written_bits++;
				fix_array_pointer(written_bits, &array_pointer);
			}
		}
		//This is for the part of the structure that is not compressed
		else
		{
			for (int j = 7; j >= 0; j--)
			{
				uc[array_pointer] |= (!!(p[i]->c & (1 << j))) << (7 - (written_bits % 8));
				written_bits++;
				fix_array_pointer(written_bits, &array_pointer);
			}
		}
#ifdef DEBUG
		fprintf(stderr, "Written bits: %d\n", written_bits);
#endif
	}

#ifdef DEBUG
	for (int i = 0; i < nr_of_arrays; i++)
	print_bin(uc[i], 8);
#endif
	return uc;
}

//this handles the array pointer:
//given the number of written bits and the current value of the array pointer it calculates whether it's time to increase the value or not
void fix_array_pointer(int written_bits, int *array_pointer)
{
	if (written_bits % 8 == 0)
		(*array_pointer)++;
}
