//
// MArray.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "MCollection.hh"
#include <vector>
#include <assert.h>

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
