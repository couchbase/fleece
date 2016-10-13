# Fleece Encoding Example

This document shows what Fleece looks like, both as raw data and as an annotated dump, and then gives an example of working with the Fleece data directly in a program without needing to parse it into any other form (or, indeed, allocate any extra heap memory.)

Let's work with the data structure expressed by the following JSON (taken from the [PSON](https://github.com/dcodeIO/PSON#tldr-numbers-please) documentation):

```json
{
    "hello": "world!",
    "time": 1234567890,
    "float": 0.01234,
    "boolean": true,
    "otherbool": false,
    "null": null,
    "obj": {
        "what": "that"
    },
    "arr": [1,2,3]
}
```

Without the pretty-printing, this is 133 bytes.

## Binary Fleece Data

Encoding as Fleece produces the following data. (To be precise, this is the output of `fleece --encode example.json | hexdump -C`.)

```
00000000  45 68 65 6c 6c 6f 46 77  6f 72 6c 64 21 00 44 74  |EhelloFworld!.Dt|
00000010  69 6d 65 00 1b d2 02 96  49 00 45 66 6c 6f 61 74  |ime.....I.Efloat|
00000020  28 00 f6 0b 76 c3 b6 45  89 3f 47 62 6f 6f 6c 65  |(...v..E.?Gboole|
00000030  61 6e 49 6f 74 68 65 72  62 6f 6f 6c 44 6e 75 6c  |anIotherboolDnul|
00000040  6c 00 43 6f 62 6a 44 77  68 61 74 00 44 74 68 61  |l.CobjDwhat.Dtha|
00000050  74 00 70 01 80 07 80 05  43 61 72 72 60 03 00 01  |t.p.....Carr`...|
00000060  00 02 00 03 70 08 80 07  80 06 80 20 38 00 80 2a  |....p...... 8..*|
00000070  80 28 80 39 80 37 80 1d  30 00 80 1c 80 15 80 26  |.(.9.7..0......&|
00000080  34 00 80 3a 80 38 80 11                           |4..:.8..|
```

At 136 bytes this is not any smaller than the JSON, but that's not the goal. The benefit of Fleece becomes apparent when we look at the internal structure of the data...

## Annotated Dump Of Fleece Structure

Here's a dump produced by `fleece --dump` that shows the structure:

```
0000: 45 68 65 6c…: "hello"
0006: 46 77 6f 72…: "world!"
000e: 44 74 69 6d…: "time"
0014: 1b d2 02 96…: 1234567890
001a: 45 66 6c 6f…: "float"
0020: 28 00 f6 0b…: 0.01234
002a: 47 62 6f 6f…: "boolean"
0032: 49 6f 74 68…: "otherbool"
003c: 44 6e 75 6c…: "null"
0042: 43 6f 62 6a : "obj"
0046: 44 77 68 61…: "what"
004c: 44 74 68 61…: "that"
0052: 70 01       : Dict[1]:
0054: 80 07       :   &"what" (@0046)
0056: 80 05       :     &"that" (@004c)
0058: 43 61 72 72 : "arr"
005c: 60 03       : Array[3]:
005e: 00 01       :   1
0060: 00 02       :   2
0062: 00 03       :   3
0064: 70 08       : Dict[8]:
0066: 80 07       :   &"arr" (@0058)
0068: 80 06       :     &Array[3] (@005c)
006a: 80 20       :   &"boolean" (@002a)
006c: 38 00       :     true
006e: 80 2a       :   &"float" (@001a)
0070: 80 28       :     &0.01234 (@0020)
0072: 80 39       :   &"hello" (@0000)
0074: 80 37       :     &"world!" (@0006)
0076: 80 1d       :   &"null" (@003c)
0078: 30 00       :     null
007a: 80 1c       :   &"obj" (@0042)
007c: 80 15       :     &Dict[1] (@0052)
007e: 80 26       :   &"otherbool" (@0032)
0080: 34 00       :     false
0082: 80 3a       :   &"time" (@000e)
0084: 80 38       :     &1234567890 (@0014)
0086: 80 11       : &Dict[8] (@0064)
```

### Huh?

What's going on here? Even with the annotations, the structure of this doesn't look anything like the JSON. Why does it start with a bunch of strings?

Fleece isn't like most other serialization formats. It isn't a syntax to be parsed, it's really more like a memory dump of an object tree, including pointers:

* Objects start with a type identifier. This is conceptually like a vtable or "isa" pointer, except that it's only 4 bits wide and the 16 values are predefined.
* Pointers are always expressed as relative backwards offsets, so the data is location-independent. (In this example only 16-bit offsets are used, but Fleece will use 32-bit when necessary.)

What this means is that **a (real) pointer to a Fleece object inside the raw data can be used directly, without any need to parse the Fleece first.** The object's type is easily discoverable, its intrinsic data is inline at that location, and any pointers to other objects (like array items) can be followed as relative offsets, resulting in (real) pointers to those objects. The result is that we have a full object graph we can work with, without having to parse anything or allocate any extra memory!

## Working With Fleece

Here's an example of how the above data could be read and used from a C++ program, with an indented explanation of what the Fleece library is doing behind the scenes.

1. You load the data into memory somehow. Let's say it ends up at address bfff0000.
2. You pass that pointer/length pair to `value::fromData()`.
    * It quickly scans the data to ensure it's valid -- most importantly, that all "pointers" (offsets) actually point inside the data.
    * It reads the 2-byte "pointer" at the end of the data (`80 11`), interprets it as a 34-byte backward offset, and arrives at address bfff0064, i.e. byte 64 of the data, the location of the root dictionary.
    * It returns this root pointer as a `const value*`.
3. You call `asDict()` on the returned value, which returns the same pointer cast to a `const dict*`.
    * The method looks at the object type in the first 4 bits to make sure it's a dictionary. If the object hadn't been a dictionary, `asDict()` would have returned `nullptr`, which you can check for and treat as an error.
4. You call `get()` on the `dict` to look up the key `"arr"`, and get back a pointer to the associated value.
    * The data inside the dict consists of alternating keys and values. Since these are fixed-width, and the keys are pre-sorted, Fleece can use binary search to look up keys in O(log _n_) time.
    * In this case it finds the key `"arr"` in 3 comparisons. (Note that the keys are stored in the dictionary as 2-byte "pointers" to the actual string data. The dump prefixes these with `&`. Most of the values are stored as "pointers" too, except for `true`, `false` and `null` which all fit inline in 2 bytes.)
    * The value (at address bfff0068, offset 68) is `80 06`. The `8` means this is a pointer, not inline, and the `006` encodes a 12-byte backward offset. This resolves to address bfff005c, which it returns as a `const value*`.
5. You call `type()` to check what type of value this is, and get back `kArray`.
    * Fleece simply uses the upper 4 bits of the first byte (`6`) to determine the object type.
6. Since it's an array, you decide to iterate over it. You directly cast the pointer to `const array*` and construct an `array::iterator` from that, then advance the iterator to get the values one by one.
    * Internally, the iterator constructor just extracts the array's length and the address of the first value. As you increment the iterator, it decrements its count and advances the value pointer (by 2 or 4 bytes.)
7. Alternatively, you could treat the iterator as an array and use `[]` to access indexes at random. 
    * Random access is as efficient as a C array because the array items, like dictionary items, are fixed width.
8. When you access an array item, the `type()` method will tell you it's a number
    * The upper `0` in the first byte indicates a short integer.
9. Calling `asInt()` on the item returns the value.
    * This just does some bit-twiddling of the inline 16-bit value to extract the 12 bits, then sign-extends it to a full integer.

Note that you accomplished all of this without _any_ heap allocations (except possibly for loading the Fleece data in step 1)!
