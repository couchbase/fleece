# Fleece

Jens Alfke — 12 May 2015

__Fleece__ is a new binary encoding for semi-structured data. Its data model is a superset of JSON, adding support for binary values. It is designed to be:

* Very fast to read: No parsing is needed, and the data can be navigated and read without any heap allocation. Fleece objects are internal pointers into the raw data. Arrays and dictionaries can be random-accessed.
* Compact: Simple values will be about the same size as JSON. Complex ones may be much smaller, since repeated values, especially strings, only need to be stored once.
* Efficient to convert into native objects: Numbers are binary, strings are raw UTF-8 without quoting, binary data is not base64-encoded. Storing repeated values once means they only need to be converted into native objects once.

## Format

Fleece data consists of **values**. The value at the _end_ of the data is the primary top-level object, usually a dictionary. (This is an oversimplification; see Finding The Root, below.)

Values are always 2-byte aligned and occupy at least two bytes. The upper 4 bits of the first byte are a tag that identify the value's type.

Some common values fit into exactly two bytes:

* `null`, `false`, `true`
* Integers in the range -2048...2047
* Empty or one-byte strings
* Empty arrays and dictionaries

Longer strings and integers include a byte count in the 2-byte header, followed by the variable-length data.

Floating-point numbers are 6 or 10 bytes long (2-byte header followed by a float or double.)

An array consists of a two-byte header, an item count (which fits in the header if it's less than 4096), and then a contiguous sequence of values. For fast random access, each value is the same length: 2 bytes in a regular **narrow** array, 4 bytes in a **wide** array.

Dictionaries are like arrays except that each item consists of two values: a key followed by the value it maps to. The items are sorted by increasing key, to allow lookup by binary search. (For JSON compatibility the keys must be strings, but the format allows other types.)

### Pointers

How do values longer than 2 bytes fit in a collection? By using **pointers**. A pointer is a special value that represents a relative offset from itself to another value. Pointers always point back (toward lower addresses) to previously-written values.

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
 0010s--- --------...    floating point (s = 0:float, 1:double). LE float data follows.
 0011ss-- --------       special (s = 0:null, 1:false, 2:true)
 0100cccc ssssssss...    string (cccc is byte count, or if it’s 15 then count follows as varint)
 0101cccc dddddddd...    binary data (same as string)
 0110wccc cccccccc...    array (c = 11-bit item count, if 2047 then overflow follows as varint;
                                w = wide, if 1 then following values are 4 bytes wide, not 2)
 0111wccc cccccccc...    dictionary (same as array, but each item is a key (string) followed by
                                     a value.)
 1ooooooo oooooooo       pointer (o = BE unsigned offset in units of 2 bytes _backwards_, 0-64kb)
                                NOTE: In a wide collection, offset field is 31 bits wide
```
Bits marked “-“ are reserved and should be set to zero.

I’ve tried to make the encoding little-endian-friendly since nearly all mainstream CPUs are little-endian. But in the case of bitfields that span parts of multiple bytes — in small integers, array counts, and pointers — a little-endian ordering would make the bitfield disconnected, thus harder to decode, so I’ve made those big-endian. (I’m open to better ideas, though.)

## Example

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

## API

### Reading

All Fleece values are self-contained: interpreting a value doesn't require accessing any external state. A value is also fast enough to parse that it's not necessary to keep a separate parsed form in memory. Because of this, the API can expose Fleece values as direct pointers into the document. These are opaque types, of course, but in C++ they are full-fledged objects of class Value (with subclasses Array and Dictionary). Working with Fleece data feels just like working with a (read-only) native object graph.

Collections provide iterators for sequential access, and getters for random access. Array random access is O(1) since the stored format is simply a native array of 16-bit values. Dictionary random access is O(log _n_), as it involves a binary search for the key.

### Writing

Fleece documents are created using an Encoder object that produces a data blob when it's done. Values are added one by one. A collection is added by making a `begin` call, then adding the values, then calling `end`. (A dictionary encoder requires a key to be written before each value.)

Behind the scenes, the data is written in bottom-up order: the values in a collection are written before the collection itself. The inline data in the collection is buffered while out-of-line values are written, then the collection object is written at the end.
