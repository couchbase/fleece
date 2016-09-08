/*
cencode.h - c header for a base64 encoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/

#ifndef BASE64_CENCODE_H
#define BASE64_CENCODE_H
#include <stdint.h>

typedef enum
{
	step_A, step_B, step_C
} base64_encodestep;

typedef struct
{
	base64_encodestep step;
	char result;
	int stepcount;
    int chars_per_line;
} base64_encodestate;

void base64_init_encodestate(base64_encodestate* state_in);

char base64_encode_value(uint8_t value_in);

size_t base64_encode_block(const uint8_t* plaintext_in,
                           size_t length_in,
                           void* code_out,
                           base64_encodestate* state_in);

size_t base64_encode_blockend(void* code_out,
                              base64_encodestate* state_in);

#endif /* BASE64_CENCODE_H */

