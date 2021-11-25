//
// Value+Dump.cc
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "FleeceImpl.hh"
#include "Doc.hh"
#include "Pointer.hh"
#include <ostream>
#include <iomanip>
#include <map>
#include <cmath>
#include <optional>
#include <sstream>
#include <stdio.h>

namespace fleece { namespace impl {
    using namespace std;
    using namespace internal;


    class ValueDumper {
    private:
        slice _data, _extern;
        std::ostream &_out;
        std::map<intptr_t, const Value*> _byAddress;

    public:
        explicit ValueDumper(const Value *value, slice data, std::ostream &out)
        :_data(data)
        ,_out(out)
        {
            if (auto scope = Scope::containing(value); scope)
                _extern = scope->externDestination();
            mapAddresses(value);
        }


        // Recursively adds addresses of `value` and its children to `_byAddress` map
        void mapAddresses(const Value *value) {
            if (auto offset = valueToOffset(value); offset) {
                _byAddress[*offset] = value;
                switch (value->type()) {
                    case kArray:
                        for (auto iter = value->asArray()->begin(); iter; ++iter) {
                            if (iter.rawValue()->isPointer())
                                mapAddresses(iter.value());
                        }
                        break;
                    case kDict:
                        for (Dict::iterator iter(value->asDict(), true); iter; ++iter) {
                            if (iter.rawKey()->isPointer())
                                mapAddresses(iter.key());
                            if (iter.rawValue()->isPointer())
                                mapAddresses(iter.value());
                        }
                        break;
                    default:
                        break;
                }
            }
        }


        // Dumps the collected values, ordered by address:
        void writeByAddress() {
            optional<intptr_t> pos;
            for (auto &i : _byAddress) {
                if (!pos) {
                    if (i.first < 0)
                        _out << "--begin extern data\n";
                } else {
                    if (*pos <= 0 && i.first >= 0)
                        _out << "--end extern data\n";
                    else if (i.first > *pos)
                        _out << "{skip " << std::hex << (i.first - *pos) << std::dec << "}\n";
                }
                pos = i.first + dump(i.second, false, 0);
                _out << '\n';
            }
        }


    private:

        optional<intptr_t> valueToOffset(const Value *value) const {
            if (_data.containsAddress(value))
                return value->_byte - (uint8_t*)_data.buf;
            else if (_extern.containsAddress(value))
                return value->_byte - (uint8_t*)_extern.end();
            else
                return nullopt;
        }


        // Writes the Value's byte offset and up to 4 bytes of its data.
        size_t dumpHex(const Value *value, bool wide) const {
            intptr_t pos = valueToOffset(value).value_or(intptr_t(value));
            char buf[64];
            sprintf(buf, "%c%04zx: %02x %02x",
                    (pos < 0 ? '-' : ' '), std::abs(pos), value->_byte[0], value->_byte[1]);
            _out << buf;
            auto size = value->dataSize();
            if (wide && size < kWide)
                size = kWide;
            if (size > 2) {
                sprintf(buf, " %02x %02x", value->_byte[2], value->_byte[3]);
                _out << buf;
                _out << (size > 4 ? "…" : " ");
            } else {
                _out << "       ";
            }
            _out << ": ";
            return size;
        }


        void writeDumpBrief(const Value *value, bool wide) const {
            if (value->tag() >= kPointerTagFirst)
                _out << "&";
            switch (value->tag()) {
                case kSpecialTag:
                case kShortIntTag:
                case kIntTag:
                case kFloatTag:
                case kStringTag: {
                    auto json = value->toJSON();
                    _out.write((const char*)json.buf, json.size);
                    break;
                }
                case kBinaryTag:
                    // TODO: show data
                    _out << "Binary[0x";
                    _out << value->asData().hexString();
                    _out << "]";
                    break;
                case kArrayTag: {
                    _out << "Array";
                    break;
                }
                case kDictTag: {
                    _out << "Dict";
                    break;
                }
                default: { // Pointer:
                    auto ptr = value->_asPointer();
                    bool external = ptr->isExternal();
                    bool legacy = false;
                    long long offset = - (long long)(wide ? ptr->offset<true>() : ptr->offset<false>());
                    if (external && !wide && offset >= 32768) {
                        // This is a narrow pointer that doesn't actually have the 'extern' bit
                        // but was created before that bit existed; that bit is actually the high
                        // bit of the offset.
                        external = false;
                        legacy = true;
                        offset -= 32768;
                    }
                    if (external && !_extern) {
                        _out << "Extern";
                    } else {
                        auto dstPtr = ptr->deref(wide);
                        writeDumpBrief(dstPtr, true);
//                        offset = valueToOffset(dstPtr).value_or(0ll);
                        offset = *valueToOffset(dstPtr);
                    }
                    char buf[32];
                    if (offset >= 0)
                        sprintf(buf, " @%04llx", offset);
                    else
                        sprintf(buf, " @-%04llx", -offset);
                    _out << buf;
                    if (legacy)
                        _out << " [legacy ptr]";
                    break;
                }
            }
        }


        // writes an ASCII dump of this value and its contained values (NOT following pointers).
        size_t dump(const Value *value, bool wide, int indent) const {
            auto size = dumpHex(value, wide);
            while (indent-- > 0)
                _out << "  ";
            writeDumpBrief(value, wide);
            int n = 0;
            switch (value->tag()) {
                case kArrayTag: {
                    _out << " [";
                    for (auto i = value->asArray()->begin(); i; ++i) {
                        if (n++ > 0) _out << ',';
                        _out << '\n';
                        size += dump(i.rawValue(), value->isWideArray(), 1);
                    }
                    _out << " ]";
                    break;
                }
                case kDictTag: {
                    _out << " {";
                    for (Dict::iterator i(value->asDict(), true); i; ++i) {
                        if (n++ > 0) _out << ',';
                        _out << '\n';
                        if (auto key = i.rawKey(); key->isInteger()) {
                            size += dumpHex(key, value->isWideArray());
                            size += (size & 1);
                            if (key->asInt() == -2048) {
                                // A -2048 key is a special case that means "parent Dict"
                                _out << "  <parent>";
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
                                    _out << "  \"" << std::string(keyStr) << '"';
                                else
                                    _out << "  SharedKeys[" << key->asInt() << "]";
                            }
                        } else {
                            size += dump(key, value->isWideArray(), 1);
                        }
                        _out << ":\n";
                        size += dump(i.rawValue(), value->isWideArray(), 2);
                    }
                    _out << " }";
                    break;
                }
                default:
                    break;
            }
            return size + (size & 1);
        }

    };



    void Value::dump(std::ostream &out) const {
        ValueDumper(this, slice(this, dataSize()), out).writeByAddress();
    }


    bool Value::dump(slice data, std::ostream &out) {
        auto root = fromData(data);
        if (!root)
            return false;
        // Walk the tree and collect every value with its address:
        ValueDumper d(root, data, out);

        // add the root pointer explicitly (`root` has been derefed already)
        auto actualRoot = (const Value*)offsetby(data.buf, data.size - internal::kNarrow);
        if (actualRoot != root)
            d.mapAddresses(actualRoot);
        d.writeByAddress();
        return true;
    }


    std::string Value::dump(slice data) {
        std::stringstream out;
        dump(data, out);
        return out.str();
    }

} }
