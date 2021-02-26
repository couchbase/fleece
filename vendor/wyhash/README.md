# wyhash

`wyhash32.h` is copied from [the wyhash GitHub repo][1], commit  091ad2a from 2020-04-25.

The only local change is the addition of a cast to `unsigned` in line 21, to avoid a compiler warning.

## What is it?

From the wyhash [README][2]:

>wyhash and wyrand are the ideal 64-bit hash function and PRNG respectively:

>solid: wyhash passed SMHasher, wyrand passed BigCrush, practrand.

>portable: 64-bit/32-bit system, big/little endian.

>fastest: Efficient on 64-bit machines, especially for short keys.

>simplest: In the sense of code size.

>wyhash is the default hasher for a hash table of the great Zig, V and Nim language.

... also used by [Abseil][3]'s [hashmap][4].

[1]: https://github.com/wangyi-fudan/wyhash/blob/master/wyhash32.h
[2]: https://github.com/wangyi-fudan/wyhash
[3]: https://github.com/abseil/abseil-cpp
[4]: https://github.com/abseil/abseil-cpp/blob/master/absl/hash/internal/wyhash.cc
