# Fleece

__Fleece__ is a new binary encoding for semi-structured data. Its data model is a superset of JSON, adding support for binary values. It is designed to be:

* **Very fast to read:** No parsing is needed, and the data can be navigated and read without any heap allocation. Fleece objects are internal pointers into the raw data. Arrays and dictionaries can be random-accessed.
* **Compact:** Simple values will be about the same size as JSON. Complex ones may be much smaller, since repeated values, especially strings, only need to be stored once.
* **Efficient to convert into native objects:** Numbers are binary, strings are raw UTF-8 without quoting, binary data is not base64-encoded. Storing repeated values once means they only need to be converted into native objects once.

## Contents

* The [design document](Fleece.md), with details on the data format
* A C++ reference implementation, including
  * Encoder and decoder/accessors
  * Extensions for converting JSON directly to Fleece or vice versa
  * Extensions for encoding from and decoding to Objective-C (Foundation) object trees.
  * Unit tests
* A command-line tool, `fleece`, that can convert JSON to Fleece or vice versa, or dump Fleece data in a human-readable form that shows the internal structure

## Status

It's early days! As of November 2015, this code is hot off the press and hasn't been put to
any serious use yet.

## License

Apache 2.0
