//
//  Val.cc
//  Fleece
//
//  Created by Jens Alfke on 8/6/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "Val.hh"


namespace fleece {

    const uint8_t Val::kUndefined[2] = {0x30, 0x00};   // a 'null' value
    const uint8_t Arr::kUndefined[2] = {0x60, 0x00};   // an empty array
    const uint8_t Dic::kUndefined[2] = {0x70, 0x00};   // an empty array

}
