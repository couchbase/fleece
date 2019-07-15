//
// KeyTree.cc
//
// Copyright (c) 2016 Couchbase, Inc All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "KeyTree.hh"
#include "varint.hh"
#include <math.h>
#include <iostream>
#include <algorithm>
#include <string>
#include "PlatformCompat.hh"
#include "betterassert.hh"

#if (defined __ANDROID__) && (__ANDROID_API__ < 18)
#define log2(x) (log(x) / log(2))
#endif
namespace fleece {
    using namespace std;


    // Data format of a tree is:
    // depth                    1 byte
    // [root node]
    //
    // Data format of a tree node is:
    // string length            varint
    // string                   variable
    // offset to right subtree  varint
    // [left subtree]
    // [right subtree]
    //
    // Offset to right subtree is 0 if there is no right subtree,
    // and the field is entirely missing in the bottom nodes (which have no subtrees.)


#pragma mark - WRITING:

    class keyTreeWriter {
    public:
        keyTreeWriter(const std::vector<slice> &strings)
        :_strings(strings)
        ,_sizes(strings.size())
        ,_childCommonPrefixes(strings.size())
        { }

        alloc_slice writeTree() {
            auto n = _strings.size();
            size_t totalSize = 1 + sizeKeyTree(0, n, 0);
            alloc_slice output(totalSize);
            _out = (uint8_t*)output.buf;

            writeByte((uint8_t)ceil(log2(n)));   // Write the depth first
            writeKeyTree(0, n, 0);
            assert(_out == output.end());
            return output;
        }

    private:
        const std::vector<slice> &_strings;
        std::vector<size_t> _sizes;
        std::vector<uint8_t> _childCommonPrefixes;
        uint8_t* _out;

        // Same logic as writeKeyTree, but just returns the size it would write, without writing.
        size_t sizeKeyTree(size_t begin, size_t end, uint8_t prefixLen) {
            size_t mid = (begin + end) / 2;
            slice str = _strings[mid];
            size_t strSize = str.size - prefixLen;
            size_t size = SizeOfVarInt(strSize) + strSize;          // middle string, w/len prefix

            if (end - begin > 1) {
                uint8_t childPrefixLen = commonPrefixLength(_strings[begin], _strings[end-1]);
                _childCommonPrefixes[mid] = childPrefixLen;
                size += 1;                                          // subtree prefix length
                size_t leftSize = sizeKeyTree(begin, mid, childPrefixLen);
                if (mid+1 < end) {
                    size += SizeOfVarInt(leftSize);                 // right subtree offset
                    size += leftSize;                               // left subtree
                    size += sizeKeyTree(mid+1, end, childPrefixLen);// right subtree
                } else {
                    size += 1;                                      // no right subtree (offset 0)
                    size += leftSize;                               // left subtree
                }
            }
            _sizes[mid] = size;
            return size;
        }

        void writeKeyTree(size_t begin, size_t end, size_t prefixLen) {
            size_t mid = (begin + end) / 2;
            // Write middle string, with length prefix:
            slice str = _strings[mid];
            str.moveStart(prefixLen);
            writeVarInt(str.size);
            write(str);                                         // Write key, minus common prefix

            if (end - begin > 1) {
                auto childPrefixLen = _childCommonPrefixes[mid];
                assert(childPrefixLen >= prefixLen);
                writeByte(uint8_t(childPrefixLen - prefixLen)); // Write subtree prefix len
                if (mid+1 < end) {
                    size_t leftSize = _sizes[(begin + mid) / 2];
                    writeVarInt(leftSize);                      // Write right subtree offset
                    writeKeyTree(begin, mid, childPrefixLen);   // Write left subtree
                    writeKeyTree(mid+1, end, childPrefixLen);   // Write right subtree
                } else {
                    writeByte(0);                               // No right subtree (write a 0)
                    writeKeyTree(begin, mid, childPrefixLen);   // Write left subtree
                }
            }
        }

        // Returns the length of the common prefix of two strings (no more than 255)
        uint8_t commonPrefixLength(slice str0, slice str1) {
            auto maxLen = min(min(str0.size, str1.size), size_t(255));
            size_t len;
            for (len = 0; len < maxLen; ++len)
                if (str1[len] != str0[len])
                    break;
            return uint8_t(len);
        }

        inline void writeByte(uint8_t byte) {
            *_out++ = byte;
        }

