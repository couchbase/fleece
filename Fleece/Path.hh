//
//  Path.hh
//  Fleece
//
//  Created by Jens Alfke on 9/28/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "Array.hh"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace fleece {
    class SharedKeys;

    /** Describes a location in a Fleece object tree, as a path from the root that follows
        dictionary properties and array elements.
        Similar to a JSONPointer or an Objective-C KeyPath, but simpler (so far.)
        It looks like "foo.bar[2][-3].baz" -- that is, properties prefixed with a ".", and array
        indexes in brackets. (Negative indexes count from the end of the array.)
        A leading JSONPath-like "$." is allowed but ignored.
        This class is pretty experimental ... syntax may change without warning! */
    class Path {
    public:
        class Element;

        Path(const std::string &specifier, SharedKeys* =nullptr);

        const std::string& specifier() const        {return _specifier;}
        const std::vector<Element>& path() const    {return _path;}

        const Value* eval(const Value *root) const noexcept;

        /** One-shot evaluation; faster if you're only doing it once */
        static const Value* eval(slice specifier, SharedKeys*, const Value *root);

        class Element {
        public:
            Element(slice property, SharedKeys *sk) :_key(new Dict::key(property, sk, false)) { }
            Element(int32_t arrayIndex)             :_index(arrayIndex) { }
            const Value* eval(const Value*) const noexcept;
            bool isKey() const                      {return _key != nullptr;}
            Dict::key& key() const                  {return *_key;}
            int32_t index() const                   {return _index;}

            static const Value* eval(char token, slice property, int32_t index, SharedKeys*,
                                     const Value *item) noexcept;
        private:
            static const Value* getFromArray(const Value*, int32_t index) noexcept;

            std::unique_ptr<Dict::key> _key {nullptr};
            int32_t _index {0};
        };

    private:
        static void forEachComponent(slice in, std::function<bool(char,slice,int32_t)> callback);

        const std::string _specifier;
        std::vector<Element> _path;
    };

}
