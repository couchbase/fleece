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

namespace fleece {

    /** Describes a location in a Fleece object tree, as a path from the root that follows
        dictionary properties and array elements.
        Similar to a JSONPointer or an Objective-C KeyPath, but simpler (so far.)
        Syntax is a series of "."-delimited elements. If an element is an integer it's interpreted
        as an array index; negative numbers count backwards from the end of the array. Otherwise
        it's interpreted as a dictionary key.
        A leading JSONPath-like "$." is allowed but ignored.
        This class is pretty experimental ... syntax may change without warning! */
    class Path {
    public:
        class Element;

        Path(const std::string &specifier);

        const std::string& specifier() const        {return _specifier;}
        const std::vector<Element>& path() const    {return _path;}

        const Value* eval(const Value *root) const;

        class Element {
        public:
            Element(slice expr);
            const Value* eval(const Value*) const;
            bool isKey() const                      {return _key != nullptr;}
            Dict::key& key() const                  {return *_key;}
            int32_t index() const                   {return _index;}
        private:
            std::unique_ptr<Dict::key> _key {nullptr};
            int32_t _index {0};
        };

    private:
        const std::string _specifier;
        std::vector<Element> _path;
    };

}
