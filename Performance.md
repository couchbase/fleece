# Fleece Performance

The Fleece test suite comes with a few simple [benchmarks](Tests/PerfTests.cc). And if run on Mac OS or iOS, there are [comparative benchmarks](Tests/ObjCTests.mm) that perform the same operations using the Foundation framework's JSON parser (`NSJSONSerialization`) and collection classes.

## What's Tested

The tests operate on a data set of 1000 fake people, that is, an array of 1000 dictionaries, each of which has the same schema consisting of a mix of primitive fields and nested objects. (Here's what one such "person" [looks like](Tests/1person.json).)

Data size
: The size of the JSON (no whitespace) and the resulting Fleece.

Convert JSON to Fleece
: The time it took to directly convert JSON to Fleece, using the `JSONConverter` class.

Parse
: Time to parse JSON to Foundation objects using `NSJSONSerialization`, and then free the objects afterwards, vs. the time to scan the Fleece data for validity and return the root object pointer.

Parse trusted
: Time to parse if the data is known to be syntactically valid. No difference for JSON, but with Fleece the scan can be skipped almost entirely.

Look up one name
: Time to retrieve the `name` property of the 123rd array element, including validation that each object (array, dictionary, string) is of the correct type, to avoid crashes.

## The Results

These come from the above test suites, run on a Macbook Pro with a 2.3 GHz Intel Core i7. (These are sizes and times, so smaller numbers are better.)


|                           | JSON      | Fleece| Gross Ratio | Net Ratio |
|---------------------------|----------:|------:|------------:|----------:|
|Data size (bytes)          |1,303,317  |911,920| 1.4x        | 1.4x      |
|Convert JSON to Fleece (µs)|--         |6,400  | -           | -         |
|Parse (µs)                 |17,000     |750    | 26.6x       | 2.37x     |
|Parse trusted (µs)         |17,000     |0.6    | 28,333.3x   | 2.66x     |
|Look up one name (ns)      |960        |49     | 19.6x       | 2.49x[1]  |

Gross Ratio
: Ratio of JSON to Fleece, excluding any cost of converting from JSON to Fleece. >1 means Fleece is better. Representative if data is converted once and then parsed / looked up a large number of times.

Net Ratio
: Ratio if JSON to Fleece, _including_ the cost of converting from JSON to Fleece _on every operation_ - i.e. "Convert JSON to Fleece" is added to all fleece operations". >1 means Fleece is better. Representative if data is converted and parsed on every operation.

[1]: "Look up on name - Net" requires some additional explanation - this compares the work requires to lookup one name JSON vs one name in Fleece, assuming in both cases one is starting with the raw data and hence it must be conveted / parsed first. Specifically: (JSON_parse + JSON_lookup_one_name) / (Convert_JSON_to_Fleece + Fleece_Parse + Fleece_lookup_one_name).

## Analysis

The Fleece data is 30% smaller than the JSON, thanks to coalescing multiple copies of the same string (every one of the 1,000 person dictionaries has the same keys!)

"Parsing" Fleece is _22 times_ faster than parsing the JSON, because it simply requires a quick in-place scan for validity, which does not allocate any memory or interact with platform libraries.

If the Fleece data is trusted to be syntactically valid (e.g. it just came out of a database record that's known to have a CRC checksum), the scan can be skipped, leading to a near-zero parse time (really just some pointer arithmetic plus decoding an integer.)

Once the data is parsed, a typical sequence of operations — array lookup plus dictionary lookup — is 20 times faster with Fleece.

It is twice as fast to convert JSON to Fleece in memory, and use the Fleece objects, than it is to parse the JSON to Foundation objects!
