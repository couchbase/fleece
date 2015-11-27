//
//  Encoder.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Encoder.hh"
#include "Endian.hh"
#include "varint.hh"
#include <algorithm>
#include <assert.h>


namespace fleece {

    typedef uint8_t byte;

    using namespace internal;

    encoder::encoder(Writer &out)
    :_out(out),
     _stackSize(0),
     _uniqueStrings(true),
     _writingKey(false),
     _blockedOnKey(false)
    {
        _strings.reserve(100);
        push(kSpecialTag, 1);                   // Top-level 'array' is just a single item
#ifndef NDEBUG
        _numNarrow = _numWide = _narrowCount = _wideCount = _numSavedStrings = 0;
#endif
    }

    encoder::~encoder() {
        end();
    }

    void encoder::end() {
        if (!_items)
            return;
        if (_stackSize > 1)
            throw "unclosed array/dict";
        if (_items->size() > 1)
            throw "top level must have only one value";

        if (_items->size() > 0) {
            fixPointers(_items);
            value &root = (*_items)[0];
            if (_items->wide) {
                _out.write(&root, kWide);
                // Top level value is 4 bytes, so append a 2-byte pointer to it, because the trailer
                // needs to be a 2-byte value:
                value ptr(4, kNarrow);
                _out.write(&ptr, kNarrow);
            } else {
                _out.write(&root, kNarrow);
            }
            _items->clear();
        }
        _items = NULL;
        _stackSize = 0;
    }

    // Returns position in the stream of the next write. Pads stream to even pos if necessary.
    size_t encoder::nextWritePos() {
        size_t pos = _out.length();
        if (pos & 1) {
            byte zero = 0;
            _out.write(&zero, 1);
            pos++;
        }
        return pos;
    }

    // Check whether any pointers in _items can't fit in a narrow value:
    void encoder::checkPointerWidths(valueArray *items) {
        if (!items->wide) {
            size_t base = nextWritePos();
            for (auto v = items->begin(); v != items->end(); ++v) {
                if (v->isPointer()) {
                    size_t pos = v->pointerValue<true>();
                    pos = base - pos;
                    if (pos >= 0x8000) {
                        items->wide = true;
                        break;
                    }
                }
                base += kNarrow;
            }
        }
    }

    void encoder::fixPointers(valueArray *items) {
        // Convert absolute offsets to relative:
        size_t base = nextWritePos();
        int width = items->wide ? kWide : kNarrow;
        for (auto v = items->begin(); v != items->end(); ++v) {
            if (v->isPointer()) {
                size_t pos = v->pointerValue<true>();
                assert(pos < base);
                pos = base - pos;
                *v = value(pos, width);
            }
            base += width;
        }
    }

    void encoder::reset() {
        end();
        _out = Writer();
        _stackSize = 0;
        push(kSpecialTag, 1);
        _strings.erase(_strings.begin(), _strings.end());
        _writingKey = _blockedOnKey = false;
    }


#pragma mark - WRITING:

    void encoder::addItem(value v) {
        if (_blockedOnKey)
            throw "need a key before this value";
        if (_writingKey) {
            _writingKey = false;
        } else {
            if (_items->tag == kDictTag)
                _blockedOnKey = _writingKey = true;
        }

        _items->push_back(v);
    }

    void encoder::writeValue(tags tag, byte buf[], size_t size, bool canInline) {
        buf[0] |= tag << 4;
        if (canInline && size <= 4) {
            if (size < 4)
                memset(&buf[size], 0, 4-size); // zero unused bytes
            addItem(*(value*)buf);
            if (size > 2)
                _items->wide = true;
        } else {
            writePointer(nextWritePos());
            _out.write(buf, size);
        }
    }

    void encoder::writePointer(size_t p)   {addItem(value(p, kWide));}


#pragma mark - SCALARS:

    void encoder::writeNull()              {addItem(value(kSpecialTag, kSpecialValueNull));}
    void encoder::writeBool(bool b)        {addItem(value(kSpecialTag, b ? kSpecialValueTrue
                                                                       : kSpecialValueFalse));}
    void encoder::writeInt(uint64_t i, bool isSmall, bool isUnsigned) {
        if (isSmall) {
            addItem(value(kShortIntTag, (i >> 8) & 0x0F, i & 0xFF));
        } else {
            byte buf[10];
            size_t size = PutIntOfLength(&buf[1], i, isUnsigned);
            buf[0] = (byte)size - 1;
            if (isUnsigned)
                buf[0] |= 0x08;
            ++size;
            if (size & 1)
                buf[size++] = 0;  // pad to even size
            writeValue(kIntTag, buf, size);
        }
    }

    void encoder::writeInt(int64_t i)   {writeInt(i, (i < 2048 && i >= -2048), false);}
    void encoder::writeUInt(uint64_t i) {writeInt(i, (i < 2048),               true);}

