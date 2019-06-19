//
// Path.hh
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

#pragma once
#include "Dict.hh"
#include "SmallVector.hh"
#include "function_ref.hh"
#include <iostream>
#include <memory>
#include <string>
#include <functional>

namespace fleece { namespace impl {
    class SharedKeys;

    /** Describes a location in a Fleece object tree, as a path from the root that follows
        dictionary properties and array elements.
        Similar to a JSONPointer or an Objective-C KeyPath, but simpler (so far.)
        It looks like "foo.bar[2][-3].baz" -- that is, properties prefixed with a ".", and array
        indexes in brackets. (Negative indexes count from the end of the array.)
        A leading JSONPath-like "$." is allowed but ignored.
        A '\' can be used to escape a special character ('.', '[' or '$') at the start of a
        property name (but not yet in the middle of a name.) */
    class Path {
    public:
        class Element;

        //// Construction from a string: (throws FleeceException with code PathSyntaxError)

        Path(slice specifier)                       {addComponents(specifier);}
        Path(const std::string &specifier)          :Path(slice(specifier)) { }

        //// Step-by-step construction:

        Path()                                      { }
        void addProperty(slice key);
        void addIndex(int index);
        void addComponents(slice components);

        Path& operator += (const Path&);
        void drop(size_t numToDropFromStart);

        //// Path element accessors:

        const smallVector<Element,4>& path() const      {return _path;}
        smallVector<Element,4>& path()                  {return _path;}
        bool empty() const                              {return _path.empty();}
        size_t size() const                             {return _path.size();}

        const Element& operator[] (size_t i) const      {return _path[i];}
        Element& operator[] (size_t i)                  {return _path[i];}

        //// Evaluation:

        const Value* eval(const Value *root NONNULL) const noexcept;

        /** One-shot evaluation; faster if you're only doing it once */
        static const Value* eval(slice specifier,
                                 const Value *root NONNULL);

        /** Evaluates a JSONPointer string (RFC 6901), which has a different syntax.
            This can only be done one-shot since JSONPointer path components are ambiguous unless
            the actual JSON is present (a number could be an array index or dict key.) */
        static const Value* evalJSONPointer(slice specifier,
                                            const Value* root NONNULL);

        //// Converting to string:

        explicit operator std::string();

        void writeTo(std::ostream&) const;

        /** Utility for writing a path component to a stream.
            It will add a backslash before any '.' and '[' characters.
            If `first` is true it will also backslash-escape a leading '$'.
            If `first` is false, it will prefix a '.'. */
        static void writeProperty(std::ostream&, slice key, bool first =false);

        /** Utility for writing a path component to a stream. */
        static void writeIndex(std::ostream&, int arrayIndex);


        /** An element of a Path, representing either a named property or an array index. */
        class Element {
        public:
            Element(slice property);
            Element(int32_t arrayIndex)             :_index(arrayIndex) { }
            Element(const Element &e);
            bool isKey() const                      {return _key != nullptr;}
            Dict::key& key() const                  {return *_key;}
            slice keyStr() const                    {return _key ? _key->string() : slice();}
            int32_t index() const                   {return _index;}

            const Value* eval(const Value* NONNULL) const noexcept;
            static const Value* eval(char token, slice property, int32_t index,
                                     const Value *item NONNULL) noexcept;
        private:
            static const Value* getFromArray(const Value* NONNULL, int32_t index) noexcept;

            alloc_slice _keyBuf;
            std::unique_ptr<Dict::key> _key {nullptr};
            int32_t _index {0};
        };

    private:
        using eachComponentCallback = function_ref<bool(char,slice,int32_t)>;
        static void forEachComponent(slice in, bool atStart, eachComponentCallback);

        smallVector<Element, 4> _path;
    };

} }
