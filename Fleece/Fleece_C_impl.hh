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
typedef slice FLSlice;
typedef FLEncoderImpl* FLEncoder;


#include "Fleece.h" /* the C header */


namespace fleece {

    void recordError(const std::exception &x, FLError *outError) noexcept;

    #define catchError(OUTERROR) \
        catch (const std::exception &x) { recordError(x, OUTERROR); }

    
    // Implementation of FLEncoder: a subclass of Encoder that keeps track of its error state.
    struct FLEncoderImpl : public Encoder {
        FLError errorCode {::NoError};
        std::string errorMessage;

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
            errorCode = ::NoError;
        }
    };

}
