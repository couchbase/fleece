// :mode=c++:
/*
encode.h - c++ wrapper for a base64 encoding algorithm

This is part of the libb64 project, and has been placed in the public domain.
For details, see http://sourceforge.net/projects/libb64
*/
#ifndef BASE64_ENCODE_H
#define BASE64_ENCODE_H

#include <iostream>

namespace base64
{
	extern "C" 
	{
		#include "cencode.h"
	}

	class encoder
	{
    public:
		encoder()
		{
            base64_init_encodestate(&_state);
        }

        void set_chars_per_line(int chars_per_line)
        {
            _state.chars_per_line = chars_per_line;
        }

		int encode(char value_in)
		{
			return base64_encode_value(value_in);
		}

		size_t encode(const void* code_in, const size_t length_in, void* plaintext_out)
		{
			return base64_encode_block((const uint8_t*)code_in, length_in, plaintext_out, &_state);
		}

		size_t encode_end(char* plaintext_out)
		{
			return base64_encode_blockend(plaintext_out, &_state);
		}

		void encode(std::istream& istream_in, std::ostream& ostream_in, size_t buffersize = 1024)
		{
			const size_t N = buffersize;
			char* plaintext = new char[N];
			char* code = new char[2*N];
            std::streamsize plainlength;
			size_t codelength;

			do
			{
				istream_in.read(plaintext, N);
				plainlength = istream_in.gcount();
				//
				codelength = encode(plaintext, plainlength, code);
				ostream_in.write(code, codelength);
			}
			while (istream_in.good() && plainlength > 0);

			codelength = encode_end(code);
			ostream_in.write(code, codelength);
			//
			base64_init_encodestate(&_state);

			delete [] code;
			delete [] plaintext;
		}

    private:
        base64_encodestate _state;
	};

} // namespace base64

#endif // BASE64_ENCODE_H

