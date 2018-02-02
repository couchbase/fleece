//
// Val.hh
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

#ifndef Val_hh
#define Val_hh

#include "Fleece.hh"

namespace fleece {

    class Arr;
    class Dic;


    class Val {
    public:
        Val()                           :_v( (const Value*)kUndefined ) { }
        explicit Val(const Value *v)    :_v(v ? v : (const Value*)kUndefined) { }

        valueType type() const          {return _v->type();}

        bool isUndefined()              {return _v == (const Value*)kUndefined;}

        explicit operator bool() const  {return _v->asBool();}
        operator int64_t() const        {return _v->asInt();}
        operator uint64_t() const       {return _v->asUnsigned();}
        operator double() const         {return _v->asDouble();}

        operator slice() const          {return _v->asString();}
        operator std::string() const    {return (std::string)_v->asString();}

        inline operator Arr() const;
        inline operator Dic() const;

    private:
        friend class Arr;
        explicit Val(const Value *v, bool) :_v(v) { }

        const Value *_v;

        static const uint8_t kUndefined[2];
    };
    

    class Arr {
    public:
        Arr()                           :_a((const Value*)kUndefined) { }
        Arr(const Array *a)             :_a(a ? a : (const Value*)kUndefined) { }

        uint32_t count() const          {return _a._count;}

        Val operator[] (uint32_t i) const     {return Val(_a[i], true);}

        class iterator {
        public:
            iterator(Arr a)             :iterator(a._a._first, a._a._wide) { }
            operator Val() const        {return Val(Value::deref(_v, _wide), true);}
            Val operator-> () const     {return operator Val();}
            iterator& operator++()      {_v = _v->next(_wide); return *this;}
            bool operator== (const iterator& i) const {return _v == i._v;}
        private:
            friend class Arr;

            iterator(const Value *v, bool wide)   :_v(v), _wide(wide) { }
            const Value *_v;
            bool _wide;
        };

        iterator begin()                {return iterator(_a._first, _a._wide);}
        iterator end()                  {return iterator(offsetby(_a._first, internal::width(_a._wide)*_a._count), _a._wide);}

    private:
        friend class iterator;
        const Array::impl _a;

        static const uint8_t kUndefined[2];
    };


    class Dic {
    public:
        Dic()                          :_d((const Dict*)kUndefined) { }
        Dic(const Dict *d)             :_d(d) { }

        uint32_t count() const          {return _d->count();}

        template <typename KEY>
        Val operator[] (KEY key) const     {return Val(_d->get(key));}

    private:
        const Dict *_d;

        static const uint8_t kUndefined[2];
    };



    Val::operator Arr() const         {return Arr(_v->asArray());}
    Val::operator Dic() const         {return Dic(_v->asDict());}

}
#endif /* Val_hh */
