//
//  Encoder.cc
//  Fleece
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#include "Encoder.hh"
#include "Array.hh"
#include "SharedKeys.hh"
#include "Endian.hh"
#include "varint.hh"
#include "FleeceException.hh"
#include "PlatformCompat.hh"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <stdlib.h>


namespace fleece {

    typedef uint8_t byte;

    using namespace internal;

    Encoder::Encoder(size_t reserveSize)
    :_out(reserveSize),
     _strings(10)
    {
        push(kSpecialTag, 1);                   // Top-level 'array' is just a single item
    }

    void Encoder::end() {
        if (!_items)
            return;
        throwIf(_stackDepth > 1, EncodeError, "unclosed array/dict");
        throwIf(_items->size() > 1, EncodeError, "top level must have only one value");

        if (_items->size() > 0) {
            checkPointerWidths(_items);
            fixPointers(_items);
            Value &root = (*_items)[0];
            if (_items->wide) {
                _out.write(&root, kWide);
                // Top level Value is 4 bytes, so append a 2-byte pointer to it, because the trailer
                // needs to be a 2-byte Value:
                Value ptr(4, kNarrow);
                _out.write(&ptr, kNarrow);
            } else {
                _out.write(&root, kNarrow);
            }
            _items->clear();
        }
        _items = nullptr;
        _stackDepth = 0;
    }

    alloc_slice Encoder::extractOutput() {
        end();
        return _out.extractOutput();
    }

    // Returns position in the stream of the next write. Pads stream to even pos if necessary.
    size_t Encoder::nextWritePos() {
        size_t pos = _out.length();
        if (pos & 1) {
            byte zero = 0;
            _out.write(&zero, 1);
            pos++;
        }
        return pos;
    }

    void Encoder::reset() {
        if (_items) {
            _items->clear();
            _items = nullptr;
        }
        _out.reset();
        _stackDepth = 0;
        push(kSpecialTag, 1);
        _strings.clear();
        _writingKey = _blockedOnKey = false;
    }


#pragma mark - WRITING:

    void Encoder::addItem(Value v) {
        throwIf(_blockedOnKey, EncodeError, "need a key before this value");
        if (_writingKey) {
            _writingKey = false;
        } else {
            if (_items->tag == kDictTag)
                _blockedOnKey = _writingKey = true;
        }

        _items->push_back(v);
    }

    void Encoder::writeValue(tags tag, byte buf[], size_t size, bool canInline) {
        buf[0] |= tag << 4;
        writeRawValue(slice(buf, size), canInline);
    }

    void Encoder::writeRawValue(slice rawValue, bool canInline) {
        if (canInline && rawValue.size <= 4) {
            if (rawValue.size < 4) {
                byte buf[4] = {0};      // zero the unused bytes
                memcpy(buf, rawValue.buf, rawValue.size);
                addItem(*(Value*)buf);
            } else {
                addItem(*(Value*)rawValue.buf);
            }
            if (rawValue.size > 2)
                _items->wide = true;
        } else {
            writePointer(nextWritePos());
            _out.write(rawValue.buf, rawValue.size);
        }
    }


#pragma mark - SCALARS:

