//
// AtomicRetained.cc
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
#include "fleece/RefCounted.hh"

namespace fleece {

    namespace internal {
        class AtomicWrapper {
        public:
            explicit AtomicWrapper(uintptr_t ref) noexcept;
            explicit AtomicWrapper(const void* FL_NULLABLE ref) noexcept
                :AtomicWrapper(reinterpret_cast<uintptr_t>(ref)) { }

            /// Safe access to `_ref` inside a callback. Returns whatever the callback returned.
            template<std::invocable<uintptr_t> FN>
            auto use(FN fn) const noexcept {
                uintptr_t ref = getAndLock();
                auto result = fn(ref);
                setAndUnlock(ref, ref);
                return result;
            }
            /// Atomically swaps `_ref` with `newRef` and returns the old value.
            uintptr_t exchangeWith(uintptr_t newRef) noexcept;

        private:
            uintptr_t getAndLock() const noexcept;
            void setAndUnlock(uintptr_t oldRef, uintptr_t newRef) const noexcept;

            mutable std::atomic<uintptr_t> _ref;
        };
    }


    /** A fully thread-safe version of `Retained` that supports concurrent gets and sets.
     *  It's a lot slower than `Retained`, so use it only when necessary: in situations where it
     *  will be called on multiple threads. */
    template <class T, Nullability N = MaybeNull>
    class AtomicRetained {
    public:
        using T_ptr = typename Retained<T,N>::template nullable_if<T,N>::ptr;
        using T_nullable_ptr = T* FL_NULLABLE;

        AtomicRetained() noexcept requires (N==MaybeNull)                 :_ref(nullptr) { }
        AtomicRetained(std::nullptr_t) noexcept requires (N==MaybeNull)   :AtomicRetained() { }
        AtomicRetained(T_ptr t) noexcept                                  :_ref(_retain(t)) { }

        AtomicRetained(const AtomicRetained &r) noexcept        :_ref(r._getRef()) { }
        AtomicRetained(AtomicRetained &&r) noexcept             :_ref(std::move(r).detach()) { }

        template <typename U, Nullability UN> requires (std::derived_from<U,T> && N >= UN)
        AtomicRetained(const AtomicRetained<U,UN> &r) noexcept  :_ref(r._getRef()) { }
        template <typename U, Nullability UN> requires (std::derived_from<U,T> && N >= UN)
        AtomicRetained(AtomicRetained<U,UN> &&r) noexcept       :_ref(std::move(r).detach()) { }

        ~AtomicRetained() noexcept                              {release(_unretainedGet());}

        AtomicRetained& operator=(T_ptr r) & noexcept {
            auto oldRef = _exchangeWith(_retain(r));
            release(oldRef);
            return *this;
        }

        AtomicRetained& operator=(const AtomicRetained &r) & noexcept {
            *this = r.get(); return *this;
        }

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        AtomicRetained& operator=(const AtomicRetained<U,UN> &r) & noexcept {
            *this = r.get(); return *this;
        }

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        AtomicRetained& operator=(const Retained<U,UN> &r) & noexcept {
            *this = r.get(); return *this;
        }

        AtomicRetained& operator= (AtomicRetained &&r) & noexcept {
            T_ptr newRef = std::move(r).detach();  // retained
            auto oldRef = _exchangeWith(newRef);
            release(oldRef);
            return *this;
        }

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        AtomicRetained& operator= (AtomicRetained<U,UN> &&r) & noexcept {
            T_ptr newRef = std::move(r).detach(); // newRef is already retained
            auto oldRef = _exchangeWith(newRef);
            release(oldRef);
            return *this;
        }
        
        explicit operator bool () const FLPURE          {return N==NonNull || (get() != nullptr);}

        // AtomicRetained doesn't dereference to `T*`, rather to `Retained<T>`.
        // This prevents a concurrent mutation from releasing the object out from under you,
        // since that temporary Retained object keeps it alive for the duration of the expression.

        operator Retained<T,N> () const & noexcept LIFETIMEBOUND FLPURE STEPOVER {return _get();}
        Retained<T,N> operator-> () const noexcept LIFETIMEBOUND FLPURE STEPOVER {return _get();}
        Retained<T,N> get() const noexcept LIFETIMEBOUND FLPURE STEPOVER         {return _get();}

        /// Converts any AtomicRetained to non-nullable form (AtomicRef), or throws if its value is nullptr.
        AtomicRetained<T,NonNull> asRef() const & noexcept(!N) {return AtomicRetained<T,NonNull>(get());}
        AtomicRetained<T,NonNull> asRef() && noexcept(!N) {
            return AtomicRetained<T,NonNull>(std::move(*this).detach(), false);  // note: non-retaining constructor
        }

        /// Converts a AtomicRetained into a raw pointer with a +1 reference that must be released.
        /// Used in C++ functions that bridge to C and return C references.
        /// @note  The opposite of this is \ref adopt.
        [[nodiscard]]
        T_ptr detach() && noexcept {
            auto oldRef = _exchangeWith(nullptr);
            return oldRef;
        }

        /// Converts a raw pointer with a +1 reference into a AtomicRetained object.
        /// This has no effect on the object's ref-count; the existing +1 ref will be released when
        /// the AtomicRetained destructs. */
        [[nodiscard]] static AtomicRetained adopt(T_ptr t) noexcept {
            return AtomicRetained(t, false);
        }

    private:
        template <class U, Nullability UN> friend class AtomicRetained;

        AtomicRetained(T_ptr t, bool) noexcept(N==MaybeNull) :_ref(t) { // private no-retain ctor
            if constexpr (N == NonNull) {
                if (t == nullptr) [[unlikely]]
                    _failNullRef();
            }
        }

        Retained<T,N> _get() const {
            return Retained<T,N>::adopt(_getRef());
        }

        T_nullable_ptr _getRef() const {
            return _ref.use( [](auto r) {
                return retain(reinterpret_cast<T_nullable_ptr>(r));
            } );
        }

        T_nullable_ptr _unretainedGet() const {
            return reinterpret_cast<T_ptr>(_ref.use( [](auto r) {return r;} ));
        }

        T_nullable_ptr _exchangeWith(T_nullable_ptr ref) {
            return reinterpret_cast<T_nullable_ptr>(_ref.exchangeWith(reinterpret_cast<uintptr_t>(ref)));
        }

        static T_ptr _retain(T_ptr t) noexcept {
            if constexpr (N == NonNull && std::derived_from<T, AtomicRetained>)
                t->_retain(); // this is faster, and it detects illegal null (by signal)
            else
                retain(t);
            return t;
        }

        static void _release(T_ptr t) noexcept {
            if constexpr (N == NonNull && std::derived_from<T, AtomicRetained>)
                t->_release(); // this is faster, and it detects illegal null (by signal)
            else
                release(t);
        }

        internal::AtomicWrapper _ref;
    };


    /// Ref<T> is an alias for a non-nullable AtomicRetained<T>.
    template <class T> using AtomicRef = AtomicRetained<T, NonNull>;

    /// NullableRef<T> is an alias for a (default) nullable AtomicRetained<T>.
    template <class T> using AtomicNullableRef = AtomicRetained<T, MaybeNull>;

    /// RetainedConst is an alias for a AtomicRetained that holds a const pointer.
    template <class T> using AtomicRetainedConst = AtomicRetained<const T>;


    template <class T> AtomicRetained(T* FL_NULLABLE) -> AtomicRetained<T>; // deduction guide
}
