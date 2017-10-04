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

namespace fleece {

    /** A mutable array of MValues. */
    template <class Native>
    class MArray : public MCollection<Native> {
    public:
        using MValue = MValue<Native>;

        MArray() { }

        void init(MValue *mv, MCollection<Native> *parent) {
            MCollection<Native>::init(mv, parent);
            _array = (const Array*)mv->value();
            _vec.clear();
            _vec.resize(_array->count());
        }

        void init(const MArray &a) {
            _array = a._array;
            _vec = a._vec;
        }

        uint32_t count() const {
            return (uint32_t)_vec.size();
        }

        const MValue& get(uint32_t i) const {
            if (i >= _vec.size())
                return MValue::empty;
            const MValue &val = _vec[i];
            if (val.isEmpty())
                const_cast<MValue&>(val) = _array->get(i);
            return val;
        }

        void set(uint32_t i, Native val) {
            assert(i < count());
            MCollection<Native>::mutate();
            _vec[i] = val;
        }

        void populateVec() {
            uint32_t i = 0;
            for (auto &v : _vec) {
                if (v.isEmpty())
                    v = _array->get(i++);
            }
        }

        void insert(uint32_t i, Native val) {
            assert(i <= count());
            MCollection<Native>::mutate();
            if (i < count())
                populateVec();
            _vec.emplace(_vec.begin() + i, val);
        }

        void remove(uint32_t i) {
            assert(i < count());
            MCollection<Native>::mutate();
            if (i < count()-1)
                populateVec();
            _vec.erase(_vec.begin() + i);
        }

        void clear() {
            if (_vec.empty())
                return;
            MCollection<Native>::mutate();
            _vec.clear();
        }

        void encodeTo(Encoder &enc) const {
            if (!MCollection<Native>::isMutated()) {
                enc << _array;
            } else {
                enc.beginArray(count());
                uint32_t i = 0;
                for (auto &v : _vec) {
                    if (v.isEmpty())
                        enc.writeValue(_array->get(i));
                    else
                        v.encodeTo(enc);
                    ++i;
                }
                enc.endArray();
            }
        }

        // This method must be implemented for each specific Native type:
        static MArray* fromNative(Native);

    private:
        const Array*        _array {nullptr};
        std::vector<MValue> _vec;
    };

}
