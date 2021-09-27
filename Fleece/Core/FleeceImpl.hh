//
// FleeceImpl.hh
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once

#ifndef __cplusplus
#error Using Fleece C++ API from C code. Please include Fleece.h instead.
#endif

#include "Value.hh"
#include "Array.hh"
#include "Dict.hh"
#include "Encoder.hh"
#include "JSONConverter.hh"
#include "SharedKeys.hh"
