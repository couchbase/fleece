//
//  sliceIO.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/slice.hh"
#include <string>
#include <stdio.h>

// True if we can use stdio's file functions.
#ifndef FL_HAVE_FILESYSTEM
#define FL_HAVE_FILESYSTEM 1
#endif

#if FL_HAVE_FILESYSTEM

namespace fleece {

    alloc_slice readFile(const char *path);

    void writeToFile(slice s, const char *path, int mode);
    void writeToFile(slice s, const char *path);
    void appendToFile(slice s, const char *path);

}

#endif // FL_HAVE_FILESYSTEM

