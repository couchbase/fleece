/*
cdecoder.c - c source to a base64 decoding algorithm implementation

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/

#include "cdecode.h"

char base64_decode_value(uint8_t value_in)
{
	static const int8_t decoding[] = {62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-2,-1,-1,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,-1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51};
	if (value_in < 43 || value_in >= 43 + sizeof(decoding)) return -1;
	return decoding[value_in - 43];
}

void base64_init_decodestate(base64_decodestate* state_in)
{
	state_in->step = step_a;
	state_in->plainchar = 0;
}

size_t base64_decode_block(const uint8_t* code_in, const size_t length_in, void* plaintext_out,
						   base64_decodestate* state_in)
{
	const uint8_t* codechar = code_in;
	uint8_t* plainchar = plaintext_out;
	int8_t fragment;
	
	*plainchar = state_in->plainchar;
	
	switch (state_in->step)
	{
		while (1)
		{
	case step_a:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_a;
					// CBL-2264: Point back to zero instead of pointing beypond the allocated buffer
					state_in->plainchar = 0;
					return plainchar - (uint8_t*)plaintext_out;
				}
				fragment = base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar    = (uint8_t)((fragment & 0x03f) << 2);
	case step_b:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_b;
					state_in->plainchar = *plainchar;
					return plainchar - (uint8_t*)plaintext_out;
				}
				fragment = base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (uint8_t)((fragment & 0x030) >> 4);
			*plainchar    = (uint8_t)((fragment & 0x00f) << 4);
	case step_c:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_c;
					state_in->plainchar = *plainchar;
					return plainchar - (uint8_t*)plaintext_out;
				}
				fragment = base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++ |= (uint8_t)((fragment & 0x03c) >> 2);
			*plainchar    = (uint8_t)((fragment & 0x003) << 6);
	case step_d:
			do {
				if (codechar == code_in+length_in)
				{
					state_in->step = step_d;
					state_in->plainchar = *plainchar;
					return plainchar - (uint8_t*)plaintext_out;
				}
				fragment = base64_decode_value(*codechar++);
			} while (fragment < 0);
			*plainchar++   |= (fragment & 0x03f);
		}
	}
	/* control should not reach here */
	return plainchar - (uint8_t*)plaintext_out;
}

