# Fleece

__Fleece__ is a new binary encoding for semi-structured data. Its data model is a superset of JSON, adding support for binary values. It is designed to be:

* **Very fast to read:** No parsing is needed, and the data can be navigated and read without any heap allocation. Fleece objects are internal pointers into the raw data. Arrays and dictionaries can be random-accessed. (Want proof? See the [benchmark](Performance.md).)
* **Compact:** Simple values will be about the same size as JSON. Complex ones may be much smaller, since repeated values, especially strings, only need to be stored once.
* **Efficient to convert into native objects:** Numbers are binary, strings are raw UTF-8 without quoting, binary data is not base64-encoded. Storing repeated values once means they only need to be converted into native objects once.

## FAQ

**Q: Why does the world need yet another binary JSON encoding?**  
A: Excellent question, sock puppet! Fleece is different from [BSON](http://bsonspec.org), [PSON](https://github.com/dcodeIO/PSON), etc. in that it's been carefully designed to not need parsing. In performance tests with other binary formats I found that, while they were faster to parse than JSON, the total time was still dominated by allocating and freeing the resulting objects, as well as the conversion from UTF-8 data to platform strings. (I was using Objective-C, but similar issues would arise if using STL or GLib or other collection frameworks.) The way around this is to structure the encoded data more like a memory dump, with "pointers" (relative byte offsets) and fixed-width random-accessible arrays. That's what Fleece does. As a result, it's many times faster to work with than JSON; [literally _20x faster_](Performance.md) in the included benchmark run on a Macbook Pro.

**Q: Why the name "Fleece"?**  
A: It's a reference to the mythical [Golden Fleece](https://en.wikipedia.org/wiki/Golden_Fleece), the treasure sought by Jason and the Argonauts.

## Contents

* Documentation, including
    * The [design document](Fleece.md), with details on the data format
    * An [example](Example.md) showing the details of the encoding of a specific data structure, and a walkthrough of what happens when a program works with the resulting Fleece objects
    * [Performance](Performance.md) figures based on the included test suite, including comparisons to JSON parsing using Apple's Foundation framework
* A C++ reference implementation, including
    * Encoder and decoder/accessors
    * Extensions for converting JSON directly to Fleece or vice versa
    * Extensions for encoding from and decoding to Objective-C (Foundation) object trees.
    * Unit tests
    * Some simple performance benchmarks
* A command-line tool, `fleece`, that can convert JSON to Fleece or vice versa, or dump Fleece data in a human-readable form that shows the internal structure

## Status

Fleece has been used in some experimental work at Couchbase, but hasn't gone into production use.

Thus far it's only been compiled with Xcode and run on Mac OS and iOS.

## Requirements / Compatibility

* Fleece _should_ be buildable with any C++11 compiler, such as current versions of Xcode, Clang, GCC or MSVC. (Yes, there are some Objective-C++ source files (`.mm`), but those are only used to provide Objective-C glue and Mac/iOS specific benchmarks. You can ignore them on other platforms.)
* There _should_ be no dependencies on any libraries, other than the standard C library and the C++ STL.
* It _should_ work correctly, and create interoperable data, on both little-endian and big-endian CPUs.

However, none of those "should"s have been put to test yet, so it's likely they're not 100% true. If you encounter problems on non-Apple platforms, please file an issue and we'll fix it.

Yes, it would be very nice to have a non-C++ API; we just haven't written one yet. A C wrapper would come first, and then that can be glued to other languages. Contributions are welcome :)

## License

Apache 2.0
