/*
 * tools.h
 *
 *  Created on: Dec 14, 2014
 *      Author: niko
 */

#ifndef TOOLS_H_
#define TOOLS_H_
#include <stdio.h>
void print_hex(unsigned char *message, int size);
void print_bin(unsigned char byte, int size);
void joinstring(unsigned char *buffer, unsigned char *left, int length_l, unsigned char *right, int length_r);
void packshort(int val, unsigned char *dest);
int bit_state(unsigned char byte, char position);
#endif /* TOOLS_H_ */
