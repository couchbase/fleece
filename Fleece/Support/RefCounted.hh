//
// RefCounted.hh
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

#pragma once
#include <atomic>

namespace fleece {

    /** Simple thread-safe ref-counting implementation.
        Note: The ref-count starts at 0, so you must call retain() on an instance, or assign it
        to a Retained, right after constructing it. */
    class RefCounted {
    public:
        RefCounted()                            { }
        
        int refCount() const                    { return _refCount; }

    protected:
        RefCounted(const RefCounted &)          { }

        /** Destructor is accessible only so that it can be overridden.
            Never call delete, only release! Overrides should be made protected or private. */
        virtual ~RefCounted();

    private:
        template <typename T>
        friend T* retain(T*) noexcept;
        template <typename T>
        friend void release(T*) noexcept;

#if DEBUG
        void _retain() noexcept                 {_careful_retain();}
        void _release() noexcept                {_careful_release();}
#else
        inline void _retain() noexcept          { ++_refCount; }
        inline void _release() noexcept         { if (--_refCount <= 0) delete this; }
#endif
        inline void _retain() const noexcept    {const_cast<RefCounted*>(this)->_retain();}
        inline void _release() const noexcept   {const_cast<RefCounted*>(this)->_release();}

        static constexpr int32_t kCarefulInitialRefCount = -6666666;
        void _careful_retain() noexcept;
        void _careful_release() noexcept;

        std::atomic<int32_t> _refCount
#if DEBUG
                                        {kCarefulInitialRefCount};
#else
                                        {0};
#endif
    };


    /** Retains a RefCounted object and returns the object. Does nothing given a null pointer. */
    template <typename REFCOUNTED>
    inline REFCOUNTED* retain(REFCOUNTED *r) noexcept {
        if (r) r->_retain();
        return r;
    }

    /** Releases a RefCounted object. Does nothing given a null pointer. */
    template <typename REFCOUNTED>
    inline void release(REFCOUNTED *r) noexcept {
        if (r) r->_release();
    }


    /** Simple smart pointer that retains the RefCounted instance it holds. */
    template <typename T>
    class Retained {
    public:
        Retained() noexcept                      :_ref(nullptr) { }
        Retained(T *t) noexcept                  :_ref(retain(t)) { }

        Retained(const Retained &r) noexcept     :_ref(retain(r._ref)) { }
        Retained(Retained &&r) noexcept          :_ref(r._ref) {r._ref = nullptr;}

        template <typename U>
        Retained(const Retained<U> &r) noexcept  :_ref(retain(r._ref)) { }

        template <typename U>
        Retained(Retained<U> &&r) noexcept       :_ref(r._ref) {r._ref = nullptr;}

        ~Retained()                              {release(_ref);}

        operator T* () const noexcept            {return _ref;}
        T* operator-> () const noexcept          {return _ref;}
        T* get() const noexcept                  {return _ref;}

        explicit operator bool () const          {return (_ref != nullptr);}

        Retained& operator=(T *t) noexcept {
            auto oldRef = _ref;
            _ref = retain(t);
            release(oldRef);
            return *this;
        }

        Retained& operator=(const Retained &r) noexcept {
            return *this = r._ref;
        }

        Retained& operator= (Retained &&r) noexcept {
            auto oldRef = _ref;
            _ref = r._ref;
            r._ref = nullptr;
            release(oldRef);
            return *this;
        }

    private:
        template <class U> friend class Retained;

        T *_ref;
    };


    /** Same as Retained, but when you only have a const pointer to the object. */
    template <typename T>
    class RetainedConst {
    public:
        RetainedConst() noexcept                        :_ref(nullptr) { }
        RetainedConst(const T *t) noexcept              :_ref(retain(const_cast<T*>(t))) { }
        RetainedConst(const RetainedConst &r) noexcept  :_ref(retain(r._ref)) { }
        RetainedConst(RetainedConst &&r) noexcept       :_ref(r._ref) {r._ref = nullptr;}
        ~RetainedConst()                                {release(const_cast<T*>(_ref));}

        operator const T* () const noexcept             {return _ref;}
        const T* operator-> () const noexcept           {return _ref;}
        const T* get() const noexcept                   {return _ref;}

        RetainedConst& operator=(const T *t) noexcept {
            auto oldRef = _ref;
            _ref = retain(const_cast<T*>(t));
            release(const_cast<T*>(oldRef));
            return *this;
        }

        RetainedConst& operator=(const RetainedConst &r) noexcept {
            return *this = r._ref;
        }

        RetainedConst& operator= (RetainedConst &&r) noexcept {
            auto oldRef = _ref;
            _ref = r._ref;
            r._ref = nullptr;
            release(oldRef);
            return *this;
        }

    private:
        const T *_ref;
    };


    template <typename REFCOUNTED>
    inline Retained<REFCOUNTED> retained(REFCOUNTED *r) noexcept {
        return Retained<REFCOUNTED>(r);
    }


}
