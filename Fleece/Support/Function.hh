//
//  Function.hh
//  Fleece
//
//  Copyright © 2020 Couchbase. All rights reserved.
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

#if __has_include(<any>)
#include <any>
#include <memory>
#include <type_traits>
#include <utility>

namespace fleece {

    template <typename Fn> class Function;

    /// A function object that can be instantiated from a lambda.
    /// It copies the lambda, so it remains valid after the caller has returned.
    /// (Comparable to std::function, but faster.)
    template <typename Ret, typename ...Params>
    class Function<Ret(Params...)> {
    public:
        /// Construct a Function from a lambda or other callable object.
        template <typename Lambda>
        Function(Lambda &&lambda,
                 typename std::enable_if<
                    !std::is_same<typename std::remove_reference<Lambda>::type,
                                  Function>::value>::type * = nullptr)
        :_storage(new std::any(std::move(lambda)))
        ,_trampoline(&trampoline<typename std::remove_reference<Lambda>::type>)
        ,_receiver(reinterpret_cast<intptr_t>(
                     std::any_cast<typename std::remove_reference<Lambda>::type>(_storage.get()) ))
        { }

        Function() noexcept                             { }
        Function(std::nullptr_t) noexcept               { }
        Function(Function &&other) noexcept             {*this = std::move(other);}

        Function(const Function &other) =delete;
        Function& operator= (const Function &other) =delete;

        Function& operator= (Function &&other) noexcept {
            _storage = std::move(other._storage);
            _trampoline = other._trampoline;
            _receiver = other._receiver;
            // Make `other` fail fast if called again:
            other._trampoline = nullptr;
            other._receiver = 0;
            return *this;
        }

        /// a Function tests as `true` if it has a function. It's equal to nullptr if it has none.
        explicit operator bool() const noexcept         {return _trampoline != nullptr;}
        bool operator== (std::nullptr_t) const noexcept {return _trampoline == nullptr;}
        bool operator!= (std::nullptr_t) const noexcept {return _trampoline != nullptr;}

        /// The function-call operator.
        Ret operator()(Params ...params) const {
            return _trampoline(_receiver, std::forward<Params>(params)...);
        }

    private:
        // calls the Lambda object through a type-erased pointer to it
        template <typename Lambda>
        static Ret trampoline(intptr_t rcvr, Params ...params) {
            return (*reinterpret_cast<Lambda*>(rcvr))( std::forward<Params>(params)... );
        }

        using TrampolinePtr = Ret (*)(intptr_t callable, Params ...);

        std::unique_ptr<std::any> _storage;              // The lambda object is stored as an 'any'
        intptr_t                  _receiver {0};         // Type-erased ptr to the lambda object
        TrampolinePtr             _trampoline {nullptr}; // C function ptr; lambda's entry point

        // TODO: It would be nice to avoid storing the lambda on the heap (in _storage.)
        // Unfortunately, _receiver points into the storage, so when copying/moving _storage,
        // it's unclear how to copy _receiver, because it might or might not need to be moved.
    };
}

#else
// w/o C++17, just make Function an alias for std::function

#include <functional>
namespace fleece {
    template <typename T> using Function = std::function<T>;
}

#endif // __has_include(<any>)
