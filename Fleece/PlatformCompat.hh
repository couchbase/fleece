//
//  PlatformCompat.hh
//  Fleece
//
//  Created by Jens Alfke on 9/8/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once


#ifdef _MSC_VER
    #include <cstdio>
    #define _usuallyTrue(VAL)               (VAL)
    #define _usuallyFalse(VAL)              (VAL)
    #define NOINLINE                        __declspec(noinline)

    #define __has_extension(X)              0
    #define __has_feature(F)                0
    #define __func__                        __FUNCTION__

    #define random()                        rand()
    #define srandom(s)                      srand(s)

    #define localtime_r(a, b)               localtime_s(b, a)

    #include <BaseTsd.h>
    typedef SSIZE_T ssize_t;

    #define MAXFLOAT FLT_MAX

    #define __printflike(A, B) 

    // MSVC doesn't support C99 so it doesn't have variable-length C arrays.
    // WARNING: sizeof() will not work on this array since it's actually declared as a pointer.
    #define StackArray(NAME, TYPE, SIZE)    TYPE* NAME = (TYPE*)_malloca(sizeof(TYPE)*(SIZE))

    #define fdopen ::_fdopen
    #define fseeko fseek
    #define ftello ftell
    #define strncasecmp _strnicmp
    #define strcasecmp _stricmp

    namespace fleece {
        int mkdir_u8(const char* const path, int mode);
        int stat_u8(const char* const filename, struct stat* const s);
        int rmdir_u8(const char* const path);
        int rename_u8(const char* const oldPath, const char* const newPath);
        int unlink_u8(const char* const filename);
        int chmod_u8(const char* const filename, int mode);
        FILE* fopen_u8(const char* const path, const char* const mode);
    }
#else

    #define _usuallyTrue(VAL)               __builtin_expect(VAL, true)
    #define _usuallyFalse(VAL)              __builtin_expect(VAL, false)
    #define NOINLINE                        __attribute((noinline))

    #ifndef __printflike
    #define __printflike(fmtarg, firstvararg) __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
    #endif
    #ifndef __unused
    #define __unused __attribute((unused))
    #endif

    #define StackArray(NAME, TYPE, SIZE)    TYPE NAME[(SIZE)]

    namespace fleece {
        inline int mkdir_u8(const char* const path, int mode) {
            return ::mkdir(path, (mode_t)mode);
        }

        inline int stat_u8(const char* const filename, struct ::stat* const s) {
            return ::stat(filename, s);
        }

        inline int rmdir_u8(const char* const path) {
            return ::rmdir(path);
        }

        inline int rename_u8(const char* const oldPath, const char* const newPath) {
            return ::rename(oldPath, newPath);
        }

        inline int unlink_u8(const char* const filename) {
            return ::unlink(filename);
        }

        inline int chmod_u8(const char* const filename, int mode) {
            return ::chmod(filename, mode);
        }

        inline FILE* fopen_u8(const char* const path, const char* const mode) {
            return ::fopen(path, mode);
        }
    }

#endif
