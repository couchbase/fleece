//
// MArray.hh
//
// Copyright (c) 2017 Couchbase, Inc All rights reserved.
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

#pragma once
#include "MCollection.hh"
#include <vector>

namespace fleece {

    /** A mutable array of MValues. */
    template <class Native>
    class MArray : public MCollection<Native> {
    public:
        using MValue = MValue<Native>;
        using MCollection = MCollection<Native>;

        /** Constructs an empty MArray not connected to any existing Fleece Array. */
        MArray() :MCollection() { }

        /** Constructs an MArray that shadows an Array stored in `mv` and contained in `parent`.
            This is what you'd call from MValue::toNative. */
        MArray(MValue *mv, MCollection *parent) {
            initInSlot(mv, parent);
        }

        /** Initializes a brand-new MArray created with the empty constructor, as though it had
            been created with the existing-Array constructor. Useful in situations where you can't
            pass parameters to the constructor (i.e. when embedding an MArray in an Objective-C++
            object.) */
        void initInSlot(MValue *mv, MCollection *parent, bool isMutable) {
            MCollection::initInSlot(mv, parent, isMutable);
            assert(!_array);
            _array = mv->value().asArray();
            _vec.resize(_array.count());
        }

        void initInSlot(MValue *mv, MCollection *parent) {
            initInSlot(mv, parent, parent->mutableChildren());
        }

        /** Copies the MArray a into the receiver. */
        void initAsCopyOf(const MArray &a, bool isMutable) {
            MCollection::initAsCopyOf(a, isMutable);
            _array = a._array;
            _vec = a._vec;
        }

        Array baseArray() const {
            return _array;
        }

        /** Returns the number of items in the array. */
        uint32_t count() const {
            return (uint32_t)_vec.size();
        }

        /** Returns a reference to the MValue of the item at the given index.
            If the index is out of range, returns an empty MValue. */
        const MValue& get(size_t i) const {
            if (_usuallyFalse(i >= _vec.size()))
                return MValue::empty;
            const MValue &val = _vec[i];
            if (_usuallyTrue(val.isEmpty()))
                const_cast<MValue&>(val) = _array[(uint32_t)i];
            return val;
        }

        /** Stores a Native value into the array.
            If the index is out of range, returns false. */
        bool set(size_t i, Native val) {
            if (_usuallyFalse(!MCollection::isMutable()))
                return false;
            if (_usuallyFalse(i >= count() || val == nullptr))
                return false;
            MCollection::mutate();
            _vec[i] = val;
            return true;
        }

        /** Inserts the value `val` into the array at index `i`,
            or returns false if the array is out of range (greater than the count.) */
        bool insert(size_t i, Native val) {
            if (_usuallyFalse(!MCollection::isMutable()))
                return false;
            size_t cnt = count();
            if (_usuallyFalse(i > cnt || val == nullptr))
                return false;
            else if (i < cnt)
                populateVec();
            MCollection::mutate();
            _vec.emplace(_vec.begin() + i, val);
            return true;
        }

        bool append(Native val) {
            return insert(count(), val);
        }

        /** Removes `n` values starting at index `i`, or returns false if the range is invalid */
        bool remove(size_t i, size_t n =1) {
            if (_usuallyFalse(!MCollection::isMutable()))
                return false;
            size_t end = i + n;
            if (end <= i)
                return (end == i);
            size_t cnt = count();
            if (_usuallyFalse(end > cnt))
                return false;
            if (end < cnt)
                populateVec();
            MCollection::mutate();
            _vec.erase(_vec.begin() + i, _vec.begin() + end);
            return true;
        }

        /** Removes all items from the array. */
        bool clear() {
            if (_usuallyFalse(!MCollection::isMutable()))
                return false;
            if (_vec.empty())
                return true;
            MCollection::mutate();
            _vec.clear();
            return true;
        }

        /** Writes the array to an Encoder as a single Value. */
        void encodeTo(Encoder &enc) const {
            if (!MCollection::isMutated()) {
                enc << _array;
            } else {
                enc.beginArray(count());
                uint32_t i = 0;
                for (auto &v : _vec) {
                    if (v.isEmpty())
                        enc.writeValue(_array[i]);
                    else
                        v.encodeTo(enc);
                    ++i;
                }
                enc.endArray();
            }
        }

    private:
        /** Loads the Fleece Values of all items into _array.
            Called by insert() and remove() before they perturb the array indexing. */
        void populateVec() {
            uint32_t i = 0;
            for (auto &v : _vec) {
                if (_usuallyTrue(v.isEmpty()))
                    v = _array[i++];
            }
        }

        Array               _array;     // Base Fleece Array (if any)
        std::vector<MValue> _vec;       // Current array; empty MValues are unmodified
    };

}
