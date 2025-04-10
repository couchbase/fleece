//
//  TempArray.hh
//  blip_cpp
//
//  Created by Jens Alfke on 1/23/18.
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include <stdlib.h>
#include <memory>


/** Use TempArray() instead of declaring a variable-length C array, since those are not
    standard C++ and not supported by MSVC. In addition, a super-large C array can
    overflow the stack and lead to crashes or memory corruption! TempArray automatically
    allocates on the heap instead of the stack when the total size is 1024 bytes or more.

        Wrong:  int widgets[n];
        Right:  TempArray(widgets, int, n);

    WARNING: sizeof(widgets) will not work since `widgets` is actually declared as a pointer. */

#ifdef _MSC_VER

    #define TempArray(NAME, TYPE, SIZE) \
        std::unique_ptr<TYPE, decltype(_freea)*> NAME##_ptr((TYPE *)_malloca((SIZE) * sizeof(TYPE)), _freea);\
        TYPE* NAME = NAME##_ptr.get()

#else

    // class used internally by TempArray macro
    template <class T>
    struct _TempArray {
        _TempArray(size_t n)
        :_onHeap(n * sizeof(T) >= 1024)
        ,_array( _onHeap ? new T[n] : nullptr)
        { }

        ~_TempArray() {
            if (_onHeap)
                delete[] _array;
        }

        operator T* () LIFETIMEBOUND {return _array;}
        template <class U> explicit operator U* () LIFETIMEBOUND {return (U*)_array;}

        bool const _onHeap;
        T* _array;
    };


    #define TempArray(NAME, TYPE, SIZE) \
        _TempArray<TYPE> NAME(SIZE); \
        if (!NAME._onHeap && (SIZE) > 0) NAME._array = (TYPE*)alloca((SIZE)*sizeof(TYPE));

#endif
