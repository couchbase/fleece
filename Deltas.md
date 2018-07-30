#  Delta Compression

The Fleece API includes functions that behave like the Unix tools `diff` and `patch`: they find the differences between two Fleece values, encode them in a compact format called a "delta", and can reconstitute the second value given the first and the delta. This provides an efficient way to store the change history of a document, or to transmit a change over a network.

In the C API (Fleece.h), the functions are `FLCreateDelta`, `FLEncodeDelta`, `FLApplyDelta`, and `FLEncodeApplyingDelta`. In the public C++ API (FleeceCpp.hh) they are methods of the `Delta` class. See the headers for documentation.

## Delta Format

Deltas are intended as opaque values to be passed to the Apply... functions. But for debugging purposes, and to aid in the creation of compatible implementations, here's a description of their internal format.

This format is based on Benjamín Eidelman's [JsonDiffPatch](https://github.com/benjamine/jsondiffpatch/blob/master/docs/deltas.md), but has been modified to be more compact. (JsonDiffPatch's patches carry a lot of information about the old value, to enable fuzzy patching when the base isn't exactly the same as the value the patch was created from, but for our purposes we don't need this.) 

A delta of two Fleece values is expressed as a JSON value, of the form:

* `newValue` — The value is completely replaced with *newValue*.
* `[ newValue ]` — The value is completely replaced with *newValue*. (This form is used for disambiguation when *newValue* is an array or object.)
* `[ ]` — The value is deleted.
* `{ "k1": v1, ... }` — Incremental update of an object. Each value `v`*n* is (recursively) a delta to apply to the old value at the corresponding key `k`*n*. (If a key didn't appear in the old object, the delta represents an insertion.)
* `["patch", 0, 2]` — Incremental update of a string. The `patch` string is a series of operations,  which describe what to do with consecutive ranges of the original UTF-8 string to transform it into the new one. The total byte count of all the operations must equal the length of the original string. There are three operations, each of which starts with a decimal whole number *n*:
    * `n=` — The next *n* bytes are left alone (i.e. copied to the new string.)
    * `n-` — The next n bytes are deleted (skipped)
    * `n+newbytes|` — The *n* bytes following the `+` (the *newbytes*) are inserted into the new string. The `|` marker is not a delimiter; it's just there to make the patch more readable, and to act as a safety check while processing the patch.
    
### Examples

```
old:   {"age": 8, "grade": 3, "name": {"first": "Bobby", "last": "Briggs"}}
new:   {"age": 18, "name": {"first": "Robert", "last": "Briggs"}}
delta: {"age": 18, "grade": [], "name": {"first": "Robert"}}

old:   {"age": 18, "name": {"first": "Robert", "last": "Briggs"}}
new:   {"age": 38, "name": {"title": "Col.", "first": "Robert", "last": "Briggs"}}
delta: {"age": 38, "name": {"title": "Maj."}}

old:   "The fog comes in on little cat feet"
new:   "The dog comes in on little cat feet"
delta: ["4=1-1+d|31=",0,2]

old:   "to wound the autumnal city. So howled out for the world to give him a name.  The in-dark answered with the wind."
new:   "To wound the eternal city. So he howled out for the world to give him its name. The in-dark answered with wind."
delta: ["1-1+T|12=5-4+eter|13=3+he |37=1-3+its|6=1-27=4-5=",0,2]
```

## Limitations

This implementation does not yet efficiently encode incremental changes to arrays. I will try to address this in the future. Computing the changes from one array to another is a complex, ambiguous, and potentially expensive task...
