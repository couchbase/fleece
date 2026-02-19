//
// RefCounted.hh
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/PlatformCompat.hh"
#include <atomic>
#include <concepts>
#include <cstddef>
#include <stdexcept>
#include <utility>

FL_ASSUME_NONNULL_BEGIN

namespace fleece {

    enum Nullability {NonNull, MaybeNull};

    /** Thread-safe ref-counting implementation.
        `RefCounted` objects should be managed by \ref Retained smart-pointers:
        `Retained<Foo> foo = new Foo(...)` or `auto foo = make_retained<Foo>(...)`.
        \note The ref-count starts at 0, so you must call retain() on an instance, or assign it
        to a Retained, right after constructing it. */
    class RefCounted {
    public:
        RefCounted()                            =default;

        /// The number of (strong) references to this object.
        int refCount() const FLPURE;

        /// Returns the strong and the weak reference count.
        std::pair<int,int> refCounts() const FLPURE;

        /// The global number of objects with weak references but no strong references.
        static size_t zombieCount();

    protected:
        RefCounted(const RefCounted &)          { } // must not copy the refCount!

        /** Destructor is accessible only so that it can be overridden.
            **Never call `delete`**, only `release`! Overrides should be made protected or private. */
        virtual ~RefCounted() noexcept;

    private:
        template <typename T, Nullability N> friend class Retained;
        template <typename T, Nullability N> friend class WeakRetained;
        template <typename T> friend T* FL_NULLABLE retain(T* FL_NULLABLE) noexcept;
        friend void release(const RefCounted* FL_NULLABLE) noexcept;
        friend void assignRef(RefCounted* FL_NULLABLE &dst, RefCounted* FL_NULLABLE src) noexcept;

#if DEBUG
        void _retain() const noexcept;
        static constexpr uint64_t kInitialRefCount = 0x66666666;
#else
        ALWAYS_INLINE void _retain() const noexcept   { ++_bothRefCounts; }
        static constexpr uint64_t kInitialRefCount = 1;
#endif
        void _release() const noexcept;

        void _weakRetain() const noexcept;
        void _weakRelease() const noexcept;
        bool _weakToStrong() const noexcept;

        mutable std::atomic<uint64_t> _bothRefCounts {kInitialRefCount};
    };

    template <class T> concept RefCountedType = std::derived_from<T, RefCounted>;


    /** Retains a RefCounted object and returns the object. Does nothing given a null pointer.
        (See also `retain(Retained&&)`, below.)
        \warning Manual retain/release is error prone. This function is intended mostly for interfacing
        with C code that can't use \ref Retained. */
    template <typename T>
    ALWAYS_INLINE T* FL_NULLABLE retain(T* FL_NULLABLE r) noexcept {
        if (r) r->_retain();
        return r;
    }

    /** Releases a RefCounted object. Does nothing given a null pointer.
        \warning Manual retain/release is error prone. This function is intended mostly for interfacing
        with C code that can't use \ref Retained. */
    void release(const RefCounted* FL_NULLABLE) noexcept;

    // Makes `assignRef` polymorphic with RefCounted subclasses.
    template <RefCountedType T>
    inline void assignRef(T* FL_NULLABLE &holder, RefCounted* FL_NULLABLE newValue) noexcept {
        assignRef((RefCounted* FL_NULLABLE&)holder, newValue);
    }

    // Type `nullable_if<T,Nullability>::ptr` is a pointer to T with the appropriate nullability.
#if __has_feature(nullability)
    template <typename X, Nullability> struct nullable_if;
    template <typename X> struct nullable_if<X,MaybeNull>   {using ptr = X* _Nullable;};
    template <typename X> struct nullable_if<X,NonNull>     {using ptr = X* _Nonnull;};
#else
    template <typename X, Nullability> struct nullable_if   {using ptr = X*;};
#endif

    [[noreturn]] void _failNullRef();

    /** A smart pointer that retains the RefCounted instance it holds, similar to `std::shared_ptr`.

        Comes in two flavors: if `N` is `MaybeNull` (the default), it may hold a null pointer.
        This is the typical `Retained<T>` that's been around for years.

        If `N` is `NonNull`, it may not hold a null pointer, and any attempt to store one will
        throw a `std::invalid_argument` exception, and/or trigger UBSan if it's enabled.
        In some cases nullability violations will be caught at compile time.
        This flavor is commonly abbreviated `Ref<T>`.

        There is no implicit conversion from `Retained<T>` to `Ref<T>`. Use the `asRef` method,
        which throws if the value is nullptr. */
    template <class T, Nullability N = MaybeNull>
    class Retained {
    public:
        using T_ptr = typename nullable_if<T,N>::ptr; // This is `T*` with appropriate nullability

        Retained() noexcept requires (N==MaybeNull)                 :_ref(nullptr) { }
        Retained(std::nullptr_t) noexcept requires (N==MaybeNull)   :Retained() { } // optimization
        Retained(T_ptr t) noexcept                                  :_ref(_retain(t)) { }

