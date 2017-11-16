/*
 * lzss.h
 *
 *  Created on: Dec 11, 2014
 *      Author: spoofy
 */

#ifndef LZSS_H_
#define LZSS_H_

void print_hex(unsigned char *message, int size);
void print_bin(unsigned char byte, int size);
unsigned char * lzss(unsigned char original_message[], long message_length, long *compressed_length);
void joinstring(unsigned char *buffer, unsigned char *left, int length_l, unsigned char *right, int length_r);
void find_match(unsigned char message[], int cur_pos, int length, int buffer[], short* match_len, short int* index_found);
void get_buffer(int length, int position, int buffer[]);
void fix_array_pointer(int written_bits, int *array_pointer);
typedef struct pattern Pattern;
unsigned char *pattern_to_uchar(Pattern *p[], int count, int * bytes_used);
void set_cpattern(unsigned short index, unsigned short length, Pattern *pattern);
void set_char(unsigned char c, Pattern *pattern);

#endif /* LZSS_H_ */
