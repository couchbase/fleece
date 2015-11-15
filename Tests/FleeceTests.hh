//
//  FleeceTests.hh
//  Fleece
//
//  Created by Jens Alfke on 11/14/15.
//  Copyright © 2015 Couchbase. All rights reserved.
//

#ifndef FleeceTests_h
#define FleeceTests_h

#include "Value.hh"
#include "Encoder.hh"
#include "Writer.hh"
#include <ostream>

using namespace fleece;


// Less-obnoxious names for assertions:
#define Assert CPPUNIT_ASSERT
#define AssertEqual(ACTUAL, EXPECTED) CPPUNIT_ASSERT_EQUAL(EXPECTED, ACTUAL)


// Some operators to make slice work with AssertEqual:
std::ostream& operator<< (std::ostream& o, slice s);

std::string sliceToHex(slice);


#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>

#endif /* FleeceTests_h */
