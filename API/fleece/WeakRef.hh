//
// WeakRef.hh
//
// Copyright 2026-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "RefCounted.hh"

FL_ASSUME_NONNULL_BEGIN

namespace fleece {

    [[noreturn]] void _failZombie(void*);


    /** A smart pointer very much like \ref Retained except that it holds a _weak_ reference.
     *  The existence of the weak reference does not keep the referred-to object alive on its own;
     *  once no strong references exist, the object is freed and any weak references cleared.
     *
     *  `WeakRetained` is useful for breaking reference cycles that could otherwise cause leaks.
     *  If one object in the cycle uses a WeakRef instead of a Ref, both objects will be freed
     *  properly once there are no external references to them.
     *
     *  You can't dereference a WeakRef to a plain pointer, because that pointer could be
     *  invalidated at any moment by another thread releasing the last strong reference.
     *  Instead, dereferencing returns a Retained<> instance with a safe strong reference.
     *
     *  It's important to remember that a WeakRef can be cleared unexpectedly. Unless you know
     *  for a fact that the pointee is still alive, it's recommended to call \ref tryGet and
     *  test the return value before using it, or else call \ref use.
     */
    template <class T, Nullability N = MaybeNull>
    class WeakRetained {
    public:
        using T_ptr = typename nullable_if<T,N>::ptr; // This is `T*` with appropriate nullability

        WeakRetained() noexcept requires (N==MaybeNull)               :_ref(nullptr) { }
        WeakRetained(std::nullptr_t) noexcept requires (N==MaybeNull) :WeakRetained() { } // optimization

        #if __has_feature(nullability)
        WeakRetained(T* FL_NONNULL t) noexcept                          :_ref(_weakRetain(t)) { }
        WeakRetained(T* FL_NULLABLE t) noexcept requires(N==MaybeNull)  :_ref(_weakRetain(t)) { }
        #else
        WeakRetained(T* t) noexcept                                 :_ref(_weakRetain(t)) { }
        #endif

        WeakRetained(const WeakRetained &r) noexcept                :_ref(_weakRetain(r._ref)) { }
        WeakRetained(WeakRetained &&r) noexcept                     :_ref(std::move(r).detach()) { }

        template <typename U, Nullability UN> requires (std::derived_from<U,T> && N >= UN)
        WeakRetained(const WeakRetained<U,UN> &r) noexcept          :_ref(_weakRetain(r._ref)) { }
        template <typename U, Nullability UN> requires (std::derived_from<U,T> && N >= UN)
        WeakRetained(WeakRetained<U,UN> &&r) noexcept               :_ref(std::move(r).detach()) { }

        ~WeakRetained() noexcept                                    {if (_ref) _ref->_weakRelease();}

        WeakRetained& operator=(T_ptr t) & noexcept {
            _weakRetain(t);
            std::swap(_ref, t);
            if (t) t->_weakRelease();
            return *this;
        }

        WeakRetained& operator=(std::nullptr_t) & noexcept requires(N==MaybeNull) { // optimized assignment
            auto oldRef = _ref;
            _ref = nullptr;
            if (oldRef) oldRef->_weakRelease();
            return *this;
        }

        WeakRetained& operator=(const WeakRetained &r) & noexcept       {*this = r._ref; return *this;}

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        WeakRetained& operator=(const WeakRetained<U,UN> &r) & noexcept {*this = r._ref; return *this;}

        WeakRetained& operator= (WeakRetained &&r) & noexcept {
            std::swap(_ref, r._ref);   // old _ref will be released by r's destructor
            return *this;
        }

        template <typename U, Nullability UN> requires(std::derived_from<U,T> && N >= UN)
        WeakRetained& operator= (WeakRetained<U,UN> &&r) & noexcept {
            if ((void*)&r != this) {
                auto oldRef = _ref;
                _ref = std::move(r).detach();
                _weakRelease(oldRef);
            }
            return *this;
        }

        /// Converts any WeakRetained to non-nullable form (WeakRef), or throws if its value is nullptr.
        WeakRetained<T,NonNull> asWeakRef() const & noexcept(!N) {return WeakRetained<T,NonNull>(_ref);}

        WeakRetained<T,NonNull> asWeakRef() && noexcept(!N) {
            WeakRetained<T,NonNull> result(_ref, false);
            _ref = nullptr;
            return result;
        }

        /// Returns true if I hold a non-null pointer.
        /// Does _not_ check if the pointed-to object still exists.
        /// @warning  Do not use this to preflight \ref asRetained.
        explicit operator bool () const noexcept FLPURE {return N==NonNull || (_ref != nullptr);}

        /// True if the object no longer exists.
        bool invalidated() const noexcept               {return !_ref || _ref->refCount() == 0;}

        /// If this holds a non-null pointer, and the object pointed to still exists,
        /// returns a `Retained` instance holding a new strong reference to it.
        /// Otherwise returns an empty (nullptr) `Retained`.
        /// @warning You **must** check the `Retained` for null before dereferencing it!
        Retained<T> tryGet() const noexcept {
            if (_ref->_weakToStrong())
                return Retained<T>::adopt(_ref);
            else
                return nullptr;
        }

