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
#include "SharedKeys.hh"
#include "FleeceException.hh"
#include <ostream>
#include <ctime>
#include <iomanip>
#include <sstream>


namespace fleece {

    static bool canBeUnquotedJSON5Key(slice key) {
        if (key.size == 0 || isdigit(key[0]))
            return false;
        for (unsigned i = 0; i < key.size; i++) {
            if (!isalnum(key[i]) && key[i] != '_' && key[i] != '$')
                return false;
        }
        return true;
    }

    template <int VER>
    alloc_slice Value::toJSON(const SharedKeys *sk) const {
        Writer writer;
        toJSON<VER>(writer, sk);
        return writer.extractOutput();
    }


    template <int VER>
    void Value::toJSON(Writer &out, const SharedKeys *sk) const {
        switch (type()) {
            case kNull:
                out << slice("null");
                return;
            case kBoolean:
                out.writeJSONBool(asBool());
                return;
            case kNumber:
                if (isInteger())
                    out.writeJSONInt(asInt(), isUnsigned());
                else if (isDouble())
                    out.writeJSONDouble(asDouble());
                else
                    out.writeJSONFloat(asFloat());
                return;
            case kString:
                out.writeJSONString(asString());
                return;
            case kData:
                out << '"';
                out.writeBase64(asData());
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
                    iter->toJSON(out, sk);
                }
                out << ']';
                return;
            }
            case kDict: {
                out << '{';
                bool first = true;
                for (auto iter = asDict()->begin(sk); iter; ++iter) {
                    if (first)
                        first = false;
                    else
                        out << ',';
                    slice keyStr = iter.keyString();
                    if (keyStr) {
                        if (VER == 5 && canBeUnquotedJSON5Key(keyStr))
                            out.write((char*)keyStr.buf, keyStr.size);
                        else
                            out.writeJSONString(keyStr);
                    } else {
                        iter.key()->toJSON(out, sk);    // non-string keys are possible...
                    }
                    out << ':';
                    iter.value()->toJSON(out, sk);
                }
                out << '}';
                return;
            }
            default:
                FleeceException::_throw(UnknownValue, "illegal typecode in Value; corrupt data?");
        }
    }


    // Explicitly instantiate both needed versions of the templates:
    template void Value::toJSON<1>(Writer &out, const SharedKeys *sk) const;
    template void Value::toJSON<5>(Writer &out, const SharedKeys *sk) const;

    template alloc_slice Value::toJSON<1>(const SharedKeys *sk) const;
    template alloc_slice Value::toJSON<5>(const SharedKeys *sk) const;
}
