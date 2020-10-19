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

#include "FleeceImpl.hh"
#include "Pointer.hh"
#include <ostream>
#include <iomanip>
#include <map>
#include <cmath>
#include <sstream>
#include <stdio.h>

namespace fleece { namespace impl {
    using namespace internal;

    void Value::writeDumpBrief(std::ostream &out, const void *base, bool wide) const {
        if (tag() >= kPointerTagFirst)
            out << "&";
        switch (tag()) {
            case kSpecialTag:
            case kShortIntTag:
            case kIntTag:
            case kFloatTag:
            case kStringTag: {
                auto json = toJSON();
                out.write((const char*)json.buf, json.size);
                break;
            }
            case kBinaryTag:
                // TODO: show data
                out << "Binary[0x";
                out << asData().hexString();
                out << "]";
                break;
            case kArrayTag: {
                out << "Array";
                break;
            }
            case kDictTag: {
                out << "Dict";
                break;
            }
            default: { // Pointer:
                auto ptr = _asPointer();
                bool external = ptr->isExternal();
                bool legacy = false;
                long long offset = - (long long)(wide ? ptr->offset<true>() : ptr->offset<false>());
                if (base) {
                    offset = (((uint8_t*)_byte + offset) - (uint8_t*)base); // absolute
                    if (external && !wide && offset >= 32768) {
                        // This is a narrow pointer that doesn't actually have the 'extern' bit
                        // but was created before that bit existed; that bit is actually the high
                        // bit of the offset.
                        external = false;
                        legacy = true;
                        offset -= 32768;
                    }
                }
                if (external)
                    out << "Extern";
                else
                    ptr->deref(wide)->writeDumpBrief(out, base, true);
                char buf[32];
                if (offset >= 0)
                    sprintf(buf, " @%04llx", offset);
                else
                    sprintf(buf, " @-%04llx", -offset);
                out << buf;
                if (legacy)
                    out << " [legacy ptr]";
                break;
            }
        }
    }

    size_t Value::dumpHex(std::ostream &out, bool wide, const void *base) const {
        ssize_t pos = _byte - (uint8_t*)base;
        char buf[64];
        sprintf(buf, "%s%04zx: %02x %02x",
                (pos < 0 ? "-" : ""), std::abs(pos), _byte[0], _byte[1]);
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
        return size;
    }

    // writes an ASCII dump of this value and its contained values (NOT following pointers).
    size_t Value::dump(std::ostream &out, bool wide, int indent, const void *base) const {
        auto size = dumpHex(out, wide, base);
        while (indent-- > 0)
            out << "  ";
        writeDumpBrief(out, base, wide);
        int n = 0;
        switch (tag()) {
            case kArrayTag: {
                out << " [";
                for (auto i = asArray()->begin(); i; ++i) {
                    if (n++ > 0) out << ',';
                    out << '\n';
                    size += i.rawValue()->dump(out, isWideArray(), 1, base);
                }
                out << " ]";
                break;
            }
            case kDictTag: {
                out << " {";
                for (Dict::iterator i(asDict(), true); i; ++i) {
                    if (n++ > 0) out << ',';
                    out << '\n';
                    if (auto key = i.rawKey(); key->isInteger()) {
                        size += key->dumpHex(out, isWideArray(), base);
                        size += (size & 1);
                        if (key->asInt() == -2048) {
                            // A -2048 key is a special case that means "parent Dict"
                            out << "  <parent>";
                        } else {
#ifdef NDEBUG
                            slice keyStr = i.keyString();
#else
                            bool oldCheck = gDisableNecessarySharedKeysCheck;
                            gDisableNecessarySharedKeysCheck = true;
                            slice keyStr = i.keyString();
                            gDisableNecessarySharedKeysCheck = oldCheck;
#endif
                            if (keyStr)
                                out << "  \"" << std::string(keyStr) << '"';
                            else
                                out << "  SharedKeys[" << key->asInt() << "]";
                        }
                    } else {
                        size += key->dump(out, isWideArray(), 1, base);
                    }
                    out << ":\n";
                    size += i.rawValue()->dump(out, isWideArray(), 2, base);
                }
                out << " }";
                break;
            }
            default:
                break;
        }
        return size + (size & 1);
    }

    void Value::dump(std::ostream &out) const {
        mapByAddress byAddress;
        mapAddresses(byAddress);
        writeByAddress(byAddress, slice(this, dataSize()), out);
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
        writeByAddress(byAddress, data, out);
        return true;
    }

    void Value::writeByAddress(const mapByAddress &byAddress, slice data, std::ostream &out) {
        // Dump them ordered by address:
        size_t pos = (size_t)data.buf;
        for (auto &i : byAddress) {
            if (i.first > pos)
                out << "  {skip " << std::hex << (i.first - pos) << std::dec << "}\n";
            pos = i.first + i.second->dump(out, false, 0, data.buf);
            out << '\n';
        }
    }

    std::string Value::dump(slice data) {
        std::stringstream out;
        dump(data, out);
        return out.str();
    }

} }
