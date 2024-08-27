//
// Fleece+ImplGlue.hh
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
#include "FleeceImpl.hh"
#include "ValueSlot.hh"
#include "Encoder.hh"
#include "JSONEncoder.hh"
#include "Path.hh"
#include "DeepIterator.hh"
#include "Doc.hh"
#include "FleeceException.hh"
#include <memory>
#include <variant>

using namespace fleece;
using namespace fleece::impl;


namespace fleece::impl {
    class FLEncoderImpl;
}

// Define the public types as typedefs of the impl types:
typedef const Value*    FLValue;
typedef const Array*    FLArray;
typedef const Dict*     FLDict;
typedef ValueSlot*      FLSlot;
typedef MutableArray*   FLMutableArray;
typedef MutableDict*    FLMutableDict;
typedef FLEncoderImpl*  FLEncoder;
typedef SharedKeys*     FLSharedKeys;
typedef Path*           FLKeyPath;
typedef DeepIterator*   FLDeepIterator;
typedef const Doc*      FLDoc;

#define FL_IMPL         // Prevents redefinition of the above types

#include "fleece/Fleece.h" /* the public C header */
#include "fleece/FLExpert.h"


FL_ASSUME_NONNULL_BEGIN

namespace fleece::impl {

    void recordError(const std::exception &x, FLError* FL_NULLABLE outError) noexcept;

    #define catchError(OUTERROR) \
        catch (const std::exception &x) { recordError(x, OUTERROR); }


    /** A wrapper around Encoder and JSONEncoder that can write either. Catches exceptions.
        The public type `FLEncoder` is a pointer to this. */
    class FLEncoderImpl {
    public:
        FLEncoderImpl(FLEncoderFormat format,
                      size_t reserveSize =0, bool uniqueStrings =true)
        :_encoder(mkEncoder(format, reserveSize ? reserveSize : 256))
        {
            if (format == kFLEncodeFleece)
                fleeceEncoder()->uniqueStrings(uniqueStrings);
            else
                jsonEncoder()->setJSON5(format == kFLEncodeJSON5);
        }

        explicit FLEncoderImpl(FILE *outputFile, bool uniqueStrings =true)
        :_encoder(outputFile)
        {
            std::get<Encoder>(_encoder).uniqueStrings(uniqueStrings);
        }

        explicit FLEncoderImpl(Encoder* fleeceEncoder)
        :_encoder(fleeceEncoder)
        { }

        Encoder* FL_NULLABLE fleeceEncoder() {
            switch (_encoder.index()) {
                case 0: return std::get_if<Encoder>(&_encoder);
                case 2: return std::get<Encoder*>(_encoder);
                default: return nullptr;
            }
        }

        JSONEncoder* FL_NULLABLE jsonEncoder()    {
            return std::get_if<JSONEncoder>(&_encoder);
        }

        bool isFleece() const noexcept {return _encoder.index() == 0;}
        bool hasError() const noexcept {return errorCode != ::kFLNoError;}

        bool encodeJSON(slice json) {
            if (isFleece()) {
                if (_jsonConverter)
                    _jsonConverter->reset();
                else
                    _jsonConverter = std::make_unique<JSONConverter>(*fleeceEncoder());
                if (_jsonConverter->encodeJSON(json)) {                   // encodeJSON can throw
                    return true;
                } else {
                    errorCode = (FLError)_jsonConverter->errorCode();
                    errorMessage = _jsonConverter->errorMessage();
                    _jsonConverter = nullptr;
                    return false;
                }
            } else {
                jsonEncoder()->writeJSON(json);
                return true;
            }
        }

        /// Invokes a callback. If it throws, catches the error, sets my error and returns false.
        template <typename CALLBACK>
        bool try_(CALLBACK const& callback) noexcept {
            if (!hasError()) {
                try {
                    return callback(this);
                } catch (const std::exception &x) {
                    recordException(x);
                }
            }
            return false;
        }

        /// Invokes a callback on the underlying encoder. Callback parameter is a ref to the Encoder or JSONEncoder.
        template <typename CALLBACK>
        auto do_(CALLBACK const& callback) {
            return std::visit([&](auto& e) {
                if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(e)>>)
                    return callback(*e);
                else
                    return callback(e);
            }, _encoder);
        }

        /// Combination of `try_` and `do_`.
        template <typename CALLBACK>
        bool try_do_(CALLBACK const& callback) {
            return try_([&](auto self) {return self->do_(callback);});
        }

        void reset() noexcept {
            do_([](auto& e) {e.reset();});
            if (_jsonConverter)
                _jsonConverter->reset();
            errorCode = ::kFLNoError;
            extraInfo = nullptr;
        }

        void recordException(const std::exception &x) noexcept {
            if (!hasError()) {
                errorCode = FLError(FleeceException::getCode(x));
                errorMessage = x.what();
            }
        }

        FLError                         errorCode {::kFLNoError};
        std::string                     errorMessage;
        void* FL_NULLABLE               extraInfo {nullptr};

    private:
        using VariEnc = std::variant<Encoder,JSONEncoder,Encoder*>;

        static VariEnc mkEncoder(FLEncoderFormat format, size_t reserveSize) {
            if (format == kFLEncodeFleece)
                return VariEnc(std::in_place_type<Encoder>, reserveSize);
            else
                return VariEnc(std::in_place_type<JSONEncoder>, reserveSize);
        }

        VariEnc                         _encoder;
        std::unique_ptr<JSONConverter>  _jsonConverter;
    };


    class FLPersistentSharedKeys : public PersistentSharedKeys {
    public:
        FLPersistentSharedKeys(FLSharedKeysReadCallback callback, void * FL_NULLABLE context)
        :_callback(callback)
        ,_context(context)
        { }

        virtual bool read() override {
            return _callback(_context, this);
        }

        virtual void write(slice encodedData) override {
            // this is only called by save(), which we never call.
            abort();
        }

    private:
        FLSharedKeysReadCallback const  _callback;
        void* FL_NULLABLE const         _context;
    };
} 

FL_ASSUME_NONNULL_END
