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
#include "PlatformCompat.hh"
#include <atomic>
#include <utility>

namespace fleece {

    /** Simple thread-safe ref-counting implementation.
        `RefCounted` objects should be managed by \ref Retained smart-pointers:
        `Retained<Foo> foo = new Foo(...)` or `auto foo = make_retained<Foo>(...)`.
        \note The ref-count starts at 0, so you must call retain() on an instance, or assign it
        to a Retained, right after constructing it. */
    class RefCounted {
    public:
        RefCounted()                            { }
        
        int refCount() const FLPURE                    { return _refCount; }

    protected:
        RefCounted(const RefCounted &)          { }

        /** Destructor is accessible only so that it can be overridden.
            **Never call `delete`**, only `release`! Overrides should be made protected or private. */
        virtual ~RefCounted();

    private:
        template <typename T>
        friend T* retain(T*) noexcept;
        friend void release(const RefCounted*) noexcept;
        friend void moveRef(RefCounted* dst, RefCounted *src) noexcept;

#if DEBUG
        void _retain() const noexcept                 {_careful_retain();}
        void _release() const noexcept                {_careful_release();}
#else
        ALWAYS_INLINE void _retain() const noexcept   { ++_refCount; }
        void _release() const noexcept;
#endif

        static constexpr int32_t kCarefulInitialRefCount = -6666666;
        void _careful_retain() const noexcept;
        void _careful_release() const noexcept;

        mutable std::atomic<int32_t> _refCount
#if DEBUG
                                               {kCarefulInitialRefCount};
#else
                                               {0};
#endif
    };


    /** Retains a RefCounted object and returns the object. Does nothing given a null pointer.
        \warning Manual retain/release is error prone. This function is intended mostly for interfacing
        with C code that can't use \ref Retained. */
    template <typename REFCOUNTED>
    ALWAYS_INLINE REFCOUNTED* retain(REFCOUNTED *r) noexcept {
        if (r) r->_retain();
        return r;
    }

    /** Releases a RefCounted object. Does nothing given a null pointer.
        \warning Manual retain/release is error prone. This function is intended mostly for interfacing
        with C code that can't use \ref Retained. */
    NOINLINE void release(const RefCounted *r) noexcept;

    // Used internally by Retained. Retains nuu and releases old.
    void moveRef(RefCounted* old, RefCounted *nuu) noexcept;

    /** Simple smart pointer that retains the RefCounted instance it holds. */
    template <typename T>
    class Retained {
    public:
        Retained() noexcept                             :_ref(nullptr) { }
        Retained(T *t) noexcept                         :_ref(retain(t)) { }

        Retained(const Retained &r) noexcept            :_ref(retain(r._ref)) { }
        Retained(Retained &&r) noexcept                 :_ref(r._ref) {r._ref = nullptr;}

        template <typename U>
        Retained(const Retained<U> &r) noexcept         :_ref(retain(r._ref)) { }

        template <typename U>
        Retained(Retained<U> &&r) noexcept              :_ref(r._ref) {r._ref = nullptr;}

        ~Retained()                                     {release(_ref);}

        operator T* () const noexcept FLPURE STEPOVER   {return _ref;}
        T* operator-> () const noexcept FLPURE STEPOVER {return _ref;}
        T* get() const noexcept FLPURE                  {return _ref;}

        explicit operator bool () const FLPURE          {return (_ref != nullptr);}

        Retained& operator=(T *t) noexcept              {moveRef(_ref, t); _ref = t; return *this;}

        Retained& operator=(const Retained &r) noexcept {return *this = r._ref;}

        template <typename U>
        Retained& operator=(const Retained<U> &r) noexcept {
            return *this = r._ref;
        }

        Retained& operator= (Retained &&r) noexcept {
            auto oldRef = _ref;
            _ref = r._ref;
            r._ref = nullptr;
            release(oldRef);
            return *this;
        }

        template <typename U>
        Retained& operator= (Retained<U> &&r) noexcept {
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
        RetainedConst() noexcept                                :_ref(nullptr) { }
        RetainedConst(const T *t) noexcept                      :_ref(retain(t)) { }
        RetainedConst(const RetainedConst &r) noexcept          :_ref(retain(r._ref)) { }
        RetainedConst(RetainedConst &&r) noexcept               :_ref(r._ref) {r._ref = nullptr;}
        ALWAYS_INLINE ~RetainedConst()                          {release(_ref);}

        operator const T* () const noexcept FLPURE  STEPOVER    {return _ref;}
        const T* operator-> () const noexcept FLPURE STEPOVER   {return _ref;}
        const T* get() const noexcept FLPURE                    {return _ref;}

        RetainedConst& operator=(const T *t) noexcept {
            auto oldRef = _ref;
            _ref = retain(t);
            release(oldRef);
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


    /// Wraps a `RefCounted` pointer in a `Retained`, bumping its refcount.
    template <typename REFCOUNTED>
    inline Retained<REFCOUNTED> retained(REFCOUNTED *r) noexcept {
        return Retained<REFCOUNTED>(r);
    }

    /// Wraps a `const RefCounted` pointer in a `RetainedConst`, bumping its refcount.
    template <typename REFCOUNTED>
    inline RetainedConst<REFCOUNTED> retained(const REFCOUNTED *r) noexcept {
        return RetainedConst<REFCOUNTED>(r);
    }


    /// make_retained<T>(...) is equivalent to `std::make_unique` and `std::make_shared`.
    /// It constructs a new RefCounted object, passing params to the constructor, returning a `Retained`.
    template<class T, class... _Args>
    static inline Retained<T>
    make_retained(_Args&&... __args) {
        return Retained<T>(new T(std::forward<_Args>(__args)...));
    }

}
