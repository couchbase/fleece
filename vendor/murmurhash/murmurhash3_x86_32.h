//
//  murmurhash3_x86_32.h
//  CBJSON
//
//  Created by Jens Alfke on 12/29/13.
//  Copyright (c) 2013 Couchbase. All rights reserved.
//

#ifndef CBJSON_murmurhash3_x86_32_h
#define CBJSON_murmurhash3_x86_32_h

#include <stdint.h>

void MurmurHash3_x86_32 ( const void * key, int len,
                         uint32_t seed, void * out );

#endif
