//
//  Path.cc
//  Fleece
//
//  Created by Jens Alfke on 9/28/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "Path.hh"
#include "SharedKeys.hh"
#include "FleeceException.hh"
#include "PlatformCompat.hh"

using namespace std;

namespace fleece {

    // Parses a path expression, calling the callback for each property or array index.
    void Path::forEachComponent(slice in, function<bool(char, slice, int32_t)> callback) {
        throwIf(in.size == 0, PathSyntaxError, "Empty path");
        uint8_t token = in.peekByte();
        if (token == '$') {
            // Starts with "$." or "$["
            in.moveStart(1);
            if (in.size == 0)
                return;                 // Just "$" means the root
            token = in.readByte();
            if (token != '.' && token != '[')
                FleeceException::_throw(PathSyntaxError, "Invalid path delimiter after $");
        } else if (token == '[' || token == '.') {
            // Starts with "[" or "."
            in.moveStart(1);
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
            int32_t index = 0;

            if (token == '.') {
                // Find end of property name:
                next = in.findAnyByteOf(slice(".[", 2));
                if (!next)
                    next = (const uint8_t*)in.end();
                param = slice(in.buf, next);
            } else if (token == '[') {
                // Find end of array index:
                next = in.findByteOrEnd(']');
                if (!next)
                    FleeceException::_throw(PathSyntaxError, "Missing ']'");
                param = slice(in.buf, next++);
                // Parse array index:
                slice n = param;
                int64_t i = n.readSignedDecimal();
                if (_usuallyFalse(n.size > 0 || i > INT32_MAX || i < INT32_MIN))
                    FleeceException::_throw(PathSyntaxError, "Invalid array index");
                index = (int32_t)i;
            } else {
                FleeceException::_throw(PathSyntaxError, "Invalid path component");
            }

            // Invoke the callback:
            if (param.size == 0)
                FleeceException::_throw(PathSyntaxError, "Empty property or index");
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

}
