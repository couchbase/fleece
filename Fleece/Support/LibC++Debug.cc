//
// LibC++Debug.cc
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#ifdef __APPLE__
#ifdef _LIBCPP_DEBUG

#include <__debug>

namespace std {
    // Resolves a link error building with libc++ in debug mode. Apparently this symbol would be in
    // the debug version of libc++.dylib, but we don't have that on Apple platforms.
    __1::__libcpp_debug_function_type __1::__libcpp_debug_function;
}

#endif
#endif
