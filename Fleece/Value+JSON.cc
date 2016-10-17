//
//  Value+JSON.cc
//  Fleece
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Value.hh"
#include "Array.hh"
#include "Writer.hh"
#include "FleeceException.hh"
#include <ostream>
#include <ctime>
#include <iomanip>
#include <sstream>


namespace fleece {

    static void writeEscaped(Writer &out, slice str) {
        out << '"';
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
                        out << '\\';
                        --start; // ch will be written in next pass
                        break;
                    case '\n':
                        out << slice("\\n");
                        break;
                    case '\t':
                        out << slice("\\t");
                        break;
                    default: {
                        char buf[7];
                        sprintf(buf, "\\u%04u", (unsigned)ch);
                        out << slice(buf);
                        break;
                    }
                }
            }
        }
        out << std::string((char*)start, end-start);
        out << '"';
    }

    alloc_slice Value::toJSON() const {
        Writer writer;
        toJSON(writer);
        return writer.extractOutput();
    }


    void Value::toJSON(Writer &out) const {
        switch (type()) {
            case kNull:
                out << slice("null");
                return;
            case kBoolean:
                out << (asBool() ? slice("true") : slice("false"));
                return;
            case kNumber: {
                char str[32];
                if (isInteger()) {
                    int64_t i = asInt();
                    if (isUnsigned())
                        sprintf(str, "%llu", (uint64_t)i);
                    else
                        sprintf(str, "%lld", i);
                } else if (isDouble()) {
                    sprintf(str, "%.16g", asDouble());
                } else {
                    sprintf(str, "%.6g", asFloat());
                }
                out << slice(str);
                return;
            }
            case kString:
                return writeEscaped(out, asString());
            case kData:
                out << '"';
                out.writeBase64(asString());
                out << '"';
                return;
            case kArray: {
                out << '[';
                bool first = true;
                for (auto iter = asArray()->begin(); iter; ++iter) {
                    if (first)
                        first = false;
                    else
                        out << ',';
                    iter->toJSON(out);
                }
                out << ']';
                return;
            }
            case kDict: {
                out << '{';
                bool first = true;
                for (auto iter = asDict()->begin(); iter; ++iter) {
                    if (first)
                        first = false;
                    else
                        out << ',';
                    iter.key()->toJSON(out);
                    out << ':';
                    iter.value()->toJSON(out);
                }
                out << '}';
                return;
            }
            default:
                FleeceException::_throw(UnknownValue, "illegal typecode in Value; corrupt data?");
        }
    }

}
