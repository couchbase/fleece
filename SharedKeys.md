# Fleece Shared Keys

**Shared keys** are an optimization for the common use case where multiple separate Fleece documents contain many of the same dictionary key strings. (For example, a Couchbase Lite database.) In practice, Fleece/JSON based data stores or APIs adopt informal schema based on dictionary keys. When these key strings are repeated thousands or millions of times, they take up an appreciable amount of space; moreover, looking up these keys requires a lot of `memcmp` calls.

The basic idea of shared keys is simple:
* Maintain an _external_, shared bidirectional mapping from key strings to small integers.
* In encoded Fleece Dict data, replace strings in key-lists with their matching integers.

This works because the Fleece storage format allows Dict keys to be arbitrary Values, even though at the API level they're always strings. When shared keys are in use, keys may be stored as small integers in the range 0...2047, which fit inline in the Dict structure.

It's efficient because the integers are obviously smaller than the strings (although how much depends on the strings and how often they'd be reused within the same document.) More importantly, the integers are a lot faster to look up, since the comparison is just a CPU `cmp` instruction rather than a `memcmp` call.

This mechanism is mostly transparent: the Fleece implementation takes care of the mapping for you. When you call `FLDict_GetKey`, Fleece first looks up the key in the mapping; if it's present, it looks up the integer instead. When you iterate a dictionary, the reverse happens: if a key is an integer, Fleece looks it up in the mapping and returns the string. When you encode a dictionary, your key strings are converted to integers when possible, which might mean registering a new key in the mapping.

## Usage

### API

The opaque type `FLSharedKeys` represents a reference to a shared-keys object.

* `FLSharedKeys`:
   - `FLSharedKeys_Create` creates a new empty instance with a ref-count of one.
   - `FLSharedKeys_Retain` increments the ref-count.
   - `FLSharedKeys_Release` releases the ref-count, freeing the object when it reaches zero.
   - `FLSharedKeys_CreateFromStateData` reconstitutes an instance from saved state data.
   - `FLSharedKeys_GetStateData` returns the (opaque) data needed to persist an instance.
   - `FLSharedKeys_Count` returns the number of keys stored. (As there is no way to remove keys, this number only ever increases.)
   - `FLSharedKeys_Encode` maps a string to an integer, optionally adding a new mapping for it if possible.
   - `FLSharedKeys_Decode` maps an integer back to a string.
* `FLDoc`:
   - `FLDoc_FromResultData` takes an `FLSharedKeys` parameter that associates those keys with the document.
   - `FLDoc_GetSharedKeys` returns the shared keys associated with a document.
* `FLEncoder`:
   - `FLEncoder_SetSharedKeys` tells an encoder to use shared keys when writing Dict keys.

### Workflow

Using shared keys requires the following workflow:

1. Before working with any Fleece documents:
   * Read the encoded shared key data from persistent storage and decode it into an `FLSharedKeys` instance.
   * Or if there's none stored, create a new empty `FLSharedKeys` instance.
2. To read a document:
   * Use `FLDoc_FromResultData` and pass the shared-keys reference as a parameter. (You can't use `FLValue_FromData` because it doesn't support shared keys.)
3. When saving a document:
   * Call `FLEncoder_SetSharedKeys` right after creating the `Encoder`.
   * If new keys were added, save the `FLSharedKeys`' state data. (You can tell keys were added because its `count` increases.)
   * Then save the encoded document data

It's important to persist the shared-keys changes before (or atomically with) the new document data. If you wait till after writing a document, a crash might leave you with persisted documents that have encoded keys with no way to decode them, causing data corruption.

### Transactions and Concurrency

Things get more complex if your storage layer allows multiple independent writers (for example, a database with ACID properties.) The danger is that two writers, with separate `FLSharedKeys` instances, might separately encode documents that each add a different shared key. Both keys would be assigned the same number. At some point the writers will detect the conflict, probably when the second one tries to persist its changes and finds the first one beat it, and now they have a problem: the shared keys have to be renumbered, both in the `FLSharedKeys` and in any documents that use the newly-added key(s). Yuck.

To prevent this, it's recommended that if your storage system supports transactions, you _only encode documents inside a transaction_. This looks like:

