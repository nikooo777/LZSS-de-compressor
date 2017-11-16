/*
 * decompressor.h
 *
 *  Created on: Dec 18, 2014
 *      Author: spoofy
 */

#ifndef DECOMPRESSOR_H_
#define DECOMPRESSOR_H_

unsigned char *decompress(unsigned char* flags, unsigned char* compressed_block, unsigned int block_length, int wasted_flags, int used_flags, unsigned int* decompressed_length);
int bit_state(unsigned char byte, char position);
int decode(char * input_file);
int get_block_size(unsigned char * flags,int wasted_flags,int used_flags, int bytes_used_by_flags);
unsigned char * get_compressed_uchar(unsigned char * compressed_block, int *byte_position, int *bit_position, unsigned char * message_window, int window_position, int *msg_length);
unsigned char get_raw_uchar(unsigned char * compressed_block, int * byte_position, int * bit_position);
#endif
/* DECOMPRESSOR_H_ */
