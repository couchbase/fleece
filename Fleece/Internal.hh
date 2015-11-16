//
//  Internal.hh
//  Fleece
//
//  Created by Jens Alfke on 11/16/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#ifndef Internal_h
#define Internal_h
#include <stdint.h>
#include <stdlib.h>

/*
 Value binary layout:

 0000iiii iiiiiiii       small integer (12-bit, signed, range ±2048)
 0001uccc iiiiiiii...    long integer (u = unsigned?; ccc = byte count - 1) LE integer follows
 0010cccc --------       floating point (cccc is byte count, always 4 or 8). LE float data follows.
 0011ssss --------       special (null, false, true)
 0100cccc ssssssss...    string (cccc is byte count, or if 15 then count follows as varint)
 0101cccc dddddddd...    binary data (same as string)
 0110cccc cccccccc...    array (c is 12-bit item count, if 4095 then count follows as varint)
 0111cccc cccccccc...    dictionary (ditto)
 1ooooooo oooooooo       pointer (o = BE signed offset in units of 2 bytes: ±32k bytes)
*/

namespace fleece {
    namespace internal {

        // The actual tags used in the encoded data:
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

        // Interpretation of ssss in a special value:
        enum {
            kSpecialValueNull = 0x00,       // 0000
            kSpecialValueFalse= 0x04,       // 0100
            kSpecialValueTrue = 0x08,       // 1000
        };

        // Min/max length of string that will be considered for sharing
        static const size_t kMinSharedStringSize =  2;
        static const size_t kMaxSharedStringSize = 99;

    }
}

#endif /* Internal_h */
