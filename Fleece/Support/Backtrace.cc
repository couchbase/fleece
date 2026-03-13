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

namespace fleece {
    using namespace std;
    using namespace signal_safe;

    Backtrace::frameInfo Backtrace::getFrame(unsigned i) const {
        precondition(i < _addrs.size());
        return getFrame(_addrs[i], i == 0);
    }


}  // namespace fleece

namespace fleece {

    char* unmangle(const char* function);

    Backtrace::~Backtrace() { free(_symbols); }

    std::string Unmangle(const char* name NONNULL) {
        auto        unmangled = unmangle(name);
        std::string result    = unmangled;
        if ( unmangled != name ) free(unmangled);
        return result;
    }

    std::string Unmangle(const std::type_info& type) { return Unmangle(type.name()); }

    shared_ptr<Backtrace> Backtrace::capture(unsigned skipFrames, unsigned maxFrames, void* context) {
        // (By capturing afterwards, we avoid the (many) stack frames associated with make_shared)
        auto bt = make_shared<Backtrace>(0, 0);
        bt->_capture(skipFrames + 1, maxFrames, context);
        return bt;
    }

    Backtrace::Backtrace(unsigned skipFrames, unsigned maxFrames, void* context) {
        if ( maxFrames > 0 ) _capture(skipFrames + 1, maxFrames, context);
    }

    void Backtrace::_capture(unsigned skipFrames, unsigned maxFrames, void* context) {
        skipFrames += 2;  // skip this frame and its child
        _addrs.resize(skipFrames + maxFrames);
        auto n = raw_capture(&_addrs[0], skipFrames + maxFrames, context);
        _addrs.resize(n);
        skip(skipFrames);
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
