//
// Backtrace.cc
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Backtrace.hh"
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string.h>
#include <algorithm>
#include "betterassert.hh"

#if defined(_MSC_VER) && !defined(__clang__)
 #  include <intrin.h>
 #  define CAPTURE_RETURN_ADDRESS() _ReturnAddress()
 #else
 #  define CAPTURE_RETURN_ADDRESS() __builtin_return_address(0)
 #endif

namespace fleece {
    using namespace std;

    char* unmangle(const char* function);

    Backtrace::~Backtrace() { free(_symbols); }

    std::string Unmangle(const char* name NONNULL) {
        auto        unmangled = unmangle(name);
        std::string result    = unmangled;
        if ( unmangled != name ) free(unmangled);
        return result;
    }

    std::string Unmangle(const std::type_info& type) { return Unmangle(type.name()); }

    NOINLINE shared_ptr<Backtrace> Backtrace::capture(unsigned skipFrames, unsigned maxFrames, void* context) {
        // (By capturing afterwards, we avoid the (many) stack frames associated with make_shared)
        auto bt = make_shared<Backtrace>();
        void* anchor = CAPTURE_RETURN_ADDRESS();
        bt->_capture(anchor, skipFrames, maxFrames, context);
        return bt;
    }

    void Backtrace::_capture(void* anchor, unsigned skipFrames, unsigned maxFrames, void* context) {
        int maxSize = static_cast<int>(skipFrames + maxFrames + 10u); // A bit of extra headroom to find the anchor
        _addrs.resize(maxSize);
        bool found_anchor = false;
        auto n = raw_capture(&_addrs[0], maxSize, context);
        for (int i = 0; i < n; i++) {
            if (_addrs[i] == anchor ) {
                found_anchor = true;
                skipFrames += i;
                break;
            }
        }

        if (_usuallyFalse(!found_anchor)) {
            // Didn't find anchor, just try to skip an appropriate number of frames
            // (raw_capture, this one, and the caller, which is either capture or
            // Backtrace constructor
            skipFrames += 3;
        }

        skip(skipFrames);
        _addrs.resize(n - skipFrames);
    }

    void Backtrace::skip(unsigned nFrames) {
        _addrs.erase(_addrs.begin(), _addrs.begin() + min(size_t(nFrames), _addrs.size()));
    }

    string Backtrace::toString() const {
        stringstream out;
        writeTo(out);
        return out.str();
    }
}  // namespace fleece
