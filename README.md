# Fleece

__Fleece__ is a new binary encoding for semi-structured data. Its data model is a superset of JSON, adding support for binary values. It is designed to be:

* **Very fast to read:** No parsing is needed, and the data can be navigated and read without any heap allocation. Fleece objects are internal pointers into the raw data. Arrays and dictionaries can be random-accessed. Performance on real-world-scale data has been clocked at 20x that of JSON. (Want proof? See the [benchmark](Performance.md).)
* **Compact:** Simple values will be about the same size as JSON. Complex ones may be much smaller, since repeated values, especially strings, only need to be stored once.
* **Efficient to convert into native objects:** Numbers are binary, strings are raw UTF-8 without quoting, binary data is not base64-encoded. Storing repeated values once means they only need to be converted into native objects once.

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

## FAQ

**Q: Why does the world need yet another binary JSON encoding?**  
A: Excellent question, sock puppet! Fleece is different from [BSON](http://bsonspec.org), [PSON](https://github.com/dcodeIO/PSON), etc. in that it's been carefully designed to not need parsing. In performance tests with other binary formats I found that, while they were faster to parse than JSON, the total time was still dominated by allocating and freeing the resulting objects, as well as the conversion from UTF-8 data to platform strings. (I was using Objective-C, but similar issues would arise if using STL or GLib or other collection frameworks.) The way around this is to structure the encoded data more like a memory dump, with "pointers" (relative byte offsets) and fixed-width random-accessible arrays. That's what Fleece does. As a result, it's many times faster to work with than JSON; [literally _20x faster_](Performance.md) in the included benchmark run on a Macbook Pro.

**Q: Can I use it in $LANGUAGE?** [where $LANGUAGE != "C++"]  
Not currently. It would be very nice to have a non-C++ API; we just haven't written one yet. A C wrapper would come first, and then that can be glued to other languages. Contributions are welcome :)

**Q: Why didn't you write this in $NEW_LANGUAGE instead of crufty C++?**  
A: I probably should have! $NEW_LANGUAGE is deservedly attracting a lot of attention for its combination of safety, readable syntax, and support for modern programming paradigms. I've been trying out $NEW_LANGUAGE and want to write more code in it. But for this I chose C++ because it's supported on all platforms, lots of people know how to use it, and it still supports high-level abstractions (unlike C.)

**Q: Why did you only benchmark it against Cocoa's Foundation classes? Those are slow.**  
Because Foundation is what I know and work with. I'd love to incorporate benchmarks of other frameworks; please send a pull request.

**Q: Why the name "Fleece"?**  
A: It's a reference to the mythical [Golden Fleece](https://en.wikipedia.org/wiki/Golden_Fleece), the treasure sought by Jason and the Argonauts.

**Q: Who wrote this?**  
[Jens Alfke](https://github.com/snej), with input from [Volker Mische](https://github.com/vmx) and [Dave Rigby](https://github.com/daverigby). (And thanks to Mark Nunberg for the excellent [jsonsl](https://github.com/mnunberg/jsonsl) parser.)

## Status

Fleece has been used in some experimental work at Couchbase, but hasn't gone into production use.

Thus far it's only been compiled with Xcode and run on Mac OS and iOS.

## Requirements / Compatibility

* Fleece _should_ be buildable with any C++11 compiler, such as current versions of Xcode, Clang, GCC or MSVC. (Yes, there are some Objective-C++ source files (`.mm`), but those are only used to provide Objective-C glue and Mac/iOS specific benchmarks. You can ignore them on other platforms.)
* There _should_ be no dependencies on any libraries, other than the standard C library and the C++ STL.
* It _should_ work correctly, and create interoperable data, on both little-endian and big-endian CPUs.

However, none of those "should"s have been put to test yet, so it's likely they're not 100% true. If you encounter problems on non-Apple platforms, please file an issue and we'll fix it.

## License

Apache 2.0