        /// If this holds a non-null pointer, and the object pointed to still exists,
        /// returns a `Retained` instance holding a new strong reference to it; otherwise throws.
        /// @throws std::illegal_state if the object no longer exists.
        Retained<T,N> get() const {
            if (!_ref || _ref->_weakToStrong())
                return Retained<T,N>::adopt(_ref);
            else [[unlikely]]
                _failZombie(_ref);
        }

        /// Similar to \ref get, will throw if the pointed-to object has been deleted.
        /// @throws std::illegal_state if the object no longer exists.
        Retained<T> operator->() const {return get();}

        /// An alternative to \ref tryGet. If the object pointed to still exists,
        /// calls the function `fn` with a pointer to it, then returns true.
        /// Otherwise just returns false.
        template <std::invocable<T*> FN>
        [[nodiscard]] bool use(FN&& fn) const requires(std::is_void_v<std::invoke_result_t<FN,T*>>) {
            if (auto ref = tryGet()) {
                fn(ref.get());
                return true;
            } else {
                return false;
            }
        }

        /// An alternative to \ref tryGet. If the object pointed to still exists,
        /// calls the function `fn` with a pointer to it, returning whatever it returned.
        /// Otherwise calls `elsefn` with no arguments, returning whatever it returned.
        template <std::invocable<T*> FN, std::invocable<> ELSEFN>
        auto use(FN&& fn, ELSEFN&& elsefn) const {
            if (auto ref = tryGet()) {
                return fn(ref.get());
            } else {
                return elsefn();
            }
        }

    private:
        template <class U, Nullability UN> friend class WeakRetained;

        WeakRetained(T_ptr t, bool) noexcept(N==MaybeNull) // private no-retain ctor
        :_ref(t) {
            if constexpr (N == NonNull) {
                if (t == nullptr) [[unlikely]]
                    _failNullRef();
            }
        }

        static T_ptr _weakRetain(T_ptr t) noexcept {
            if constexpr (N == NonNull) {
                t->_weakRetain(); // this is faster, and it detects illegal null (by signal)
            } else {
                if (t) t->_weakRetain();
            }
            return t;
        }

        static void _weakRelease(T_ptr t) noexcept {
            if constexpr (N == NonNull) {
                t->_weakRelease(); // this is faster, and it detects illegal null (by signal)
            } else {
                if (t) t->_weakRelease();
            }
        }

        [[nodiscard]] T_ptr detach() && noexcept        {return std::exchange(_ref, nullptr);}

        // _ref has to be declared nullable, even when N==NonNull, because a move assignment
        // sets the moved-from _ref to nullptr. The WeakRetained may not used any more in this state,
        // but it will be destructed, which is why the destructor also checks for nullptr.
        T* FL_NULLABLE _ref;
    };

    template <class T> WeakRetained(T* FL_NULLABLE) -> WeakRetained<T>; // deduction guide

    /// WeakRef<T> is an alias for a non-nullable WeakRetained<T>.
    template <class T> using WeakRef = WeakRetained<T, NonNull>;

    /// NullableWeakRef<T> is an alias for a (default) nullable WeakRetained<T>.
    template <class T> using NullableWeakRef = WeakRetained<T, MaybeNull>;


    /** The weak-reference equivalent of \ref RetainedBySubclass. */
    template <class T>
    class WeakRetainedBySubclass {
    public:
        WeakRetainedBySubclass() = default;

        template <std::derived_from<T> Sub>
        explicit WeakRetainedBySubclass(Sub* FL_NULLABLE sub) noexcept
            requires (std::derived_from<Sub,RefCounted>)
            :_ptr{sub}, _ref{sub} { }

        template <std::derived_from<T> Sub, Nullability N>
        explicit WeakRetainedBySubclass(Retained<Sub,N> sub) noexcept
            requires (std::derived_from<Sub,RefCounted>)
            :WeakRetainedBySubclass{sub.get()} { }

        explicit operator bool() const noexcept FLPURE  {return _ptr != nullptr;}

        bool invalidated() const noexcept               {return _ref.invalidated();}

        /// If this holds a non-null pointer, and the object pointed to still exists,
        /// returns a `RetainedBySubclass` instance holding a new strong reference to it.
        /// Otherwise returns an empty (nullptr) result.
        /// @warning You **must** check the result for null before dereferencing it!
        RetainedBySubclass<T> tryGet() const noexcept {
            if (auto strongRef = _ref.tryGet())
                return {_ptr, std::move(strongRef)};
            else
                return {};
        }

        /// An alternative to \ref tryGet. If the object pointed to still exists,
        /// calls the function `fn` with a pointer to it, then returns true.
        /// Otherwise just returns false.
        template <std::invocable<T*> FN>
        bool use(FN&& fn) const requires(std::is_void_v<std::invoke_result_t<FN,T*>>) {
            if (auto ref = tryGet()) {
                fn(ref.get());
                return true;
            } else {
                return false;
            }
        }

        void clear() noexcept                           {_ref = nullptr; _ptr = nullptr;}

    private:
        T* FL_NULLABLE           _ptr = nullptr;
        WeakRetained<RefCounted> _ref;
    };

}

FL_ASSUME_NONNULL_END
