//
//  Value+Dump.cpp
//  Fleece
//
//  Created by Jens Alfke on 11/27/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#include "Value.hh"
#include <ostream>
#include <iomanip>
#include <map>
#include <sstream>

namespace fleece {
    using namespace internal;

    void value::writeDumpBrief(std::ostream &out, const void *base, bool wide) const {
        if (tag() >= kPointerTagFirst)
            out << "&";
        switch (tag()) {
            case kSpecialTag:
            case kShortIntTag:
            case kIntTag:
            case kFloatTag:
            case kStringTag:
                toJSON(out);
                break;
            case kBinaryTag:
                // TODO: show data
                out << "Binary[";
                toJSON(out);
                out << "]";
                break;
            case kArrayTag: {
                out << "Array[" << arrayCount() << "]";
                break;
            }
            case kDictTag: {
                out << "Dict[" << arrayCount() << "]";
                break;
            }
            default: { // Pointer:
                deref(this, wide)->writeDumpBrief(out, base, true);
                auto offset = - (ptrdiff_t)(wide ? pointerValue<true>() : pointerValue<false>());
                char buf[32];
                if (base)
                    sprintf(buf, " (@%04lx)", offset + _byte - (uint8_t*)base); // absolute
                else
                    sprintf(buf, " (@-%04lx)", offset);
                out << buf;
                break;
            }
        }
    }

    // writes an ASCII dump of this value and its contained values (NOT following pointers).
    void value::dump(std::ostream &out, bool wide, int indent, const void *base) const {
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
                    i.rawValue()->dump(out, isWideArray(), 1, base);
                }
                break;
            }
            case kDictTag: {
                out << ":\n";
                for (auto i = asDict()->begin(); i; ++i) {
                    i.rawKey()  ->dump(out, isWideArray(), 1, base);
                    i.rawValue()->dump(out, isWideArray(), 2, base);
                }
                break;
            }
            default:
                out << "\n";
                break;
        }
    }


    // Recursively adds addresses of v and its children to byAddress map
    void value::mapAddresses(mapByAddress &byAddress) const {
        byAddress[(size_t)this] = this;
        switch (type()) {
            case kArray:
                for (auto iter = asArray()->begin(); iter; ++iter) {
                    if (iter.rawValue()->isPointer())
                        iter.value()->mapAddresses(byAddress);
                }
                break;
            case kDict:
                for (auto iter = asDict()->begin(); iter; ++iter) {
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

    bool value::dump(slice data, std::ostream &out) {
        auto root = fromData(data);
        if (!root)
            return false;
        mapByAddress byAddress;
        root->mapAddresses(byAddress);
        rootPointer(data)->mapAddresses(byAddress); // add the root pointer explicitly
        for (auto i = byAddress.begin(); i != byAddress.end(); ++i) {
            i->second->dump(out, false, 0, data.buf);
        }
        return true;
    }

    std::string value::dump(slice data) {
        std::stringstream out;
        dump(data, out);
        return out.str();
    }

}