    void encoder::writeDouble(double n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int64_t)n) {
            return writeInt((int64_t)n);
        } else {
            littleEndianDouble swapped = n;
            uint8_t buf[2 + sizeof(swapped)];
            buf[0] = 0x08; // 'double' size flag
            buf[1] = 0;
            memcpy(&buf[2], &swapped, sizeof(swapped));
            writeValue(kFloatTag, buf, sizeof(buf));
        }
    }

    void encoder::writeFloat(float n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int32_t)n) {
            writeInt((int32_t)n);
        } else {
            littleEndianFloat swapped = n;
            uint8_t buf[2 + sizeof(swapped)];
            buf[0] = 0x00; // 'float' size flag
            buf[1] = 0;
            memcpy(&buf[2], &swapped, sizeof(swapped));
            writeValue(kFloatTag, buf, sizeof(buf));
        }
    }


#pragma mark - STRINGS / DATA:

    // used for strings and binary data
    slice encoder::writeData(tags tag, slice s) {
        uint8_t buf[4 + kMaxVarintLen64];
        buf[0] = (uint8_t)std::min(s.size, (size_t)0xF);
        const void *dst;
        if (s.size < kWide) {
            // Tiny data fits inline:
            memcpy(&buf[1], s.buf, s.size);
            writeValue(tag, buf, 1 + s.size);
            dst = NULL;     // ?????
        } else {
            // Large data doesn't:
            size_t bufLen = 1;
            if (s.size >= 0x0F)
                bufLen += PutUVarInt(&buf[1], s.size);
            if (s.size == 0)
                buf[bufLen++] = 0;
            writeValue(tag, buf, bufLen, false);       // write header/count
            dst = _out.write(s.buf, s.size);
        }
        return slice(dst, s.size);
    }

    void encoder::writeString(slice s) {
        // Check whether this string's already been written:
        if (__builtin_expect(_uniqueStrings && s.size >= kWide && s.size <= kMaxSharedStringSize,
                             true)) {
            auto entry = _strings.find(s);
            if (entry != _strings.end()) {
                writePointer(entry->second);
#ifndef NDEBUG
                _numSavedStrings++;
#endif
            } else {
                size_t offset = nextWritePos();
                s = writeData(kStringTag, s);
                if (s.buf) {
                    //fprintf(stderr, "Caching `%.*s` --> %zu\n", (int)s.size, s.buf, offset);
                    _strings.insert({s, offset});
                }
            }
        } else {
            writeData(kStringTag, s);
        }
    }

    void encoder::writeString(std::string s) {
        writeString(slice(s));
    }

    void encoder::writeData(slice s) {
        writeData(kBinaryTag, s);
    }


#pragma mark - ARRAYS / DICTIONARIES:

    void encoder::push(tags tag, size_t reserve) {
        if (_stackSize >= _stack.size())
            _stack.emplace_back();
        _items = &_stack[_stackSize++];
        _items->reset(tag);
        if (reserve > 0)
            _items->reserve(reserve);
    }

    void encoder::beginArray(size_t reserve) {
        push(kArrayTag, reserve);
    }

    void encoder::beginDictionary(size_t reserve) {
        push(kDictTag, 2*reserve);
        _writingKey = _blockedOnKey = true;
    }

    void encoder::endArray() {
        endCollection(internal::kArrayTag);
    }

    void encoder::endDictionary() {
        if (!_writingKey)
            throw "need a value";
        endCollection(internal::kDictTag);
    }

    void encoder::endCollection(tags tag) {
        if (_items->tag != tag)
            throw "ending wrong type of collection";

        // Pop _items off the stack:
        valueArray *items = _items;
        --_stackSize;
        _items = &_stack[_stackSize - 1];
        _writingKey = _blockedOnKey = false;

        checkPointerWidths(items);

        auto count = (uint32_t)items->size();    // includes keys if this is a dict!
        if (items->tag == kDictTag)
            count /= 2;

        // Write the array header to the outer value:
        uint8_t buf[2 + kMaxVarintLen32];
        uint32_t inlineCount = std::min(count, (uint32_t)0x07FF);
        buf[0] = (uint8_t)(inlineCount >> 8);
        buf[1] = (uint8_t)(inlineCount & 0xFF);
        size_t bufLen = 2;
        if (count >= 0x0FFF) {
            bufLen += PutUVarInt(&buf[2], count);
            if (bufLen & 1)
                buf[bufLen++] = 0;
        }
        if (items->wide)
            buf[0] |= 0x08;     // "wide" flag
        writeValue(items->tag, buf, bufLen, (count==0));          // can inline only if empty

        fixPointers(items);

        // Write the values:
        if (count > 0) {
            auto nValues = items->size();
            if (items->wide) {
                _out.write(&(*items)[0], kWide*nValues);
            } else {
                uint16_t narrow[nValues];
                int i = 0;
                for (auto v = items->begin(); v != items->end(); ++v, ++i) {
                    ::memcpy(&narrow[i], &*v, kNarrow);
                }
                _out.write(narrow, kNarrow*nValues);
            }
        }

#ifndef NDEBUG
        if (items->wide) {
            _numWide++;
            _wideCount += count;
        } else {
            _numNarrow++;
            _narrowCount += count;
        }
#endif

        items->clear();
    }

    void encoder::writeKey(std::string s)   {writeKey(slice(s));}

    void encoder::writeKey(slice s) {
        if (!_blockedOnKey) {
            if (_items->tag == kDictTag)
                throw "need a value after a key";
            else
                throw "not writing a dictionary";
        }
        _blockedOnKey = false;
        writeString(s);
    }

}
