//
// Fleece.h
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
#ifndef _FLEECE_H
#define _FLEECE_H

// This "umbrella header" includes the commonly-used parts of the Fleece C API.

#include "FLBase.h"
#include "FLCollections.h"
#include "FLDeepIterator.h"
#include "FLDoc.h"
#include "FLEncoder.h"
#include "FLJSON.h"
#include "FLKeyPath.h"
#include "FLMutable.h"
#include "FLValue.h"

// #include "FLExpert.h"  -- advanced & rarely-used functionality

#ifdef __OBJC__
    // When compiling as Objective-C, include CoreFoundation / Objective-C utilities:
#   include "Fleece+CoreFoundation.h"
#endif

#endif // _FLEECE_H
