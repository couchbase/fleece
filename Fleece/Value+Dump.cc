//
// Value+Dump.cc
//
// Copyright (c) 2015 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "Fleece.hh"
#include <ostream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdio.h>

namespace fleece {
    using namespace internal;

    void Value::writeDumpBrief(std::ostream &out, const void *base, bool wide) const {
        if (tag() >= kPointerTagFirst)
            out << "&";
        switch (tag()) {
            case kSpecialTag:
            case kShortIntTag:
            case kIntTag:
            case kFloatTag:
            case kStringTag:
                out << std::string(toJSON());
                break;
            case kBinaryTag:
                // TODO: show data
                out << "Binary[";
                out << std::string(toJSON());
                out << "]";
                break;
            case kArrayTag: {
                out << "Array[" << asArray()->count() << "]";
                break;
            }
            case kDictTag: {
                out << "Dict[" << asDict()->rawCount() << "]";
                break;
            }
            default: { // Pointer:
                deref(this, wide)->writeDumpBrief(out, base, true);
                auto offset = - (int64_t)(wide ? pointerValue<true>() : pointerValue<false>());
                char buf[32];
                if (base)
                    sprintf(buf, " (@%04llx)", (long long)(((uint8_t*)_byte + offset) - (uint8_t*)base)); // absolute
                else
                    sprintf(buf, " (@-%04llx)", (long long)-offset);
                out << buf;
                break;
            }
        }
    }

    // writes an ASCII dump of this value and its contained values (NOT following pointers).
    size_t Value::dump(std::ostream &out, bool wide, int indent, const void *base) const {
        size_t pos = _byte - (uint8_t*)base;
        char buf[64];
        sprintf(buf, "%04zx: %02x %02x", pos, _byte[0], _byte[1]);
        out << buf;
        auto size = dataSize();
        if (wide && size < kWide)
            size = kWide;
        if (size > 2) {
            sprintf(buf, " %02x %02x", _byte[2], _byte[3]);
            out << buf;
            out << (size > 4 ? "…" : " ");
        } else {
            out << "       ";
        }
        out << ": ";

        while (indent-- > 0)
            out << "  ";
        writeDumpBrief(out, base, (size > 2));
        switch (tag()) {
            case kArrayTag: {
                out << ":\n";
                for (auto i = asArray()->begin(); i; ++i) {
                    size += i.rawValue()->dump(out, isWideArray(), 1, base);
                }
                break;
            }
            case kDictTag: {
                out << ":\n";
                for (Dict::iterator i(asDict(), true); i; ++i) {
                    size += i.rawKey()  ->dump(out, isWideArray(), 1, base);
                    size += i.rawValue()->dump(out, isWideArray(), 2, base);
                }
                break;
            }
            default:
                out << "\n";
                break;
        }
        return size + (size & 1);
    }


    // Recursively adds addresses of v and its children to byAddress map
    void Value::mapAddresses(mapByAddress &byAddress) const {
        byAddress[(size_t)this] = this;
        switch (type()) {
            case kArray:
                for (auto iter = asArray()->begin(); iter; ++iter) {
                    if (iter.rawValue()->isPointer())
                        iter.value()->mapAddresses(byAddress);
                }
                break;
            case kDict:
                for (Dict::iterator iter(asDict(), true); iter; ++iter) {
                    if (iter.rawKey()->isPointer())
                        iter.key()->mapAddresses(byAddress);
                    if (iter.rawValue()->isPointer())
                        iter.value()->mapAddresses(byAddress);
                }
                break;
            default:
                break;
        }
    }

    bool Value::dump(slice data, std::ostream &out) {
        auto root = fromData(data);
        if (!root)
            return false;
        // Walk the tree and collect every value with its address:
        mapByAddress byAddress;
        root->mapAddresses(byAddress);

        // add the root pointer explicitly (`root` has been derefed already)
        auto actualRoot = (const Value*)offsetby(data.buf, data.size - internal::kNarrow);
        if (actualRoot != root)
            actualRoot->mapAddresses(byAddress);
        // Dump them ordered by address:
        size_t pos = (size_t)data.buf;
        for (auto &i : byAddress) {
            if (i.first != pos)
                out << "  {skip " << std::hex << (i.first - pos) << "}\n";
            pos = i.first + i.second->dump(out, false, 0, data.buf);
        }
        return true;
    }

    std::string Value::dump(slice data) {
        std::stringstream out;
        dump(data, out);
        return out.str();
    }

}

