//
// Internal.hh
//
// Copyright 2015-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include <stdint.h>
#include <stdlib.h>

#ifndef NDEBUG
#include <atomic>
#endif

/*
 Value binary layout:

 0000iiii iiiiiiii       small integer (12-bit, signed, range ±2048)
 0001uccc iiiiiiii...    long integer (u = unsigned?; ccc = byte count - 1) LE integer follows
 0010ss-- --------...    floating point (see below for `ss` meaning). LE float data follows.
 0011ss-- --------       special (s = 0:null, 1:false, 2:true, 3:undefined)
 0100cccc ssssssss...    string (cccc is byte count, or if it’s 15 then count follows as varint)
 0101cccc dddddddd...    binary data (same as string)
 0110wccc cccccccc...    array (c = 11-bit item count, if 2047 then count follows as varint;
                                w = wide, if 1 then following values are 4 bytes wide, not 2)
 0111wccc cccccccc...    dictionary (same as array, but count refers to key/value pairs)
 1xoooooo oooooooo       pointer (x = external?, denotes ptr outside data to prev written data;
                                o = BE unsigned offset in units of 2 bytes back, up to -32KB)
                                NOTE: In a wide collection, offset field is 30 bits wide

 Bits marked "-" are reserved and should be set to zero.
*/

namespace fleece { namespace impl { namespace internal {

    enum {
        kNarrow = 2,
        kWide   = 4
    };

    static inline int width(bool wide) { return wide ? kWide : kNarrow; }

    // The actual tags used in the encoded data, i.e. high 4 bits of 1st byte:
    enum tags : uint8_t {
        kShortIntTag = 0,
        kIntTag,
        kFloatTag,
        kSpecialTag,
        kStringTag,
        kBinaryTag,
        kArrayTag,
        kDictTag,
        kPointerTagFirst = 8            // 9...15 are also pointers
    };

    // Interpretation of ss-- in a Float value:
    enum {
        kFloatValue32BitSingle  = 0x00,     // 0000  32-bit float
        kFloatValue32BitDouble  = 0x04,     // 0100  64-bit float encoded as 32-bit w/o data loss
        kFloatValue64BitDouble  = 0x08,     // 1000  64-bit float
    };

    // Interpretation of ss-- in a special value:
    enum {
        kSpecialValueNull       = 0x00,       // 0000
        kSpecialValueUndefined  = 0x0C,       // 1100
        kSpecialValueFalse      = 0x04,       // 0100
        kSpecialValueTrue       = 0x08,       // 1000
    };

    // Min/max length of string that will be considered for sharing
    // (not part of the format, just a heuristic used by the encoder & Obj-C decoder)
    static const size_t kMinSharedStringSize =  2;
    static const size_t kMaxSharedStringSize = 15;

    // Minimum array count that has to be stored outside the header
    static const uint32_t kLongArrayCount = 0x07FF;

    class Pointer;
    class HeapValue;
    class HeapCollection;
    class HeapArray;
    class HeapDict;

    // There is a sanity-check that prevents the use of numeric dict keys when there is no
    // SharedKeys in scope. The Encoder test case "DictionaryNumericKeys" needs to disable this
    // temporarily, so `gDisableNecessarySharedKeysCheck` is declared for that purpose, but
    // only in debug builds.
#ifdef NDEBUG
    constexpr bool gDisableNecessarySharedKeysCheck = false;
#else
    extern bool gDisableNecessarySharedKeysCheck;
    extern std::atomic<unsigned> gTotalComparisons;
#endif

// Value instances are only declared directly in a few special cases such as the constants
// Array::kEmpty and Dict::kEmpty. But in those cases it's essential that they be aligned
// on a 2-byte boundary, i.e. have even addresses, because Fleece treats the LSB of a pointer
// as a flag indicating a mutable value.
#ifndef _MSC_VER
    #define EVEN_ALIGNED __attribute__((aligned(2)))
#else
    #define EVEN_ALIGNED
#endif


} } }
