# Fleece Storage

**Jens Alfke -- created 9 May 2018**

Recent changes to Fleece make it useable as a lightweight document database -- a persistent key-value store where the values are JSON-equivalent documents. While it lacks important features like queries, and its performance isn't on par with Couchbase Lite, its small code size and memory usage should make it useful for embedded systems.

There are four major parts to Fleece Storage:

1. **Mutable collections** -- `MutableArray` and `MutableDict` objects make it possible to construct new documents and modify existing ones. They're subclasses of `Array` and `Dict`, so everything works as you'd expect.
2. **Incremental updates** -- A mutable collection can be encoded as a delta to be appended to its previous encoding. This provides an efficient append-only persistent representation. In the future it will also allow deltas to be replicated easily, by just sending all the data starting from the last EOF known to the peer.
3. **Hash Trees** -- The Dict structure isn't very scalable as the top level of a key-value store, which might have thousands of keys. A new `HashTree` structure, based on a [Hash Array Mapped Trie][HAMT], provides very fast access to any number of keys (no more than six pointer indirections).
4. The **`DB` class** uses the above features, with persistent Fleece data stored on disk in an append-only file. The file is memory-mapped so pages are read only as needed. (In an embedded system, it could instead be stored in contiguous blocks of RAM or raw flash storage.)

## Mutable Collections

`MutableArray` is a subclass of `Array`. (In the C API, `FLMutableArray` is type-compatible with `FLArray`.) You can create an empty instance from scratch, or create a mutable copy of an existing Array. It offers the expected operations, like setting values at integer indexes, and inserting or removing elements.

`MutableDict` is the same, a subclass of `Dict`.

Each also offers `getMutableArray()` and `getMutableDict()` methods, which promote an existing `Array` or `Dict` element to a mutable copy and store it back into the collection. This makes it easy to modify nested values.

One thing you need to watch out for: Since the mutable classes are heap-allocated, memory management is an issue. These classes are ref-counted, using the same `RefCounted` utility class that LiteCore does. The `Retained<>` template is a smart pointer that takes care of retaining and releasing the values it holds. The factory methods return `Retained` since the objects they return have no references to them yet; you should assign their return values to `Retained` objects. In many cases, though, you don't need to do extra work because the accessor methods return objects that are already being referenced by their parent collection, so if you only use them briefly without modifying the parent, there's no need to retain/release them.

Here's an example of reading a Fleece dictionary, and modifying it in memory:

```c++
    const Dict* originalDict = Value::fromData(originalData)->asDict();
    Retained<MutableDict> newDict = MutableDict::newDict(originalDict);
    newDict->set("something"_sl, "newValue"_sl);
    newDict->remove("obsolete"_sl);
    Retained<MutableArray> items = newDict->getMutableArray("items");
    items->append(175);
    items->append("foo");     
```

## Incremental Updates

If you create a mutable copy of an encoded Fleece document's root-level collection, the `Encoder` class can encode that modified object in a space-efficient delta form that contains only the modified values (plus some bookkeeping overhead.) You just need to call `Encoder::setBase()` with the slice containing the original document. The Encoder's output will then be designed to be appended to that original data; the concatenation will be readable as a regular Fleece document whose value is the current value of the root container.

Here's an example, that continues from the above example by encoding the modified data:

```c++
        Encoder enc;
        enc.setBase(originalData);
        enc.reuseBaseStrings();
        enc.writeValue(update);
        alloc_slice deltaData = enc2.extractOutput();
```

`deltaData` will be pretty small, a few hundred bytes, no matter how large `originalData` is. (Of course, to read it, it has to first be appended to `originalData`.)

Since it makes the most sense to append deltas to a file, the Encoder class now has the option to write directly to a `FILE*` handle instead of buffering its output in memory.

## Hash Trees

A [Hash Array Mapped Trie][HAMT] is a multi-level hash table. The key's hash code is treated as a bit-string and broken into pieces; Fleece uses a 32-bit hash and five-bit pieces. The pieces are then used as successive indexes in a trie -- each node of the trie has 32-way branching since there are 32 combinations of 5 bits. The leaf node at the end of the path stores the key and its value. This sounds really space-inefficient, but there are some clever tricks to store the trie nodes very compactly.

This structure lends itself to append-only updating, because any change only affects a small number of trie nodes. The modified nodes are then appended to the file, pointing back to their unchanged children, with the new root node at the end of the file. (This is the same way Couchbase Server and CouchDB's storage works, except they're using B-trees instead of HAMTs.)

`HashTree` represents such a tree, whose keys are slices and values are Fleece `Value` objects. It's not type-compatible with `Value`, but like it, the in-memory format is identical to the persisted format, so it requires no parsing. Once the data is read (or mmap'ed), it can be treated as a HashTree with nearly zero overhead.

`MutableHashTree` extends a HashTree, allowing you to make changes to it. You can add / update / remove keys, or modify the values in place via the same `getMutableArray()` and `getMutableDict()` methods that the mutable collections provide. The modified tree can then be encoded to an `Encoder`, either in its entirety or as a delta.

HashTree deltas aren't quite as space-efficient as ones based on Dicts, but they're more scaleable. I haven't done performance testing yet, so I don't know where the crossover is, but I imagine that Dicts will bog down with hundreds of thousands of keys, while HashTree will be just fine.

## The DB Class

`DB` is a fairly simple class on its own; what it does is bring together all of the above to create a persistent key-value store where the values are JSON-equivalent documents. It does most of what a Couchbase Lite database does ... with the notable exceptions of queries, eventing, and replication.

You construct a DB on a filesystem path. It maps the file into memory and treats it as a MutableHashTree; thus far there's nearly zero I/O nor heap allocation.

The basic get, insert, update, delete operations are similar to MutableHashTree and MutableDict, except that the values are always Dicts.

You can iterate over a DB, but the keys will not be in sorted order, since the underlying storage is a hash table not a tree.

The `commitChanges()` method saves changes to disk, by using an Encoder to write a delta to the end of the file. Or `revertChanges` will get rid of in-memory changes by replacing the MutableHashTree with a new unmodified instance.

Here's the above example, instead using a DB:

```c++
    DB mydb("mydbfile", "rw+");
    Retained<MutableDict> newDict = db.getMutable("doc");
    newDict->set("something"_sl, "newValue"_sl);
    newDict->remove("obsolete"_sl);
    Retained<MutableArray> items = newDict->getMutableArray("items");
    items->append(175);
    items->append("foo");
    mydb.commitChanges();
```

### TBD:

* In its current form, `DB` uses memory-mapped files. This is extremely efficient, but it's not available on embedded systems, since their CPUs don't have fancy MMUs. (For that matter, many of them have rudimentary OSs that don't even have filesystems!) I will be exploring ways to implement DB functionality under those constraints.
* There's no compaction yet; the file grows with every commit. It can be compacted by saving to a new file (which writes everything from scratch) and then replacing the old file with the new one; but this involves a lot of I/O and storage space. I have some ideas of how to do this incrementally and more efficiently.
* This data format doesn't take any care to minimize disk sector reads. It's not trying to align things to 4KB boundaries, and with delta encoding of the individual documents (Dicts), a single document may be spread out across multiple storage blocks. On the plus side, this makes the data a lot more compact. I think that for small embedded use cases, that's more important.


[HAMT]: https://en.wikipedia.org/wiki/Hash_array_mapped_trie
