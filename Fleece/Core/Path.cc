//
// Path.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
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

#include "Path.hh"
#include "SharedKeys.hh"
#include "FleeceException.hh"
#include "PlatformCompat.hh"
#include <iostream>

using namespace std;

namespace fleece {

    // Parses a path expression, calling the callback for each property or array index.
    void Path::forEachComponent(slice in, eachComponentCallback callback) {
        throwIf(in.size == 0, PathSyntaxError, "Empty path");
        throwIf(in[in.size-1] == '\\', PathSyntaxError, "'\\' at end of string");

        uint8_t token = in.peekByte();
        if (token == '$') {
            // Starts with "$." or "$["
            in.moveStart(1);
            if (in.size == 0)
                return;                 // Just "$" means the root
            token = in.readByte();
            throwIf(token != '.' && token != '[', PathSyntaxError, "Invalid path delimiter after $");
        } else if (token == '[' || token == '.') {
            // Starts with "[" or "."
            in.moveStart(1);
        } else if (token == '\\') {
            // First character of path is escaped (probably a '$' or '.' or '[')
            if (in[1] == '$')
                in.moveStart(1);
            token = '.';
        } else {
            // Else starts with a property name
            token = '.';
        }

        if (in.size == 0 && token == '.')
            return;                     // "." or "" mean the root

        while (true) {
            // Read parameter (property name or array index):
            const uint8_t* next;
            slice param;
            alloc_slice unescaped;
            int32_t index = 0;

            if (token == '.') {
                // Find end of property name:
                next = in.findAnyByteOf(".[\\"_sl);
                if (next == nullptr) {
                    param = in;
                    next = (const uint8_t*)in.end();
                } else if (*next != '\\') {
                    param = slice(in.buf, next);
                } else {
                    // Name contains escapes -- need to unescape it:
                    unescaped.reset(in.size);
                    auto dst = (uint8_t*)unescaped.buf;
                    for (next = (const uint8_t*)in.buf; next < in.end(); ++next) {
                        uint8_t c = *next;
                        if (c == '\\')
                            c = *++next;
                        *dst++ = c;
                    }
                    param = slice(unescaped.buf, dst);
                }

            } else if (token == '[') {
                // Find end of array index:
                next = in.findByteOrEnd(']');
                if (!next)
                    FleeceException::_throw(PathSyntaxError, "Missing ']'");
                param = slice(in.buf, next++);
                // Parse array index:
                slice n = param;
                int64_t i = n.readSignedDecimal();
                throwIf(n.size > 0 || i > INT32_MAX || i < INT32_MIN,
                        PathSyntaxError, "Invalid array index");
                index = (int32_t)i;
            } else {
                FleeceException::_throw(PathSyntaxError, "Invalid path component");
            }

            // Invoke the callback:
            throwIf(param.size == 0, PathSyntaxError, "Empty property or index");
            if (_usuallyFalse(!callback(token, param, index)))
                return;

            // Did we read the whole expression?
            if (next >= in.end())
                break;              // LOOP EXIT

            // Read the next token and go round again…
            token = *next;
            in.setStart(next+1);
        }
    }


    /*static*/ const Value* Path::evalJSONPointer(slice specifier, SharedKeys *sk,
                                                  const Value *root)
    {
        auto current = root;
        throwIf(specifier.readByte() != '/', PathSyntaxError, "JSONPointer does not start with '/'");
        while (specifier.size > 0) {
            if (!current)
                return nullptr;

            auto slash = specifier.findByteOrEnd('/');
            slice param(specifier.buf, slash);

            switch(current->type()) {
                case kArray: {
                    auto i = param.readDecimal();
                    if (_usuallyFalse(param.size > 0 || i > INT32_MAX))
                        FleeceException::_throw(PathSyntaxError, "Invalid array index in JSONPointer");
                    current = ((const Array*)current)->get((uint32_t)i);
                    break;
                }
                case kDict: {
                    string key = param.asString();
                    current = ((const Dict*)current)->get(key, sk);
                    break;
                }
                default:
                    current = nullptr;
                    break;
            }

            if (slash == specifier.end())
                break;
            specifier.setStart(slash+1);
        }
        return current;
    }

    /*static*/ const Value* Path::eval(slice specifier, SharedKeys *sk, const Value *root) {
        const Value *item = root;
        if (_usuallyFalse(!item))
            return nullptr;
        forEachComponent(specifier, [&](char token, slice component, int32_t index) {
            item = Element::eval(token, component, index, sk, item);
            return (item != nullptr);
        });
        return item;
    }


    Path::Path(const string &specifier, SharedKeys *sk)
    :_specifier(specifier)
    {
        forEachComponent(slice(_specifier), [&](char token, slice component, int32_t index) {
            if (token == '.')
                _path.emplace_back(component, sk);
            else
                _path.emplace_back(index);
            return true;
        });
    }


    const Value* Path::eval(const Value *root) const noexcept {
        const Value *item = root;
        if (_usuallyFalse(!item))
            return nullptr;
        for (auto &e : _path) {
            item = e.eval(item);
            if (!item)
                break;
        }
        return item;
    }


    Path::Element::Element(slice property, SharedKeys *sk)
    :_keyBuf(property)
    ,_key(new Dict::key(_keyBuf, sk, false))
    { }


    const Value* Path::Element::eval(const Value *item) const noexcept {
        if (_key) {
            auto d = item->asDict();
            if (_usuallyFalse(!d))
                return nullptr;
            return d->get(*_key);
        } else {
            return getFromArray(item, _index);
        }
    }

    /*static*/ const Value* Path::Element::eval(char token, slice comp, int32_t index,
                                                SharedKeys *sk, const Value *item) noexcept {
        if (token == '.') {
            auto d = item->asDict();
            if (_usuallyFalse(!d))
                return nullptr;
            return d->get(comp, sk);
        } else {
            return getFromArray(item, index);
        }
    }


    const Value* Path::Element::getFromArray(const Value* item, int32_t index) noexcept {
        auto a = item->asArray();
        if (_usuallyFalse(!a))
            return nullptr;
        if (index < 0) {
            uint32_t count = a->count();
            if (_usuallyFalse((uint32_t)-index > count))
                return nullptr;
            index += count;
        }
        return a->get((uint32_t)index);
    }


    void Path::writeProperty(std::ostream &out, slice key, bool first) {
        if (first) {
            if (key.hasPrefix('$'))
                out << '\\';
        } else {
            out << '.';
        }
        const uint8_t *toQuote;
        while (nullptr != (toQuote = key.findAnyByteOf(".[\\"_sl))) {
            out.write((const char *)key.buf, toQuote - (const uint8_t*)key.buf);
            out << '\\' << *toQuote;
            key.setStart(toQuote + 1);
        }
        out.write((const char *)key.buf, key.size);
    }

    void Path::writeIndex(std::ostream &out, int index) {
        out << '[' << index << ']';
    }


}
