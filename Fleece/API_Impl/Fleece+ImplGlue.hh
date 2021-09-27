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
#include "JSONEncoder.hh"
#include "Path.hh"
#include "DeepIterator.hh"
#include "Doc.hh"
#include "FleeceException.hh"
#include <memory>

using namespace fleece;
using namespace fleece::impl;


namespace fleece { namespace impl {
    struct FLEncoderImpl;
} }

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


namespace fleece { namespace impl {

    void recordError(const std::exception &x, FLError *outError) noexcept;

    #define catchError(OUTERROR) \
        catch (const std::exception &x) { recordError(x, OUTERROR); }

    
    // Implementation of FLEncoder: a subclass of Encoder that keeps track of its error state.
    struct FLEncoderImpl {
        FLError errorCode {::kFLNoError};
        const bool ownsFleeceEncoder {true};
        std::string errorMessage;
        std::unique_ptr<Encoder> fleeceEncoder;
        std::unique_ptr<JSONEncoder> jsonEncoder;
        std::unique_ptr<JSONConverter> jsonConverter;
        void* extraInfo {nullptr};

        FLEncoderImpl(FLEncoderFormat format,
                      size_t reserveSize =0, bool uniqueStrings =true)
        {
            if (reserveSize == 0)
                reserveSize = 256;
            if (format == kFLEncodeFleece) {
                fleeceEncoder.reset(new Encoder(reserveSize));
                fleeceEncoder->uniqueStrings(uniqueStrings);
            } else {
                jsonEncoder.reset(new JSONEncoder(reserveSize));
                jsonEncoder->setJSON5(format == kFLEncodeJSON5);
            }
        }

        FLEncoderImpl(FILE *outputFile, bool uniqueStrings =true) {
            fleeceEncoder.reset(new Encoder(outputFile));
            fleeceEncoder->uniqueStrings(uniqueStrings);
        }

        FLEncoderImpl(Encoder *encoder)
        :ownsFleeceEncoder(false)
        ,fleeceEncoder(encoder)
        { }

        ~FLEncoderImpl() {
            if (!ownsFleeceEncoder)
                fleeceEncoder.release();
        }

        bool isFleece() const {
            return fleeceEncoder != nullptr;
        }

        bool hasError() const {
            return errorCode != ::kFLNoError;
        }

        void recordException(const std::exception &x) noexcept {
            if (!hasError()) {
                fleece::impl::recordError(x, &errorCode);
                errorMessage = x.what();
            }
        }

        void reset() {
            if (fleeceEncoder)
                fleeceEncoder->reset();
            if (jsonConverter)
                jsonConverter->reset();
            if (jsonEncoder) {
                jsonEncoder->reset();
            }
            errorCode = ::kFLNoError;
            extraInfo = nullptr;
        }
    };

    #define ENCODER_DO(E, METHOD) \
        (E->isFleece() ? E->fleeceEncoder->METHOD : E->jsonEncoder->METHOD)

    // Body of an FLEncoder_WriteXXX function:
    #define ENCODER_TRY(E, METHOD) \
        try{ \
            if (!E->hasError()) { \
                ENCODER_DO(E, METHOD); \
                return true; \
            } \
        } catch (const std::exception &x) { \
            E->recordException(x); \
        } \
        return false;


    class FLPersistentSharedKeys : public PersistentSharedKeys {
    public:
        FLPersistentSharedKeys(FLSharedKeysReadCallback callback, void *context)
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
        void* const                     _context;
    };
} }
