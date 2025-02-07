# Fleece Internals

Jens Alfke

__Fleece__ is a binary encoding for semi-structured data. Its data model is a superset of JSON, adding support for binary values. It is designed to be:

* Very fast to read: No parsing is needed, and the data can be navigated and read without any heap allocation. Fleece objects are internal pointers into the raw data. Arrays and dictionaries can be random-accessed.
* Compact: Simple values will be about the same size as JSON. Complex ones may be much smaller, since repeated values, especially strings, only need to be stored once. There is a facility for substituting small integers for common object keys, across any number of encoded documents, which significantly reduces size and speeds up object lookups.
* Efficient to convert into native objects: Numbers are binary, strings are raw UTF-8 without quoting, binary data is not base64-encoded. Storing repeated values once means they only need to be converted into native objects once.
* Amenable to delta compression, without needing complex algorithms like zdelta. It's very easy to encode a modified document as a delta from the original, and the delta merely needs to be appended to the original data to be read.
* Incrementally mutatable: you can make mutable copies of the original immutable values, modify them, and re-encode them back to compact data. This allocates memory proportional to the amount you've changed, regardless of the size of the entire data.

## 1. The Data Format

Fleece data consists of a sequence of **values**. Values are always 2-byte aligned, and occupy at least two bytes. The upper 4 bits of the first byte are a tag that identify the value's type. The value at the _end_ of the data is the primary top-level object, usually a dictionary. (This is an oversimplification; see Finding The Root, below.)

Some common values fit into exactly two bytes:

* `null`, `false`, `true`
* Integers in the range -2048...2047
* Empty or one-byte strings
* Empty arrays and dictionaries

Longer strings and integers include a byte count in the 2-byte header, followed by the variable-length data.

Floating-point numbers are 6 or 10 bytes long (2-byte header followed by a float or double.)

