//
// Expert.hh
//
// Copyright 2017-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#ifndef _FLEECE_OBSCURE_HH
#define _FLEECE_OBSCURE_HH
#ifndef _FLEECE_HH
#include "Fleece.hh"
#endif

#include "FLExpert.h"

FL_ASSUME_NONNULL_BEGIN

namespace fleece {

    // VOLATILE API: These methods are meant for internal use, and will be removed
    // in a future release

    //  Rarely-needed or advanced functionality; a C++ wrapper around fleece/FLExpert.h
    //  For documentation, see the comments above the C functions these wrap.


    /** Just a simple wrapper around \ref FLValue_FromData.
        You should generally use a \ref Doc instead; it's safer.*/
    static inline Value ValueFromData(slice data, FLTrust t =kFLUntrusted) {
        return FLValue_FromData(data,t);
    }


    //====== ENCODER:


    /** Encoder subclass that exposes more bells and whistles, most of which are experimental.
        You don't actually instantiate this, you call `expert(enc)` (below) to make a reference. */
    class Encoder_ExpertAPI : public Encoder {
    public:
        Encoder_ExpertAPI() = delete;

        /// Creates an Encoder that writes directly to a file.
        static inline Encoder encodeToFile(FILE *file, bool uniqueStrings =true);

        inline void amend(slice base, bool reuseStrings =false, bool externPointers =false);

        slice base() const                              {return FLEncoder_GetBase(_enc);}

        void suppressTrailer()                          {FLEncoder_SuppressTrailer(_enc);}

        inline void writeRaw(slice data)                {FLEncoder_WriteRaw(_enc, data);}

        size_t bytesWritten() const                     {return FLEncoder_BytesWritten(_enc);}
        size_t nextWritePos() const                     {return FLEncoder_GetNextWritePos(_enc);}

        inline size_t finishItem()                      {return FLEncoder_FinishItem(_enc);}
    };

    // use this to call one of the above methods; e.g. `expert(e).suppressTrailer()`.
    static inline auto& expert(Encoder &enc)        {return (Encoder_ExpertAPI&)enc;}
    static inline auto& expert(const Encoder &enc)  {return (const Encoder_ExpertAPI&)(enc);}


    //====== DELTAS:


    /** Generates and applyies JSON-format deltas/diffs between two Fleece values.
        See <https://github.com/couchbaselabs/fleece/wiki/Advanced-Fleece#5-json-deltas> */
    class JSONDelta {
    public:
        static inline alloc_slice create(Value old, Value nuu);
        static inline bool create(Value old, Value nuu, Encoder &jsonEncoder);

        [[nodiscard]] static inline alloc_slice apply(Value old,
                                                      slice jsonDelta,
                                                      FLError* FL_NULLABLE error);
        /// Writes patched Fleece to the Encoder.
        /// On failure, returns false and sets the Encoder's error property.
        static inline bool apply(Value old,
                                 slice jsonDelta,
                                 Encoder &encoder);
    };


    //====== SHARED KEYS:


    /** Keeps track of a set of dictionary keys that are stored in abbreviated (small integer) form.

        Encoders can be configured to use an instance of this, and will use it to abbreviate keys
        that are given to them as strings. (Note: This class is not thread-safe!)

        See <https://github.com/couchbaselabs/fleece/wiki/Advanced-Fleece#4-shared-keys> */
    class SharedKeys {
    public:
        SharedKeys()                                        :_sk(nullptr) { }
        SharedKeys(FLSharedKeys FL_NULLABLE sk)             :_sk(FLSharedKeys_Retain(sk)) { }
        ~SharedKeys()                                       {FLSharedKeys_Release(_sk);}

