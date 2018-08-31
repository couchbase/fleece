//
//  sliceIO.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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