        inline void write(slice s) {
            memcpy(_out, s.buf, s.size);
            _out += s.size;
        }

        inline size_t writeVarInt(size_t n) {
            size_t size = PutUVarInt(_out, n);
            _out += size;
            return size;
        }
    };
    

    KeyTree KeyTree::fromSortedStrings(const std::vector<slice>& strings) {
        return KeyTree(keyTreeWriter(strings).writeTree());
    }

    KeyTree KeyTree::fromStrings(std::vector<slice> strings) {
        std::sort(strings.begin(), strings.end());
        return fromSortedStrings(strings);
    }


#pragma mark - READING:

    KeyTree::KeyTree(const void *encodedDataStart)
    :_data(encodedDataStart)
    { }

    KeyTree::KeyTree(alloc_slice encoded)
    :_ownedData(encoded),
    _data(encoded.buf)
    { }
    
    static int32_t readVarInt(const uint8_t* &tree) {
        slice buf(tree, kMaxVarintLen32);
        uint32_t n;
        if (!ReadUVarInt32(&buf, &n))
            return -1;
        tree = (const uint8_t*)buf.buf;
        return n;
    }

    static slice readKey(const uint8_t* &tree) {
        slice key;
        int32_t len = readVarInt(tree);
        if (len >= 0) {
            key = slice(tree, len);
            tree += len;
        }
        return key;
    }


    unsigned KeyTree::operator[] (slice str) const {
        const uint8_t* tree = (const uint8_t*)_data;
        unsigned id = 0;
        unsigned mask = 1;
        for (unsigned depth = *tree++; depth > 0; --depth) {
            slice key = readKey(tree);
            if (!key.buf)
                return 0; // parse error
            if (depth > 1) {
                uint8_t prefixLen = *tree++;
                if (prefixLen > 0) {
                    // Match common prefix, then strip it:
                    if (memcmp(str.buf, key.buf, prefixLen) != 0)
                        return 0;
                    str.moveStart(prefixLen);
                    key.moveStart(prefixLen);
                }
            }
            int cmp = str.compare(key);
            if (cmp == 0)
                return id | mask;                   // Found it!
            if (depth == 1)
                return 0;
            int32_t leftTreeSize = readVarInt(tree);
            if (leftTreeSize < 0)
                return 0; // parse error
            if (cmp > 0) {
                tree += leftTreeSize;               // take right branch
                id |= mask;
            }
            mask <<= 1;
        }
        return 0;
    }

    string KeyTree::operator[] (unsigned id) const {
        string result;
        if (id == 0)
            return "";
        const uint8_t* tree = (const uint8_t*)_data;
        for (unsigned depth = *tree++; depth > 0; --depth) {
            slice key = readKey(tree);
            if (!key.buf)
                return ""; // parse error
            if (id == 1)
                return result + string(key);
            if (depth == 1)
                break;
            uint8_t prefixLen = *tree++;
            if (prefixLen > 0)
                result += string((const char*)key.buf, prefixLen);
            int32_t leftTreeSize = readVarInt(tree);
            if (leftTreeSize < 0)
                return ""; // parse error
            if (id & 1) {
                if (leftTreeSize == 0)
                    return ""; // no right subtree
                tree += leftTreeSize;
            }
            id >>= 1;
        }
        return result;
    }

    void KeyTree::dump() {
        auto tree = (const uint8_t*)_data;
        auto depth = *tree++;
        dump(tree, depth);
    }

    void KeyTree::dump(const uint8_t *tree, unsigned depth, unsigned indent, unsigned prefix) {
        slice key = readKey(tree);
        unsigned childPrefix = prefix;
        int32_t leftTreeSize = 0;
        if (depth > 1) {
            childPrefix += *tree++;
            leftTreeSize = readVarInt(tree);
            dump(tree, depth-1, indent+1, childPrefix);
        }

        for (unsigned i = 0; i < indent; i++)
            std::cerr << ".   ";
        if (prefix > 0)
            std::cerr << std::string(prefix, '_');
        std::cerr << std::string(key);
        if (childPrefix > prefix)
            cerr << "(" << (childPrefix-prefix) << ")";
        std::cerr << "\n";

        if (depth > 1) {
            if (leftTreeSize > 0)
                dump(tree + leftTreeSize, depth-1, indent+1, childPrefix);
        }
    }

}
