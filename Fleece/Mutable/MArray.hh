//
//  MArray.hh
//  Fleece
//
//  Created by Jens Alfke on 5/29/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "MCollection.hh"
#include <vector>

namespace fleeceapi {

    /** A mutable array of MValues. */
    template <class Native>
    class MArray : public MCollection<Native> {
    public:
        using MValue = MValue<Native>;
        using MCollection = MCollection<Native>;

        MArray() { }

        void initInSlot(MValue *mv, MCollection *parent) {
            MCollection::initInSlot(mv, parent);
            _array = mv->value().asArray();
            _vec.clear();
            _vec.resize(_array.count());
        }

        void init(const MArray &a) {
            _array = a._array;
            _vec = a._vec;
        }

        uint32_t count() const {
            return (uint32_t)_vec.size();
        }

        const MValue& get(size_t i) const {
            if (i >= _vec.size())
                return MValue::empty;
            const MValue &val = _vec[i];
            if (val.isEmpty())
                const_cast<MValue&>(val) = _array[(uint32_t)i];
            return val;
        }

        bool set(size_t i, Native val) {
            if (i >= count() || val == nullptr)
                return false;
            MCollection::mutate();
            _vec[i] = val;
            return true;
        }

        // Loads the Fleece Values of all items into _array.
        // Called by insert() and remove() before they perturb the array indexing.
        void populateVec() {
            uint32_t i = 0;
            for (auto &v : _vec) {
                if (v.isEmpty())
                    v = _array[i++];
            }
        }

        bool insert(size_t i, Native val) {
            size_t cnt = count();
            if (i > cnt || val == nullptr)
                return false;
            else if (i < cnt)
                populateVec();
            MCollection::mutate();
            _vec.emplace(_vec.begin() + i, val);
            return true;
        }

        bool remove(size_t i) {
            size_t cnt = count();
            if (i >= cnt)
                return false;
            else if (i < cnt-1)
                populateVec();
            MCollection::mutate();
            _vec.erase(_vec.begin() + i);
            return true;
        }

        void clear() {
            if (_vec.empty())
                return;
            MCollection::mutate();
            _vec.clear();
        }

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
        Array               _array;     // Base Fleece Array (if any)
        std::vector<MValue> _vec;       // Current array; empty MValues are unmodified
    };

}
