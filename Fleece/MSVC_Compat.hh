//
//  MSVC_Compat.hh
//  Fleece
//
//  Created by Jens Alfke on 9/8/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once


#ifdef _MSC_VER

    #define __builtin_expect(VAL, EXPECTED) (VAL)

    #define __has_extension(X)              0
    #define __has_feature(F)                0
    #define __func__                        __FUNCTION__

    #define alloca(SIZE)                    _malloca(SIZE)
    #define random()                        rand()
    #define srandom(seed)                   srand(seed)

    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;

    // MSVC doesn't support C99 so it doesn't have variable-length C arrays.
    // WARNING: sizeof() will not work on this array since it's actually declared as a pointer.
    #define StackArray(NAME, TYPE, SIZE)    TYPE* NAME = (TYPE*)_malloca(sizeof(TYPE)*(size))

#else

    #define StackArray(NAME, TYPE, SIZE)    TYPE NAME[(SIZE)]

#endif
