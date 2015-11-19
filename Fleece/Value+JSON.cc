//
//  Value+JSON.cc
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Value.hh"
#include <ostream>
#include <ctime>
#include <iomanip>
#include <sstream>


namespace fleece {

    static void base64_encode(std::ostream &out,
                              unsigned char const* bytes_to_encode,
                              unsigned int in_len);

    static void writeEscaped(std::ostream &out, slice str) {
        out << "\"";
        auto start = (const uint8_t*)str.buf;
        auto end = (const uint8_t*)str.end();
        for (auto p = start; p < end; p++) {
            uint8_t ch = *p;
            if (ch == '"' || ch == '\\' || ch < 32 || ch == 127) {
                // Write characters from start up to p-1:
                out << std::string((char*)start, p-start);
                start = p + 1;
                switch (ch) {
                    case '"':
                    case '\\':
                        out << "\\";
                        --start; // ch will be written in next pass
                        break;
                    case '\n':
                        out << "\\n";
                        break;
                    case '\t':
                        out << "\\t";
                        break;
                    default: {
                        char buf[7];
                        sprintf(buf, "\\u%04u", (unsigned)ch);
                        out << buf;
                        break;
                    }
                }
            }
        }
        out << std::string((char*)start, end-start);
        out << "\"";
    }

    static void writeBase64(std::ostream &out, slice data) {
        out << '"';
        base64_encode(out, (const uint8_t*)data.buf, (unsigned)data.size);
        out << '"';
    }

    std::string value::toJSON() const {
        std::stringstream s;
        writeJSON(s);
        return s.str();
    }


    void value::writeJSON(std::ostream &out) const {
        switch (type()) {
            case kNull:
                out << "null";
                return;
            case kBoolean:
                out << (asBool() ? "true" : "false");
                return;
            case kNumber:
                if (isInteger()) {
                    int64_t i = asInt();
                    if (isUnsigned())
                        out << (uint64_t)i;
                    else
                        out << i;
                } else {
                    if (isDouble())
                        out << std::setprecision(16) << asDouble();
                    else
                        out << std::setprecision(6) << asFloat();
                }
                return;
            case kString:
                writeEscaped(out, asString());
                return;
            case kData:
                return writeBase64(out, asString());
            case kArray: {
                out << '[';
                const array* a = asArray();
                bool first = true;
                for (array::iterator iter(a); iter; ++iter) {
                    if (first)
                        first = false;
                    else
                        out << ',';
                    iter->writeJSON(out);
                }
                out << ']';
                return;
            }
            case kDict: {
                out << '{';
                const dict* d = asDict();
                bool first = true;
                for (dict::iterator iter(d); iter; ++iter) {
                    if (first)
                        first = false;
                    else
                        out << ',';
                    iter.key()->writeJSON(out);
                    out << ':';
                    iter.value()->writeJSON(out);
                }
                out << '}';
                return;
            }
            default:
                throw "illegal typecode";
        }

    }


#pragma mark - BASE64:


    /*
     base64.cpp and base64.h

     Copyright (C) 2004-2008 René Nyffenegger

     This source code is provided 'as-is', without any express or implied
     warranty. In no event will the author be held liable for any damages
     arising from the use of this software.

     Permission is granted to anyone to use this software for any purpose,
     including commercial applications, and to alter it and redistribute it
     freely, subject to the following restrictions:

     1. The origin of this source code must not be misrepresented; you must not
     claim that you wrote the original source code. If you use this source code
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original source code.

     3. This notice may not be removed or altered from any source distribution.

     René Nyffenegger rene.nyffenegger@adp-gmbh.ch

     */

    // Modified to write to an ostream instead of returning a string, and to fix compiler warnings.

    static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

    static void base64_encode(std::ostream &out,
                              unsigned char const* bytes_to_encode,
                              unsigned int in_len)
    {
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = (unsigned char)((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = (unsigned char)((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for(i = 0; (i <4) ; i++)
                    out << base64_chars[char_array_4[i]];
                i = 0;
            }
        }

        if (i)
        {
            for(j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = (unsigned char)((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = (unsigned char)((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                out << base64_chars[char_array_4[j]];

            while((i++ < 3))
                out << '=';

        }
    }

}
