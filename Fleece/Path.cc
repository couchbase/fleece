//
//  Path.cc
//  Fleece
//
//  Created by Jens Alfke on 9/28/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "Path.hh"
#include "FleeceException.hh"

using namespace std;

namespace fleece {

    Path::Path(const string &specifier)
    :_specifier(specifier)
    {
        slice in(_specifier);
        if (in.hasPrefix(slice("$.")))
            in.moveStart(2);
        while (true) {
            auto dot = in.findByte('.');
            if (!dot)
                dot = in.end();
            _path.emplace_back(slice(in.buf, dot));
            in.setStart(dot);
            if (in.size == 0)
                break;
            in.moveStart(1);
        }
    }


    const Value* Path::eval(const Value *root) const {
        const Value *item = root;
        if (!item)
            return nullptr;
        for (auto &e : _path) {
            item = e.eval(item);
            if (!item)
                break;
        }
        return item;
    }


    Path::Element::Element(slice expr) {
        if (expr.size == 0)
            throw FleeceException(PathSyntaxError, "Empty path element");
        if (isdigit(expr[0]) || expr[0] == '-') {
            int64_t i = expr.readSignedDecimal();
            if (expr.size > 0 || i > INT32_MAX || i < INT32_MIN)
                throw FleeceException(PathSyntaxError, "Invalid numeric index");
            _index = (int32_t)i;
        } else {
            _key.reset(new Dict::key(expr));
        }
    }


    const Value* Path::Element::eval(const Value *item) const {
        if (_key) {
            auto d = item->asDict();
            if (!d)
                return nullptr;
            return d->get(*_key);
        } else {
            auto a = item->asArray();
            if (!a)
                return nullptr;
            int32_t i = _index;
            if (i < 0) {
                uint32_t count = a->count();
                if ((uint32_t)-i > count)
                    return nullptr;
                i += count;
            }
            return a->get((uint32_t)i);
        }
    }

}
