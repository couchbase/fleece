![Build and Test](https://github.com/couchbaselabs/fleece/workflows/Build%20and%20Test/badge.svg)

# Fleece

__Fleece__ is a binary encoding for semi-structured data. Its data model is a superset of JSON, adding support for binary values. It is designed to be:

* **Very fast to read:** No parsing is needed, and the data can be navigated and read without any heap allocation. Fleece objects are internal pointers into the raw data. Arrays and dictionaries can be random-accessed. Performance on real-world-scale data has been clocked at 20x that of JSON. (Want proof? See the [benchmark](Performance.md).)
* **Compact:** Simple values will be about the same size as JSON. Complex ones may be much smaller, since repeated values, especially strings, only need to be stored once.
* **Efficient to convert into native objects:** Numbers are binary, strings are raw UTF-8 without quoting, binary data is not base64-encoded. Storing repeated values once means they only need to be converted into native objects once.
* **Appendable:** Fleece is what's known as a [persistent data structure](https://en.wikipedia.org/wiki/Persistent_data_structure). A Fleece document can be mutated by appending data to it. The mutation is in effect a delta, so it's usually much smaller than the original document. And the original document is unchanged, which is great for concurrency as well as (simple) version control.

## What You Get

* Documentation, including
    * **High-level API guide "[Using Fleece](https://github.com/couchbaselabs/fleece/wiki/Using-Fleece)"**
    * The [design document](Fleece.md), with details on the data format and other internals
    * An [example](Example.md) showing the details of the encoding of a specific data structure, and a walkthrough of what happens when a program works with the resulting Fleece objects
* A C++ reference implementation, including:
    * Encoder and decoder/accessors
    * Extensions for converting JSON directly to Fleece or vice versa
    * Extensions for encoding from and decoding to Objective-C (Foundation) object trees
    * Extensions for mutable values, making it easy to modify Fleece documents and then save them again
    * Extensions for delta compression
    * Unit tests
    * Some simple performance benchmarks
* C++ and C APIs
* A command-line tool, `fleece`, that can convert JSON to Fleece or vice versa, or dump Fleece data in a human-readable form that shows the internal structure
* Some experimental stuff:
    * A [Hash-Array-Mapped Trie](https://en.wikipedia.org/wiki/Hash_array_mapped_trie) implementation for building highly scaleable persistent hash tables in Fleece
    * At the other extreme, an extremely compact binary tree of strings that might find a use someday

## FAQ

**Q: Why does the world need yet another binary JSON encoding?**  
A: Excellent question, sock puppet! Fleece is different from [BSON](http://bsonspec.org), [PSON](https://github.com/dcodeIO/PSON), etc. in that it's been carefully designed to not need parsing or heap allocation. In performance tests with other binary formats I found that, while they were faster to parse than JSON, the total time was still dominated by allocating and freeing the resulting objects, as well as the conversion from UTF-8 data to platform strings. (I was using Objective-C, but similar issues would arise if using STL or GLib or other collection frameworks.) The way around this is to structure the encoded data more like a memory dump, with "pointers" (relative byte offsets) and fixed-width random-accessible arrays. That's what Fleece does. As a result, it's many times faster to work with than JSON; [literally _20x faster_](Performance.md) in the included benchmark run on a Macbook Pro.

**Q: Can I use it in \$LANGUAGE?** [where \$LANGUAGE not in ("C++", "C")]  
A: Not currently. It would be very nice to more bindings, and the C API should make that fairly
straightforward since it's easy to call from other languages. (But any real API should follow
the language's idioms, instead of being a direct translation!)

**Q: Why didn't you write this in \$NEW_LANGUAGE instead of crufty C++?**  
A: I probably should have! \$NEW_LANGUAGE is deservedly attracting a lot of attention for its combination of safety, readable syntax, and support for modern programming paradigms. I've been trying out \$NEW_LANGUAGE and want to write more code in it. But for this I chose C++ because it's supported on all platforms, lots of people know how to use it, and it still supports high-level abstractions (unlike C.)

**Q: Why the name "Fleece"?**  
A: It's a reference to the mythical [Golden Fleece](https://en.wikipedia.org/wiki/Golden_Fleece), the treasure sought by Jason and the Argonauts.

**Q: Who wrote this?**  
[Jens Alfke](https://github.com/snej), with design input from [Volker Mische](https://github.com/vmx) and [Dave Rigby](https://github.com/daverigby), and much help with portability and bug-fixing from [Jim Borden](https://github.com/borrrden). (And thanks to Mark Nunberg for the excellent [jsonsl](https://github.com/mnunberg/jsonsl) parser.)

## Status

Fleece is in active use and development. It is a core component of Couchbase Lite, via the [LiteCore](https://github.com/couchbase/couchbase-lite-core) library.

## Requirements / Compatibility

* Fleece builds with Xcode, Clang, GCC and MSVC. Most of the code is C++17, but some newer parts need C++20. (Yes, there are some Objective-C++ source files (`.mm`), but those are only used to provide Objective-C glue and Mac/iOS specific benchmarks. You can ignore them on other platforms.)
* There are no dependencies on any external libraries, other than the standard C library and the C++ STL.
* It _should_ work correctly, and create interoperable data, on both little-endian and big-endian CPUs, but admittedly we are not currently testing or using Fleece on any big-endian platforms.

We use GitHub CI, covering Clang, GCC, Xcode and MSVC on macOS, Linux and Windows. If you encounter portability problems, please file an issue and we'll fix it.

## License

Couchbase Business Source License