    void Encoder::writeNull()              {addItem(Value(kSpecialTag, kSpecialValueNull));}
    void Encoder::writeBool(bool b)        {addItem(Value(kSpecialTag, b ? kSpecialValueTrue
                                                                         : kSpecialValueFalse));}
    void Encoder::writeInt(uint64_t i, bool isSmall, bool isUnsigned) {
        if (isSmall) {
            addItem(Value(kShortIntTag, (i >> 8) & 0x0F, i & 0xFF));
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

    void Encoder::writeInt(int64_t i)   {writeInt(i, (i < 2048 && i >= -2048), false);}
    void Encoder::writeUInt(uint64_t i) {writeInt(i, (i < 2048),               true);}

    void Encoder::writeDouble(double n) {
        throwIf(std::isnan(n), InvalidData, "Can't write NaN");
        if (n == (int64_t)n) {
            return writeInt((int64_t)n);
        } else if (n == (float)n) {
            return _writeFloat((float)n);
        } else {
            littleEndianDouble swapped = n;
            uint8_t buf[2 + sizeof(swapped)];
            buf[0] = 0x08; // 'double' size flag
            buf[1] = 0;
            memcpy(&buf[2], &swapped, sizeof(swapped));
            writeValue(kFloatTag, buf, sizeof(buf));
        }
    }

    void Encoder::writeFloat(float n) {
        throwIf(std::isnan(n), InvalidData, "Can't write NaN");
        if (n == (int32_t)n)
            writeInt((int32_t)n);
        else
            _writeFloat(n);
    }

    void Encoder::_writeFloat(float n) {
        littleEndianFloat swapped = n;
        uint8_t buf[2 + sizeof(swapped)];
        buf[0] = 0x00; // 'float' size flag
        buf[1] = 0;
        memcpy(&buf[2], &swapped, sizeof(swapped));
        writeValue(kFloatTag, buf, sizeof(buf));
    }


#pragma mark - STRINGS / DATA:

    // used for strings and binary data. Returns the location where s got written to, which
    // can be used until the enoding is over. (Unless it's inline, in which case s.buf is nullptr.)
    slice Encoder::writeData(tags tag, slice s) {
        uint8_t buf[4 + kMaxVarintLen64];
        buf[0] = (uint8_t)std::min(s.size, (size_t)0xF);
        const void *dst;
        if (s.size < kNarrow) {
            // Tiny data fits inline:
            memcpy(&buf[1], s.buf, s.size);
            writeValue(tag, buf, 1 + s.size);
            dst = nullptr;
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

    // Returns the location where s got written to, if possible, just like writeData above.
    slice Encoder::_writeString(slice s, bool asKey) {
        // Check whether this string's already been written:
        if (_usuallyTrue(_uniqueStrings && s.size >= kNarrow && s.size <= kMaxSharedStringSize)) {
            auto &entry = _strings.find(s);
            if (entry.first.buf != nullptr) {
//                fprintf(stderr, "Found `%.*s` --> %u\n", (int)s.size, s.buf, entry.second);
                writePointer(entry.second.offset);
#ifndef NDEBUG
                _numSavedStrings++;
#endif
                if (asKey)
                    entry.second.usedAsKey = true;
                return entry.first;
            } else {
                auto offset = nextWritePos();
                throwIf(offset > 1u<<31, MemoryError, "encoded data too large");
                s = writeData(kStringTag, s);
                if (s.buf) {
#if 0
                    if (_strings.count() == 0)
                        fprintf(stderr, "---- new encoder ----\n");
                    fprintf(stderr, "Caching `%.*s` --> %u\n", (int)s.size, s.buf, offset);
#endif
                    StringTable::info i = {asKey, (uint32_t)offset};
                    _strings.addAt(entry, s, i);
                }
                return s;
            }
        } else {
            return writeData(kStringTag, s);
        }
    }

    void Encoder::writeString(const std::string &s) {
        _writeString(slice(s), false);
    }

    void Encoder::writeData(slice s) {
        writeData(kBinaryTag, s);
    }


    void Encoder::writeValue(const Value *value) {
        switch (value->tag()) {
            case kShortIntTag:
            case kIntTag:
            case kFloatTag:
            case kSpecialTag:
                writeRawValue(slice(value, value->dataSize()));
                break;
            case kStringTag:
                writeString(value->asString());
                break;
            case kBinaryTag:
                writeData(value->asData());
                break;
            case kArrayTag: {
                auto iter = value->asArray()->begin();
                beginArray(iter.count());
                for (; iter; ++iter) {
                    writeValue(iter.value());
                }
                endArray();
                break;
            }
            case kDictTag: {
                auto iter = value->asDict()->begin();
                beginDictionary(iter.count());
                for (; iter; ++iter) {
                    if (iter.key()->isInteger())
                        writeKey((int)iter.key()->asInt());
                    else
                        writeKey(iter.key()->asString());
                    writeValue(iter.value());
                }
                endDictionary();
                break;
            }
            default:
                FleeceException::_throw(UnknownValue, "illegal tag in Value; corrupt data?");
        }
    }


#pragma mark - POINTERS:

    // Pointers are added here as absolute positions in the stream (and fixed up before writing)
    void Encoder::writePointer(size_t p)   {addItem(Value(p, kWide));}

    // Check whether any pointers in _items can't fit in a narrow Value:
    void Encoder::checkPointerWidths(valueArray *items) {
        if (!items->wide) {
            size_t base = nextWritePos();
            for (auto v = items->begin(); v != items->end(); ++v) {
                if (v->isPointer()) {
                    size_t pos = v->pointerValue<true>();
                    if (base - pos >= 0x10000) {
                        items->wide = true;
                        break;
                    }
                }
                base += kNarrow;
            }
        }
    }

    // Convert absolute offsets to relative in _items:
    void Encoder::fixPointers(valueArray *items) {
        size_t base = nextWritePos();
        int width = items->wide ? kWide : kNarrow;
        for (auto v = items->begin(); v != items->end(); ++v) {
            if (v->isPointer()) {
                size_t pos = v->pointerValue<true>();
                assert(pos < base);
                pos = base - pos;
                *v = Value(pos, width);
            }
            base += width;
        }
    }

#pragma mark - ARRAYS / DICTIONARIES:

    void Encoder::writeKey(const std::string &s)   {writeKey(slice(s));}

    void Encoder::writeKey(slice s) {
        int encoded;
        if (_sharedKeys && _sharedKeys->encodeAndAdd(s, encoded)) {
            writeKey(encoded);
            return;
        }
        if (_usuallyFalse(!_blockedOnKey))
            throwUnexpectedKey();
        _blockedOnKey = false;
        s = _writeString(s, true);
        if (_sortKeys)
            _items->keys.push_back(s);
    }

    void Encoder::writeKey(int n) {
        if (_usuallyFalse(!_blockedOnKey))
            throwUnexpectedKey();
        _blockedOnKey = false;
        writeInt(n);
        if (_sortKeys)
            _items->keys.push_back(nullslice);
    }

    void Encoder::throwUnexpectedKey() {
        if (_items->tag == kDictTag)
            FleeceException::_throw(EncodeError, "need a value after a key");
        else
            FleeceException::_throw(EncodeError, "not writing a dictionary");
    }

    void Encoder::push(tags tag, size_t reserve) {
        throwIf(_stackDepth >= _stack.size(), EncodeError, "nesting is too deep");
        _items = &_stack[_stackDepth++];
        _items->reset(tag);
        if (reserve > 0)
            _items->reserve(reserve);
    }

    void Encoder::beginArray(size_t reserve) {
        push(kArrayTag, reserve);
    }

    void Encoder::beginDictionary(size_t reserve) {
        push(kDictTag, 2*reserve);
        _writingKey = _blockedOnKey = true;
    }

    void Encoder::endArray() {
        endCollection(internal::kArrayTag);
    }

    void Encoder::endDictionary() {
        throwIf(!_writingKey, EncodeError, "need a value");
        endCollection(internal::kDictTag);
    }

    void Encoder::endCollection(tags tag) {
        if (_usuallyFalse(_items->tag != tag)) {
            if (_items->tag == kSpecialTag)
                FleeceException::_throw(EncodeError, "endCollection: not in a collection");
            else
                FleeceException::_throw(EncodeError, "ending wrong type of collection");
        }

        // Pop _items off the stack:
        valueArray *items = _items;
        --_stackDepth;
        _items = &_stack[_stackDepth - 1];
        _writingKey = _blockedOnKey = false;

        if (_sortKeys && tag == kDictTag)
            sortDict(*items);

        checkPointerWidths(items);

        auto count = (uint32_t)items->size();    // includes keys if this is a dict!
        if (items->tag == kDictTag)
            count /= 2;

        // Write the array header to the outer Value:
        uint8_t buf[2 + kMaxVarintLen32];
        uint32_t inlineCount = std::min(count, (uint32_t)kLongArrayCount);
        buf[0] = (uint8_t)(inlineCount >> 8);
        buf[1] = (uint8_t)(inlineCount & 0xFF);
        size_t bufLen = 2;
        if (count >= kLongArrayCount) {
            bufLen += PutUVarInt(&buf[2], count - kLongArrayCount);
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
                StackArray(narrow, uint16_t, nValues);
                size_t i = 0;
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

    // compares dictionary keys as slices. If a slice has a null `buf`, it represents an integer
    // key, whose value is in the `size` field.
    static int compareKeysByIndex(const void *a, const void *b) {
        const slice &sa = **(const slice**)a;
        const slice &sb = **(const slice**)b;
        if (sa.buf) {
            if (sb.buf)
                return sa.compare(sb);                  // string key comparison
            else
                return 1;
        } else {
            if (sb.buf)
                return -1;
            else
                return (int)sa.size - (int)sb.size;     // integer key comparison
        }
        assert(sa.buf && sb.buf);
        return sa.compare(sb);
    }

    void Encoder::sortDict(valueArray &items) {
        auto &keys = items.keys;
        size_t n = keys.size();
        if (n < 2)
            return;

        // Fill in the pointers of any keys that refer to inline strings:
        for (unsigned i = 0; i < n; i++) {
            if (keys[i].buf == nullptr) {
                const Value *item = &items[2*i];
                if (item->tag() == kStringTag)
                    keys[i].buf = offsetby(item, 1);                // inline string
                else
                    keys[i] = slice(nullptr, item->asUnsigned());   // integer
            }
        }

        // Construct an array that describes the permutation of item indices:
        StackArray(indices, const slice*, n);
        const slice* base = &keys[0];
        for (unsigned i = 0; i < n; i++)
            indices[i] = base + i;
        ::qsort(indices, n, sizeof(indices[0]), &compareKeysByIndex);
        // indices[i] is now a pointer to the Value that should go at index i

        // Now rewrite items according to the permutation in indices:
        Value *old = (Value*) alloca(2*n * sizeof(Value));
        memcpy(old, &items[0], 2*n * sizeof(Value));
        for (size_t i = 0; i < n; i++) {
            auto j = indices[i] - base;
            if ((ssize_t)i != j) {
                items[2*i]   = old[2*j];
                items[2*i+1] = old[2*j+1];
            }
        }
    }

    // Writes an array of strings, containing all the strings that have appeared as dictionary keys
    // so far. The array is structured as a hash table; it's literally organized as a dump of a
    // StringTable. That means that a reader can use the same hash function and algorithm as
    // StringTable to very quickly convert a platform string to a Fleece Value.
    void Encoder::writeKeyTable() {
        StringTable keys;
        for (auto iter = _strings.begin(); iter != _strings.end(); ++iter) {
            if (iter->buf && iter.value().usedAsKey)
                keys.add(iter, iter.value());
        }

        beginArray();
        for (auto iter = keys.begin(); iter != keys.end(); ++iter) {
            if (iter->buf) {
                writeString(iter);
            } else {
                writeNull();
            }
        }
        endArray();
    }
}