An array consists of a two-byte header, an item count (which fits in the header if it's less than 4096), and then a contiguous sequence of values. For fast random access, each value is the same length: 2 bytes in a regular **narrow** array, 4 bytes in a **wide** array.

### Dictionaries

Dictionaries are like arrays, except that each item consists of two values: a key followed by the value it maps to. The items are sorted by increasing key, to allow lookup by binary search. (For JSON compatibility the keys must be strings, but the format allows other types.)

Keys are sorted, to enable lookup by binary search. The key ordering is very simple: integers (q.v.) sort before strings, and strings are compared lexicographically as byte sequences, as if by memcmp, _not_ by any higher-level collation algorithms like Unicode.

#### Shared (Integer) Keys

Keys MAY take the form of non-negative small integers, if the "shared keys" optimization is being used. In this case, there is an external persistent mapping from integers to strings that's used to interpret the meanings of the integer keys. Using this optimization can significantly shrink the data size and speed up key lookups.

>**Note:** The shared-keys optimization obviously requires that all creators and consumers of the encoded data agree on a shared consistent mapping. This is outside the scope of Fleece. The library provides support for managing such a mapping, and using it while encoding and decoding, but leaves the persistent shared storage of the map (as an array of strings) up to the client.

>**Note:** If a given key string is part of the mapping, it MUST always be encoded as an integer, since readers will be expecting it to be an integer. (Otherwise readers would have to retry every failed integer lookup using the equivalent string, which would slow down access.)

#### Dictionary Inheritance

A dictionary may inherit from another dictionary that appears earlier: any keys that don't appear in it have the values they had in the earlier dict. In other words, keys in the newer dict override values from the older dict. A deletion is represented internally by a key with the special value `undefined`.

This inheritance is just an encoding shorthand, a form of delta compression, and is _not_ visible in the API. The dictionary acts like any other: the `get` method follows inheritance (and returns a null pointer for a deleted value, never `undefined`), and an iterator shows the effective contents including inherited values.

Inheritance is represented by a key with value -2048, whose value is a pointer to the inherited dictionary. Since -2048 is the lowest possible short integer, this key/value pair will always appear first. This makes it very easy to find. Also, since this key is out of range of the normal key space, the regular binary-search key lookup algorithm will never see it and doesn't have to treat it as a special case.

Multi-level inheritance is allowed, although more levels slow down lookups, so the encoder may want to use a heuristic to decide when to write the full dictionary.

### Pointers

How do values longer than 4 bytes fit in a collection? By using **pointers**. A pointer is a special value that represents a relative offset from itself to another value. Pointers always point back (toward lower addresses) to previously-written values.

Pointers are transparently dereferenced, like symlinks. So if a long value needs to be added to a collection, it's written outside (before) the collection, then a pointer to it is added.

(Narrow collections have 2-byte pointers with a range of up to 65534 bytes back. Wide collections have 4-byte pointers with a range of 4 gigabytes.)

Pointers also allow already-written values to be reused later on. If a string appears multiple times (very common for dictionary keys), it only has to be written once, and all the references to it can be pointers. The same can be done with numbers, though that’s not implemented yet. It's also possible to use pointers for repeated arrays or dictionaries, but detecting the duplicates would slow down writing.

>**Note:** The limited range of narrow pointers means that some very long arrays or dictionaries cannot be represented in narrow form, because at the end of the collection the free space before the collection is more than 64k bytes away, making it impossible to add a >2-byte value there. This is the main reason wide collections exist.

>**Note:** The trade-offs between narrow and wide collections are subtle. Narrow collections are generally more space-efficient, although 3- or 4-byte values use less space in a wide collection since they can be inlined. Narrow collections have limits on sharing of values due to limited pointer range; a string may have to be written twice if the two occurrances are >64kbytes apart. And of course, in some cases only a wide collection will work, as discussed in the previous note. The current encoder doesn't use all these criteria to decide, so it sometimes errs on the side of caution and emits a wide value when it could have been narrow.

### Finding The Root

Because Fleece is written bottom-up, the root object is at the end. Finding it can be a bit tricky. The procedure looks like this:

* Start two bytes from the end of the data.
* If this value is a pointer, dereference it (as a narrow pointer.)
* If _this_ value is a pointer, dereference it (as a _wide_ pointer.)

Now you're at the true root object.

The reason for the (rare) second pointer dereference is that the true root object may be larger than 64k bytes, in which case its start is too far back for a 2-byte pointer to reach. In that case the encoder writes a 4-byte pointer to it, then a 2-byte pointer pointing to _that_ since the data must by definition end with a 2-byte pointer.

### Details

```
 0000iiii iiiiiiii       small integer (12-bit, signed, range ±2048)
 0001uccc iiiiiiii...    long integer (u = unsigned?; ccc = byte count - 1) LE integer follows
 0010sx-- --------...    floating point (s = 0:32-bit, 1:64-bit). LE float data follows.
                                If x=1, value is a `double` even if encoded as 32-bit `float`.
 0011ss-- --------       special (s = 0:null, 1:false, 2:true, 3:undefined)
 0100cccc ssssssss...    string (cccc is byte count, or if it’s 15 then count follows as varint)
 0101cccc dddddddd...    binary data (same as string)
 0110wccc cccccccc...    array (c = 11-bit item count, if 2047 then overflow follows as varint;
                                w = wide, if 1 then following values are 4 bytes wide, not 2)
 0111wccc cccccccc...    dictionary (same as array, but each item is two values (key, value).
 1ooooooo oooooooo       pointer (o = BE unsigned offset in units of 2 bytes _backwards_, 0-64kb)
                                NOTE: In a wide collection, offset field is 31 bits wide
```
Bits marked “-“ are reserved and should be set to zero.

I’ve tried to make the encoding little-endian-friendly since nearly all mainstream CPUs are little-endian. But in the case of bitfields that span parts of multiple bytes — in small integers, array counts, and pointers — a little-endian ordering would make the bitfield disconnected, thus harder to decode, so I’ve made those big-endian. (I’m open to better ideas, though.)

### Example

Here is JSON `{“foo”: 123}` converted to Fleece:

| Offset | Bytes          | Explanation                   |
|--------|----------------|-------------------------------|
| 00     | 43 `f` `o` `o` | String `"foo"`                |
| 04     | 70 01          | Start of dictionary, count=1  |
| 06     | 80 03          | Key: Pointer, offset -6 bytes |
| 08     | 00 7B          | Value: Integer = 123          |
| 0A     | 80 03          | Trailing ptr, offset -6 bytes |

This occupies 12 bytes — one byte longer than JSON.

Using a wide dictionary, this would be:

| Offset | Bytes          | Explanation                   |
|--------|----------------|-------------------------------|
| 00     | 78 01          | Start of dictionary, count=1  |
| 02     | 43 `f` `o` `o` | Key: String `"foo"`           |
| 06     | 00 7B 00 00    | Value: Integer = 123          |
| 0A     | 80 05          | Trailing ptr, offset -10 bytes|

For a more complex case, see the [Example](Example.md) document.


## 2. Storage

### Reading

All Fleece values are self-contained: interpreting a value doesn't require accessing any external state. A value is also fast enough to parse that it's not necessary to keep a separate parsed form in memory. Because of this, the API can expose Fleece values as direct pointers into the document. These are opaque types, of course, but in C++ they are full-fledged objects of class Value (with subclasses Array and Dictionary). Working with Fleece data feels just like working with a (read-only) native object graph.

Collections provide iterators for sequential access, and getters for random access. Array random access is O(1) since the stored format is simply a native array of 16-bit values. Dictionary random access is O(log _n_), as it involves a binary search for the key.

### Writing

Fleece documents are created using an Encoder object that produces a data blob when it's done. Values are added one by one. A collection is added by making a `begin` call, then adding the values, then calling `end`. (A dictionary encoder requires a key to be written before each value.)

Behind the scenes, the data is written in bottom-up order: the values in a collection are written before the collection itself. The inline data in the collection is buffered while out-of-line values are written, then the collection object is written at the end. If the collection is a dictionary, the encoder first sorts the key/value pairs by key, to speed up lookups. (This can be turned off, but usually shouldn't be.)

The encoder keeps a hash table of strings that remembers the offset where each string has been written. Subsequent uses of the same string are encoded as pointers to that offset.

The encoder may optionally be given a SharedKeys object that manages a mapping of strings to integers. If so, it will consult that object before writing a dictionary key, and if it returns an integer, the encoder will write that instead. The SharedKeys class is adaptive, so when it's given a string not in the mapping, and the string is considered eligible for mapping, it will add it to the list and return the newly assigned integer. This allows the mapping to grow adaptively as data is encoded. Of course the client is responsible for persistent storage of the mapping, and making sure that readers have access to it as well.

#### Delta Encoding

A modified version of a document can be encoded as a _delta_: a shorter piece of data that, when appended to the original encoded document, forms a complete Fleece document containing the modified values. The reason the delta is shorter is that it encodes unchanged values as pointers back to where the values appear in the original data. The only values that have to be written explicitly to the delta are those that changed, plus their containers. The root value (usually a Dict) always has to be rewritten since it's the ultimate container of everything.

Dictionary inheritance makes deltas even more space-efficient, since only those key/value pairs that changed have to be rewritten.

A delta is generated by setting the `base` property of the Encoder to point to the original Fleece data (its address and length) before encoding begins. During encoding, the client should write the unchanged values as `Value` pointers (i.e. call `writeValue()`); the encoder will recognize a Value that points within the base document and write a pointer to it.

Literally appending the delta onto the original base document may be impractical. In that case, you can tell the Encoder to set the `extern` flag on pointers into the base; there's an optional parameter to `setBase` that enables this. You then need to open the delta document with a `Doc` and specify the (current) location of the base data in the `externData` constructor parameter.

### Scopes and Docs

As Fleece grew, it became important to attach some metadata to a block of Fleece data:
- A reference count, to provide semi-automatic memory managment
- A SharedKeys object that contains the string-to-small-integer mapping used to access numeric Dict keys

To do this without having to widen a Value pointer, we introduced a hack: there is a global data structure that maps address ranges to objects called `Scope`s. Each `Scope` instance has a `slice` or `alloc_slice` that references the data, and a `SharedKeys` pointer. A `Value` can find its `Scope` (if any) by looking up its address in that map.

Most of the time we use a subclass of `Scope`: `Doc`. This class _requires_ that the Fleece data be contained in an `alloc_slice`, so that it can be retained and released. That in turn allows Values to be retained and released.

(In fact, `Scope` itself isn't even exposed in the public API, because in general Fleece wants Values to be retain-able. If you manage the memory yourself and Fleece just has a `slice`, any calls to retain or release a Value will trigger an exception.)


## 3. Mutable Collections

Mutable values allow you to use Fleece as a general purpose collection API. You can read in compact immutable data, make changes to individual objects, and then write the result back out as data. Or you can start by creating a mutable data structure and then serialize it.

Under the hood, mutable collections are very different from immutable ones; they're actually implemented using C++ `vector` and `map` objects. But the API hides this from you.

The key to this is that a mutable `Value` has an odd address. Normally Fleece data must be loaded two-byte aligned, and values always have even length, so a normal `Value` always has an even address.

In fact, a mutable `Value` points to an odd-numbered byte inside a heap-allocated C++ object, a `HeapValue`, of which the common subclasses are `HeapArray` or `HeapDict`. Here's an example of a mutable string:

```
    HeapValue--> 01 00 00 00        reference count (inherited from RefCounted)
                 FF                 padding byte, ensuring the Value gets an odd address
    Value -----> 43                 first byte of Value
                 'f' 'o' 'o'        subsequent bytes of Value
```

If the address of the HeapValue is 0x60004B00, then the `Value` pointer seen by API clients is 0x60004B05.

In the case of `HeapArray` and `HeapDict`, only the first byte of the Value is present; after that comes internal data such as the `std::vector` or `std::map` object. This means the regular Fleece array/dict code won't work on such values; the first byte allows Fleece to tell that this is an array or dict, but no more. So those API calls, like `Array::length`, first check whether the Value is mutable (by checking the low bit of the address.) If so, they cast the pointer to the appropriate `HeapValue` subclass (subtracting 5 from the address) and call the corresponding method on that class.

### Mutable Copies

It's very common to make a mutable copy of an (immutable) Value, then start modifying it. To make this more efficient, a mutable collection created this way acts as a delta. It keeps a pointer to the base Value, and only records the items that are changed. (This is sort of like Dict inheritance in encoded data.)

A mutable copy of an Array creates a C++ `std::vector` the size of the array, but leaves its values empty. When it looks up a value in the vector and finds it's empty, it refers to the original Array and gets the value from there. When it stores a value, that replaces the empty item in the vector.

A mutable copy of a Dict begins with an empty `std::map`. When it look up a key and can't find it in the map, it refers to the original Dict. Changed values get written to the map. (Deleting a key also writes to the map but stores an empty value as a marker.)

Those `vector` and `map` objects themelves store items of class `ValueSlot`, which is a holder for a Fleece value. `ValueSlot` is 8 bytes in size. It can hold either an inline Value, or a pointer to a Value. The inline Value can be up to 7 bytes long, so it suffices for nulls, booleans, small integers and very short strings; larger scalars are allocated on the heap as `HeapValue` and pointed to by the slot.