1. Make changes to documents without re-encoding; either using the mutable-Fleece API or by mutating your own data model objects.
2. When ready to save, begin a transaction in your storage layer.
3. Re-read the shared keys from storage, if they've changed (another writer may have added keys since you last checked.)
4. Encode your changed documents and write them to storage
5. If the shared keys changed since step 3, write them to storage.
6. Commit the transaction
7. If the commit failed, or you had to abort, discard the changes to the shared keys by re-reading them from storage as in step 3.

An additional side effect of concurrency is that you might, at any time, read a document from storage that uses a new shared key created by another writer. The conservative solution is to check for updates to the persisted shared keys before every read (you can amortize this by checking once at the start of a read-only transaction, if available.) The _efficient_ way is to have the `FLSharedKeys` proactively refresh itself from storage any time it encounters a key number it doesn't recognize; this is what Couchbase Lite Core does, but unfortunately there is not yet any public API for this. The daring may inspect the `fleece::impl::PersistentSharedKeys` class.

## Some Gnarly Details

As a reminder, only 2048 keys can be shared. (That's the maximum inline short integer.)

### Key criteria

Not all key strings are encoded to integers, only ones that
* are 16 bytes or less
* consist only of ASCII alphanumeric characters, "`_`" or "`-`".

Why? It's an attempt to weed out keys that are document-specific, not part of a schema. For example, in the older Couchbase Mobile schema, a document represents a set of attached files as a dictionary whose keys are the filenames. In such a database, the shared-key space would fill up with filenames that are probably document-specific, which is a waste.

You might want to use different criteria, but unfortunately they can't be changed without breaking compatibility. More specifically, _the writer and reader of a document have to agree on which keys can be encoded_, otherwise key lookup breaks. Why? Imagine I decide to allow "`.`" in encoded keys. I encode `{"Mr.Rogers":1928}`. The string is added to the shared keys as 19, and the Dict is written with 19 as the key. Fine. The problem comes when you try to look up the key `"Mr.Rogers"`. Your copy of Fleece looks at the key and decides it _can't_ be encoded, so it looks up the literal string in the Dict. It doesn't find it, so it returns `NULL`. Oops.

(This could be worked around by having `FLDict_Get` look up all keys in the shared-keys table, whether or not it think they're eligible, but that would slow it down. Plus, making that change now wouldn't help all pre-existing Fleece implementations out in the world.)

### Internal concurrency

`FLSharedKeys` is thread-safe. Internally it stores a hash table mapping strings to integers, and for the reverse mapping a fixed-size array of 2048 string pointers. The reverse mapping is thread-safe as long as the load/stores of the pointers are atomic, which they are in modern CPU architectures. The hash table has to be protected with a mutex. This slows down string-based key lookups slightly (another good reason to use `FLDictKey`!) Internally, `FLEncoder` grabs the mutex once at the start of encoding and releases it at the end, which boosts its speed significantly.

### Pre-encoding key for lookup

It's faster to look up a key in a Dict if you provide its encoded integer. This saves a character-set check and table lookup. Fleece provides an optimization to help you do this: `FLDictKey`. This is a small struct that's initialized with a string and can be used in place of that string to look up a key in a Dict. Internally, if/when a shared key encoding is found, that integer is stored into the `FLDictKey` so it can be used directly on subsequent lookups, making them faster.

`FLKeyPath` uses `FLDictKey` internally to speed it up.

### Looking up a Dict's SharedKeys

The weirdest part of the whole scheme is how `FLDict_Get` finds the associated `FLSharedKeys`, when it needs to look up the integer encoding of a key. Originally, the keys had to be passed as an optional parameter to every call. This turned out to be really messy, because anything that might call `FLDict_Get` also had to take the keys as a parameter, and so on up the call chain. It created a lot of API complexity in Couchbase Lite and made Fleece harder to work with and understand.

Nowdays, Fleece keeps a single global mapping from memory address ranges to `FLSharedKeys` instances. `FLDoc_FromResultData` adds an entry to the map, associating the encoded Fleece data in memory with the shared keys it's given. The entry is removed when the `FLDoc` is freed. `FLDict_Get` looks up the dict's address in the map and gets the shared keys (if any.)

So far this lookup process has not turned out to be a major hot-spot in profiling, partly because the really intensive usages of key lookup (like Couchbase Lite's query and indexing) use pre-encoded keys thanks to `FLKeyPath`.
