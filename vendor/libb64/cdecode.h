/*
cdecode.h - c header for a base64 decoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/

#ifndef BASE64_CDECODE_H
#define BASE64_CDECODE_H
#include <stdint.h>
#include <stdlib.h>

typedef enum
{
	step_a, step_b, step_c, step_d
} base64_decodestep;

typedef struct
{
	base64_decodestep step;
	uint8_t plainchar;
} base64_decodestate;

void base64_init_decodestate(base64_decodestate* state_in);

char base64_decode_value(uint8_t value_in);

size_t base64_decode_block(const uint8_t* code_in,
                           const size_t length_in,
                           void* plaintext_out,
                           base64_decodestate* state_in);

#endif /* BASE64_CDECODE_H */

