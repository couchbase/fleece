//
// LibC++Debug.cc
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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
