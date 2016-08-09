// :mode=c++:
/*
decode.h - c++ wrapper for a base64 decoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_DECODE_H
#define BASE64_DECODE_H

#include <iostream>

namespace base64
{
	extern "C"
	{
		#include "cdecode.h"
	}

	class decoder
	{
    public:
		decoder()
		{
            base64_init_decodestate(&_state);
        }

		int decode(char value_in)
		{
			return base64_decode_value(value_in);
		}

		size_t decode(const void* code_in, const size_t length_in, void* plaintext_out)
		{
			return base64_decode_block(code_in, length_in, plaintext_out, &_state);
		}

		void decode(std::istream& istream_in, std::ostream& ostream_in, int buffersize_in = 1024)
		{
			base64_init_decodestate(&_state);
			//
			const int N = buffersize_in;
			char* code = new char[N];
			char* plaintext = new char[N];
            std::streamsize codelength;
			size_t plainlength;

			do
			{
				istream_in.read((char*)code, N);
				codelength = istream_in.gcount();
				plainlength = decode(code, codelength, plaintext);
				ostream_in.write((const char*)plaintext, plainlength);
			}
			while (istream_in.good() && codelength > 0);
			//
			base64_init_decodestate(&_state);

			delete [] code;
			delete [] plaintext;
		}

    private:
        base64_decodestate _state;
	};

} // namespace base64



#endif // BASE64_DECODE_H