        Retained(const Retained &r) noexcept                        :_ref(_retain(r.get())) { }
        Retained(Retained &&r) noexcept                             :_ref(std::move(r).detach()) { }

        template <typename U, Nullability UN> requires (std::derived_from<U,T> && N >= UN)
        Retained(const Retained<U,UN> &r) noexcept                  :_ref(_retain(r.get())) { }
        template <typename U, Nullability UN> requires (std::derived_from<U,T> && N >= UN)
        Retained(Retained<U,UN> &&r) noexcept                       :_ref(std::move(r).detach()) { }

        ~Retained() noexcept                                        {release(_ref);}

        Retained& operator=(T_ptr t) & noexcept {
            _retain(t);
            std::swap(_ref, t);
            release(t);
            return *this;
        }

        Retained& operator=(std::nullptr_t) & noexcept requires(N==MaybeNull) { // optimized assignment
            auto oldRef = _ref;
            _ref = nullptr;
            release(oldRef);
            return *this;
        }

        Retained& operator=(const Retained &r) & noexcept       {*this = r.get(); return *this;}

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        Retained& operator=(const Retained<U,UN> &r) & noexcept {*this = r.get(); return *this;}

        Retained& operator= (Retained &&r) & noexcept {
            std::swap(_ref, r._ref);   // old _ref will be released by r's destructor
            return *this;
        }

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        Retained& operator= (Retained<U,UN> &&r) & noexcept {
            if ((void*)&r != this) {
                auto oldRef = _ref;
                _ref = std::move(r).detach();
                _release(oldRef);
            }
            return *this;
        }
        
        explicit operator bool () const FLPURE          {return N==NonNull || (_ref != nullptr);}

        // typical dereference operations:
        operator T_ptr () const & noexcept LIFETIMEBOUND FLPURE STEPOVER {return _ref;}
        T_ptr operator-> () const noexcept LIFETIMEBOUND FLPURE STEPOVER {return _ref;}
        T_ptr get() const noexcept LIFETIMEBOUND FLPURE STEPOVER         {return _ref;}

        /// Converts any Retained to non-nullable form (Ref), or throws if its value is nullptr.
        Retained<T,NonNull> asRef() const & noexcept(!N) {return Retained<T,NonNull>(_ref);}
        Retained<T,NonNull> asRef() && noexcept(!N) {
            Retained<T,NonNull> result(_ref, false);
            _ref = nullptr;
            return result;
        }

        /// Converts a Retained into a raw pointer with a +1 reference that must be released.
        /// Used in C++ functions that bridge to C and return C references.
        /// @note  The opposite of this is \ref adopt.
        [[nodiscard]]
        T_ptr detach() && noexcept                  {auto r = _ref; _ref = nullptr; return r;}

        /// Converts a raw pointer with a +1 reference into a Retained object.
        /// This has no effect on the object's ref-count; the existing +1 ref will be released when
        /// the Retained destructs. */
        [[nodiscard]] static Retained adopt(T_ptr t) noexcept {
            return Retained(t, false);
        }

        /// Equivalent to `get` but without the `LIFETIMEBOUND` attribute. For use in rare cases where you have
        /// a `Retained<T>` and need to return it as a `T*`, which is normally illegal, but you know that there's
        /// another `Retained` value keeping the object alive even after this function returns.
        T_ptr unsafe_get() const noexcept          {return _ref;}

        // The operator below is often a dangerous mistake, so it's deliberately made impossible.
        // It happens in these sorts of contexts, where it can produce a dangling pointer to a
        // deleted object:
        //      Retained<Foo> createFoo();
        //      ...
        //      Foo *foo = createFoo();     // ☠️
        // or:
        //      return createFoo();         // ☠️
        //
        // However, it _is_ valid if you're passing the Retained r-value as a function parameter,
        // since it will not be released until after the function returns:
        //      void handleFoo(Foo*);
        //      ...
        //      handleFoo( createFoo() );           // Would be OK, but prohibited due to the above
        // In this case you can use an explicit `get()` to work around the prohibition:
        //      handleFoo( createFoo().get() );     // OK!
        // ...or promote it to an l-value:
        //      Retained<Foo> foo = createFoo();
        //      handleFoo(foo);                     // OK!
        // ...or change `handleFoo`s parameter to Retained:
        //      void handleFoo(Retained<Foo>);
        //      ...
        //      handleFoo( createFoo() );           // OK!
        operator T_ptr () const && =delete; // see above^

    private:
        template <class U, Nullability UN> friend class Retained;

        Retained(T_ptr t, bool) noexcept(N==MaybeNull) // private no-retain ctor
        :_ref(t) {
            if constexpr (N == NonNull) {
                if (t == nullptr) [[unlikely]]
                    _failNullRef();
            }
        }

        static T_ptr _retain(T_ptr t) noexcept {
            if constexpr (N == NonNull && std::derived_from<T, Retained>)
                t->_retain(); // this is faster, and it detects illegal null (by signal)
            else
                retain(t);
            return t;
        }

