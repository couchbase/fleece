//
//  Fleece_C_impl.hh
//  Fleece
//
//  Created by Jens Alfke on 9/19/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "Fleece.hh"
#include "FleeceException.hh"
using namespace fleece;

namespace fleece {
    struct FLEncoderImpl;
}

#define FL_IMPL
typedef const Value* FLValue;
typedef const Array* FLArray;
typedef const Dict* FLDict;
typedef struct {
    const void* buf;
    size_t size;

    operator slice() const { return *((slice*)this); }
} FLSlice;
typedef FLEncoderImpl* FLEncoder;
typedef SharedKeys* FLSharedKeys;

// A convenient constant denoting a null slice.
#ifdef _MSC_VER
const FLSlice kFLSliceNull = { NULL, 0 };
#else
#define kFLSliceNull ((FLSlice){NULL, 0})
#endif


#include "Fleece.h" /* the C header */


namespace fleece {

    void recordError(const std::exception &x, FLError *outError) noexcept;

    #define catchError(OUTERROR) \
        catch (const std::exception &x) { recordError(x, OUTERROR); }

    
    // Implementation of FLEncoder: a subclass of Encoder that keeps track of its error state.
    struct FLEncoderImpl : public Encoder {
        FLError errorCode {::NoError};
        std::string errorMessage;
        std::unique_ptr<JSONConverter> jsonConverter {nullptr};

        FLEncoderImpl(size_t reserveOutputSize =256) :Encoder(reserveOutputSize) { }

        bool hasError() const {
            return errorCode != ::NoError;
        }

        void recordException(const std::exception &x) noexcept {
            if (!hasError()) {
                fleece::recordError(x, &errorCode);
                errorMessage = x.what();
            }
        }

        void reset() {              // careful, not a real override (non-virtual method)
            Encoder::reset();
            if (jsonConverter)
                jsonConverter->reset();
            errorCode = ::NoError;
        }
    };

}
