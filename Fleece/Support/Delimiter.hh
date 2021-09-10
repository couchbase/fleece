//
// Delimiter.hh
//
// Copyright 2021-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include <ostream>

namespace fleece {

    /** A utility to simplify writing a series of values to a stream with delimiters between
        them. Declare a `delimiter` object, then write it to the stream before each item.
        The first call will write nothing; subsequent calls will write the delimiter string.
        ```
        delimiter delim;
        for (auto &item : items)
            out << delim << item.description();
        ```
     */
    class delimiter {
    public:
        delimiter(const char *str =",") :_str(str) {}

        int count() const               {return _count;}
        const char* string() const      {return _str;}

        int operator++()                {return ++_count;} // prefix ++
        int operator++(int)             {return _count++;} // postfix ++

        const char* next()              {return (_count++ == 0) ? "" : _str;}

    private:
        int _count = 0;
        const char* const _str;
    };


    static inline std::ostream& operator<< (std::ostream &out, delimiter &delim) {
        if (delim++ > 0)
            out << delim.string();
        return out;
    }

}