        static void _release(T_ptr t) noexcept {
            if constexpr (N == NonNull && std::derived_from<T, Retained>)
                t->_release(); // this is faster, and it detects illegal null (by signal)
            else
                release(t);
        }

        // _ref has to be declared nullable, even when N==NonNull, because a move assignment
        // sets the moved-from _ref to nullptr. The Retained may not used any more in this state,
        // but it will be destructed, which is why the destructor also checks for nullptr.
        T* FL_NULLABLE _ref;
    };

    template <class T> Retained(T* FL_NULLABLE) -> Retained<T>; // deduction guide


    /// Ref<T> is an alias for a non-nullable Retained<T>.
    template <class T> using Ref = Retained<T, NonNull>;

    /// NullableRef<T> is an alias for a (default) nullable Retained<T>.
    template <class T> using NullableRef = Retained<T, MaybeNull>;

    /// RetainedConst is an alias for a Retained that holds a const pointer.
    template <class T> using RetainedConst = Retained<const T>;


    /** Wraps a pointer in a Retained. */
    template <RefCountedType T>
    [[nodiscard]] Retained<T> retained(T* FL_NULLABLE r) noexcept {
        return Retained<T>(r);
    }

    /** Wraps a non-null pointer in a Ref. */
    template <RefCountedType T>
    [[nodiscard]] Ref<T> retainedRef(T* FL_NONNULL r) noexcept {
        return Ref<T>(r);
    }

    /** Converts a raw pointer with a +1 reference into a Retained object.
        This has no effect on the object's ref-count; the existing +1 ref will be released when the
        Retained destructs. */
    template <RefCountedType T>
    [[nodiscard]] Retained<T> adopt(T* FL_NULLABLE r) noexcept {
        return Retained<T>::adopt(r);
    }

    /** make_retained<T>(...) is similar to `std::make_unique` and `std::make_shared`.
        It constructs a new RefCounted object, passing params to the constructor, returning a `Ref`. */
    template<RefCountedType T, class... Args>
    [[nodiscard]] Ref<T> make_retained(Args&&... args) {
        return Ref<T>(new T(std::forward<Args>(args)...));
    }

    /** `retained_cast<T>(...)` is like `dynamic_cast` but on pointers wrapped in Retained.
        If the arg is not an instance of T, the result will be empty/null. */
    template <RefCountedType T, RefCountedType U, Nullability Nullable>
    [[nodiscard]] Retained<T,Nullable> retained_cast(Retained<U,Nullable> r) noexcept {
        return adopt<T>(dynamic_cast<T*>(std::move(r).detach()));
    }


    /** Extracts the pointer from a Retained. It must later be released via `release`.
        This is used in bridging functions that return a direct pointer for a C API. */
    template <typename T, Nullability N>
    [[nodiscard]] ALWAYS_INLINE T* FL_NULLABLE retain(Retained<T,N> &&retained) noexcept {
        return std::move(retained).detach();
    }


    /** A ref-counted smart pointer to an arbitrary type `T` which does not have to derive from
     *  \ref RefCounted. However, it has to be instantiated with an instance of a subclass which
     *  _does_ derive from RefCounted.
     *
     *  This is useful when you have something like a pure-virtual delegate interface and want to
     *  manage delegates with ref-counting. Making the interface derive from RefCounted would be
     *  awkward for classes that want to implement it, if they already derive from RefCounted
     *  through another base class. Instead, the delegate can be passed as a RetainedBySubclass.
     *
     *  PS: If you need a _weak_ reference, use \ref WeakRetainedBySubclass. */
    template <class T>
    class RetainedBySubclass {
    public:
        RetainedBySubclass() = default;

        template <std::derived_from<T> Sub>
        explicit RetainedBySubclass(Sub* FL_NULLABLE sub) noexcept
            requires (std::derived_from<Sub,RefCounted>)
            :_ptr{sub}, _ref{sub} { }

        template <std::derived_from<T> Sub, Nullability N>
        explicit RetainedBySubclass(Retained<Sub,N> sub) noexcept
            requires (std::derived_from<Sub,RefCounted>)
            :_ptr{sub}, _ref{std::move(sub)} { }

        explicit operator bool() const noexcept FLPURE      {return _ptr != nullptr;}

        T* FL_NULLABLE get() const noexcept FLPURE          {return _ptr;}
        T* FL_NULLABLE operator*() const noexcept FLPURE    {return _ptr;}
        T* FL_NULLABLE operator->() const noexcept FLPURE   {return _ptr;}

        void clear()                                        {_ref = nullptr; _ptr = nullptr;}

    private:
        template <class U> friend class WeakRetainedBySubclass;
        RetainedBySubclass(T* FL_NULLABLE ptr, Retained<RefCounted> sub)
            :_ptr(ptr), _ref(std::move(sub)) { }

        T* FL_NULLABLE       _ptr = nullptr;
        Retained<RefCounted> _ref;
    };

}

FL_ASSUME_NONNULL_END