        static SharedKeys create()                          {return SharedKeys(FLSharedKeys_New(), 1);}
        static inline SharedKeys create(slice state);
        bool loadState(slice data)                          {return FLSharedKeys_LoadStateData(_sk, data);}
        [[nodiscard]] bool loadState(Value state)           {return FLSharedKeys_LoadState(_sk, state);}
        alloc_slice stateData() const                       {return FLSharedKeys_GetStateData(_sk);}
        inline void writeState(const Encoder &enc);
        unsigned count() const                              {return FLSharedKeys_Count(_sk);}
        void revertToCount(unsigned count)                  {FLSharedKeys_RevertToCount(_sk, count);}
        void disableCaching()                               {if (_sk) FLSharedKeys_DisableCaching(_sk);}

        operator FLSharedKeys FL_NULLABLE () const          {return _sk;}
        bool operator== (SharedKeys other) const            {return _sk == other._sk;}

        SharedKeys(const SharedKeys &other) noexcept        :_sk(FLSharedKeys_Retain(other._sk)) { }
        SharedKeys(SharedKeys &&other) noexcept             :_sk(other._sk) {other._sk = nullptr;}
        inline SharedKeys& operator= (const SharedKeys &other);
        inline SharedKeys& operator= (SharedKeys &&other) noexcept;

    private:
        SharedKeys(FLSharedKeys sk, int)                    :_sk(sk) { }
        FLSharedKeys FL_NULLABLE _sk {nullptr};
    };


    //====== DEPRECATED:


    /** A Dict that manages its own storage. This has been superseded by \ref Doc. */
    class AllocedDict : public Dict, alloc_slice {
    public:
        AllocedDict()
        =default;

        explicit AllocedDict(alloc_slice s)
        :Dict(FLValue_AsDict(FLValue_FromData(s, kFLUntrusted)))
        ,alloc_slice(std::move(s))
        { }

        explicit AllocedDict(slice s)
        :AllocedDict(alloc_slice(s)) { }

        const alloc_slice& data() const                 {return *this;}
        explicit operator bool () const                 {return Dict::operator bool();}

        // MI disambiguation:
        inline Value operator[] (slice key) const       {return Dict::get(key);}
        inline Value operator[] (const char *key) const {return Dict::get(key);}
    };


    //====== IMPLEMENTATION GUNK:

    inline Encoder Encoder_ExpertAPI::encodeToFile(FILE *file, bool uniqueStrings) {
        return Encoder(FLEncoder_NewWritingToFile(file, uniqueStrings));
    }
    inline void Encoder_ExpertAPI::amend(slice base, bool reuseStrings, bool externPointers) {
        FLEncoder_Amend(_enc, base, reuseStrings, externPointers);
    }

    inline alloc_slice JSONDelta::create(Value old, Value nuu) {
        return FLCreateJSONDelta(old, nuu);
    }
    inline bool JSONDelta::create(Value old, Value nuu, Encoder &jsonEncoder) {
        return FLEncodeJSONDelta(old, nuu, jsonEncoder);
    }
    inline alloc_slice JSONDelta::apply(Value old, slice jsonDelta, FLError * FL_NULLABLE error) {
        return FLApplyJSONDelta(old, jsonDelta, error);
    }
    inline bool JSONDelta::apply(Value old,
                                 slice jsonDelta,
                                 Encoder &encoder)
    {
        return FLEncodeApplyingJSONDelta(old, jsonDelta, encoder);
    }

    inline void SharedKeys::writeState(const Encoder &enc) {
        FLSharedKeys_WriteState(_sk, enc);
    }
    inline SharedKeys SharedKeys::create(slice state) {
        auto sk = create();
        sk.loadState(state);
        return sk;
    }
    inline SharedKeys& SharedKeys::operator= (const SharedKeys &other) {
        auto sk = FLSharedKeys_Retain(other._sk);
        FLSharedKeys_Release(_sk);
        _sk = sk;
        return *this;
    }
    inline SharedKeys& SharedKeys::operator= (SharedKeys &&other) noexcept {
        FLSharedKeys_Release(_sk);
        _sk = other._sk;
        other._sk = nullptr;
        return *this;
    }

}

FL_ASSUME_NONNULL_END

#endif // _FLEECE_OBSCURE_HH
