//
//  DeepIterator.hh
//  Fleece
//
//  Created by Jens Alfke on 4/17/18.
//  Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "Array.hh"
#include "Dict.hh"
#include <memory>
#include <vector>
#include <deque>

namespace fleece { namespace impl {
    class SharedKeys;


    /** A deep, hierarchical iterator of an entire container. All values in the container and its
        sub-containers will be visited. First the root itself is visited, then all the items in
        the root container, then all the items in its first sub-container, etc. So it's breadth-
        first within a container, but depth-first overall.
     
        Any container and its children can be skipped by calling skipChildren() when that
        container is visited.

        If you want to ignore the root container, either call next() immediately after creating
        the iterator, or during the iteration ignore the current value if path() is empty.

        The iteration is (obviously) not recursive, so it uses minimal stack space. It uses a
        small amount of heap space, roughly proportional to the number of sub-containers. */
    class DeepIterator {
    public:
        DeepIterator(const Value *root);

        inline explicit operator bool() const           {return _value != nullptr;}
        inline DeepIterator& operator++ ()              {next(); return *this;}

        /** The current value, or NULL if the iterator is finished. */
        const Value* value() const                      {return _value;}

        /** Call this to skip iterating the children of the current value. */
        void skipChildren()                             {_skipChildren = true;}

        /** Advances the iterator. */
        void next();

        struct PathComponent {
            slice key;          ///< Dict key, or nullslice if none
            uint32_t index;     ///< Array index, only if there's no key
        };

        /** The path to the current value. */
        const std::vector<PathComponent>& path() const  {return _path;}

        /** The path expressed as a string in JavaScript syntax using "." and "[]". */
        std::string pathString() const;

        /** The path to the current value, in JSONPointer (RFC 6901) syntax. */
        std::string jsonPointer() const;

        /** The Dict key of the current value, or nullkey if the parent is an Array. */
        slice keyString() const                         {return _path.empty() ? nullslice : _path.back().key;}

        /** The Array index of the current value, or 0 if the parent is a Dict. */
        uint32_t index() const                          {return _path.empty() ? 0 : _path.back().index;}

    private:
        bool iterateContainer(const Value *);
        void queueChildren();

        const SharedKeys* _sk {nullptr};
        const Value* _value;
        std::vector<PathComponent> _path;
        std::deque<std::pair<PathComponent,const Value*>> _stack;
        bool _skipChildren;
        std::unique_ptr<Dict::iterator> _dictIt;
        std::unique_ptr<Array::iterator> _arrayIt;
        uint32_t _arrayIndex;
    };

} }
