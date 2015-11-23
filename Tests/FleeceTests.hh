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
#include <time.h>

using namespace fleece;


// Less-obnoxious names for assertions:
#define Assert CPPUNIT_ASSERT
#define AssertEqual(ACTUAL, EXPECTED) CPPUNIT_ASSERT_EQUAL(EXPECTED, ACTUAL)


// Some operators to make slice work with AssertEqual:
std::ostream& operator<< (std::ostream& o, slice s);

std::string sliceToHex(slice);


alloc_slice readFile(const char *path);
void writeToFile(slice s, const char *path);


struct mmap_slice : public slice {
    mmap_slice(const char *path);
    ~mmap_slice();

private:
    int _fd;
    void* _mapped;
    mmap_slice(const mmap_slice&);
};


class Stopwatch {
public:
    Stopwatch()         :_start(::clock()) { }
    void reset()        {_start = clock();}
    double elapsed()    {return (clock() - _start) / (double)CLOCKS_PER_SEC;}
    double elapsedMS()  {return elapsed() * 1000.0;}
private:
    clock_t _start;
};


#include <cppunit/TestCase.h>
#include <cppunit/extensions/HelperMacros.h>

#endif /* FleeceTests_h */
