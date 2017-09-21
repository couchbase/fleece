//
//  Internal.hh
//  Fleece
//
//  Created by Jens Alfke on 11/16/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

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
 0010s--- --------...    floating point (s = 0:float, 1:double). LE float data follows.
 0011ssss --------       special values like null, true, false (see kSpecialValue constants below)
 0100cccc ssssssss...    string (cccc is byte count, or if it’s 15 then count follows as varint)
 0101cccc dddddddd...    binary data (same as string)
 0110wccc cccccccc...    array (c = 11-bit item count, if 2047 then count follows as varint;
                                w = wide, if 1 then following values are 4 bytes wide, not 2)
 0111wccc cccccccc...    dictionary (same as array)
 1ooooooo oooooooo       pointer (o = BE unsigned offset in units of 2 bytes back; up to -64kbytes)
                                NOTE: In a wide collection, offset field is 31 bits wide

 Bits marked "-" are reserved and should be set to zero.
*/

namespace fleece {
    namespace internal {

        enum {
            kNarrow = 2,
            kWide   = 4,
            kFat    = 16,
        };

        static inline int width(uint8_t wide) {
            static constexpr int kWidths[3] = {kNarrow, kWide, kFat};
            return kWidths[wide];
        }

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

        // First byte of a special value (including the tag plus the 'ssss' bits)
        enum {
            kSpecialValueNull         = (kSpecialTag << 4) | 0x00,   // 0011 0000
            kSpecialValueFalse        = (kSpecialTag << 4) | 0x04,   // 0011 0100
            kSpecialValueTrue         = (kSpecialTag << 4) | 0x08,   // 0011 1000

            // These special values are never stored; they're only found as Value*s in memory.
            kSpecialValueMutableArray = (kSpecialTag << 4) | 0x01,   // 0011 0001
            kSpecialValueMutableDict  = (kSpecialTag << 4) | 0x02,   // 0011 0010
        };

        // Min/max length of string that will be considered for sharing
        // (not part of the format, just a heuristic used by the encoder & Obj-C decoder)
        static const size_t kMinSharedStringSize =  2;
        static const size_t kMaxSharedStringSize = 15;

        // Minimum array count that has to be stored outside the header
        static const uint32_t kLongArrayCount = 0x07FF;

        class MutableValue;

#ifndef NDEBUG
        extern std::atomic<unsigned> gTotalComparisons;
#endif

    }
}
