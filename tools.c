/*
 * tools.c
 *
 *  Created on: Dec 14, 2014
 *      Author: niko
 */
#include "tools.h"
#include <stdio.h>

//this is a set of functions that come in handy when programming

//function to prints bytes as hex simbols divided by |
void print_hex(unsigned char *message, int size)
{
	for (int i = 0; i < size; i++)
	{
		fprintf(stderr,"|%x", *(message + i));
	}
	fprintf(stderr,"|\n");
}

void print_bin(unsigned char byte, int size)
{
	fprintf(stderr,"|");
	for (int j = size-1; j >=0; j--)
		fprintf(stderr,"%d", !!(byte&(1<<j)));
	fprintf(stderr, "|\n");
}

void joinstring(unsigned char *buffer, unsigned char *left, int length_l, unsigned char *right, int length_r)
{
	for (int i = 0; i < length_l; i++)
	{
		buffer[i] = left[i];
	}
	for (int i = length_l; i < length_l + length_r; i++)
	{
		buffer[i] = right[i - length_l];
	}
}

void packshort(int val, unsigned char *dest)
{
	dest[0] = (val & 0x0000ff00) >> 8;
	dest[1] = (val & 0x000000ff);
}

//returns 1 if bit at position pos is 1, else 0
int bit_state(unsigned char byte, char position)
{
	return !!(byte & (1 << (7 - position)));
}
