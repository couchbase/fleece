//
// FLEncoder.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "Fleece+ImplGlue.hh"
#include "Builder.hh"

#define ENCODER_DO(E, METHOD)   (E)->do_([&](auto& e) {return e.METHOD;})

#define ENCODER_TRY(E, METHOD)  return (E)->try_([&](auto e) { ENCODER_DO(e, METHOD); return true;})

FLEncoder FLEncoder_New(void) FLAPI {
    return FLEncoder_NewWithOptions(kFLEncodeFleece, 0, true);
}

FLEncoder FLEncoder_NewWithOptions(FLEncoderFormat format,
                                   size_t reserveSize, bool uniqueStrings) FLAPI
{
    return new FLEncoderImpl(format, reserveSize, uniqueStrings);
}

FLEncoder FLEncoder_NewWritingToFile(FILE *outputFile, bool uniqueStrings) FLAPI {
    return new FLEncoderImpl(outputFile, uniqueStrings);
}

void FLEncoder_Reset(FLEncoder e) FLAPI {
    e->reset();
}

void FLEncoder_Free(FLEncoder FL_NULLABLE e) FLAPI {
    delete e;
}

void FLEncoder_SetSharedKeys(FLEncoder e, FLSharedKeys FL_NULLABLE sk) FLAPI {
    if (e->isFleece())
        e->fleeceEncoder()->setSharedKeys(sk);
}

void FLEncoder_SuppressTrailer(FLEncoder e) FLAPI {
    if (e->isFleece())
        e->fleeceEncoder()->suppressTrailer();
}

void FLEncoder_Amend(FLEncoder e, FLSlice base, bool reuseStrings, bool externPointers) FLAPI {
    if (e->isFleece() && base.size > 0) {
        e->fleeceEncoder()->setBase(base, externPointers);
        if(reuseStrings)
            e->fleeceEncoder()->reuseBaseStrings();
    }
}

FLSlice FLEncoder_GetBase(FLEncoder e) FLAPI {
    if (e->isFleece())
        return e->fleeceEncoder()->base();
    return {};
}

size_t FLEncoder_GetNextWritePos(FLEncoder e) FLAPI {
    if (e->isFleece())
        return e->fleeceEncoder()->nextWritePos();
    return 0;
}

size_t FLEncoder_BytesWritten(FLEncoder e) FLAPI {
    return ENCODER_DO(e, bytesWritten());
}

intptr_t FLEncoder_LastValueWritten(FLEncoder e) FLAPI {
    return e->isFleece() ? intptr_t(e->fleeceEncoder()->lastValueWritten()) : kFLNoWrittenValue;
}

bool FLEncoder_WriteValueAgain(FLEncoder e, intptr_t prewritten) FLAPI {
    return e->isFleece() && e->fleeceEncoder()->writeValueAgain(Encoder::PreWrittenValue(prewritten));
}

bool FLEncoder_WriteNull(FLEncoder e)              FLAPI {ENCODER_TRY(e, writeNull());}
bool FLEncoder_WriteUndefined(FLEncoder e)         FLAPI {ENCODER_TRY(e, writeUndefined());}
bool FLEncoder_WriteBool(FLEncoder e, bool b)      FLAPI {ENCODER_TRY(e, writeBool(b));}
bool FLEncoder_WriteInt(FLEncoder e, int64_t i)    FLAPI {ENCODER_TRY(e, writeInt(i));}
bool FLEncoder_WriteUInt(FLEncoder e, uint64_t u)  FLAPI {ENCODER_TRY(e, writeUInt(u));}
bool FLEncoder_WriteFloat(FLEncoder e, float f)    FLAPI {ENCODER_TRY(e, writeFloat(f));}
bool FLEncoder_WriteDouble(FLEncoder e, double d)  FLAPI {ENCODER_TRY(e, writeDouble(d));}
bool FLEncoder_WriteString(FLEncoder e, FLSlice s) FLAPI {ENCODER_TRY(e, writeString(s));}
bool FLEncoder_WriteDateString(FLEncoder e, FLTimestamp ts, bool asUTC)
                                                   FLAPI {ENCODER_TRY(e, writeDateString(ts,asUTC));}
bool FLEncoder_WriteData(FLEncoder e, FLSlice d)   FLAPI {ENCODER_TRY(e, writeData(d));}
bool FLEncoder_WriteRaw(FLEncoder e, FLSlice r)    FLAPI {ENCODER_TRY(e, writeRaw(r));}
bool FLEncoder_WriteValue(FLEncoder e, FLValue v)  FLAPI {ENCODER_TRY(e, writeValue(v));}

bool FLEncoder_WriteFormatted(FLEncoder e, const char* format, ...) FLAPI {
    va_list args;
    va_start(args, format);
    bool ok = FLEncoder_WriteFormattedArgs(e, format, args);
    va_end(args);
    return ok;
}

bool FLEncoder_WriteFormattedArgs(FLEncoder e, const char* format, va_list args) FLAPI {
    return e->try_([&](auto impl) {
        impl->do_([&](auto& enc) { builder::VEncode(enc, format, args); });
        return true;
    });
}

bool FLEncoder_BeginArray(FLEncoder e, size_t reserve)  FLAPI {ENCODER_TRY(e, beginArray(reserve));}
bool FLEncoder_EndArray(FLEncoder e)                    FLAPI {ENCODER_TRY(e, endArray());}
bool FLEncoder_BeginDict(FLEncoder e, size_t reserve)   FLAPI {ENCODER_TRY(e, beginDictionary(reserve));}
bool FLEncoder_WriteKey(FLEncoder e, FLSlice s)         FLAPI {ENCODER_TRY(e, writeKey(s));}
bool FLEncoder_WriteKeyValue(FLEncoder e, FLValue key)  FLAPI {ENCODER_TRY(e, writeKey(key));}
bool FLEncoder_EndDict(FLEncoder e)                     FLAPI {ENCODER_TRY(e, endDictionary());}

void FLJSONEncoder_NextDocument(FLEncoder e) FLAPI {
    if (!e->isFleece())
        e->jsonEncoder()->nextDocument();
}

bool FLEncoder_ConvertJSON(FLEncoder e, FLSlice json) FLAPI {
    return e->try_([&](auto impl) { return impl->encodeJSON(json); });
}

FLError FLEncoder_GetError(FLEncoder e) FLAPI {
    return e->errorCode;
}

const char* FL_NULLABLE FLEncoder_GetErrorMessage(FLEncoder e) FLAPI {
    return e->errorMessage.empty() ? nullptr : e->errorMessage.c_str();
}

void FLEncoder_SetExtraInfo(FLEncoder e, void* FL_NULLABLE info) FLAPI {
    e->extraInfo = info;
}

void* FL_NULLABLE FLEncoder_GetExtraInfo(FLEncoder e) FLAPI {
    return e->extraInfo;
}

FLSliceResult FLEncoder_Snip(FLEncoder e) FLAPI {
    if (e->isFleece())
        return FLSliceResult(e->fleeceEncoder()->snip());
    else
        return {};
}

size_t FLEncoder_FinishItem(FLEncoder e) FLAPI {
    if (e->isFleece())
        return e->fleeceEncoder()->finishItem();
    return 0;
}

FLDoc FL_NULLABLE FLEncoder_FinishDoc(FLEncoder e, FLError * FL_NULLABLE outError) FLAPI {
    if (auto enc = e->fleeceEncoder()) {
        if (!e->hasError()) {
            try {
                return retain(enc->finishDoc());       // finish() can throw
            } catch (const std::exception &x) {
                e->recordException(x);
            }
        }
    } else {
        e->errorCode = kFLUnsupported;  // Doc class doesn't support JSON data
    }
    // Failure:
    if (outError)
        *outError = e->errorCode;
    e->reset();
    return nullptr;
}


FLSliceResult FLEncoder_Finish(FLEncoder e, FLError * FL_NULLABLE outError) FLAPI {
    if (!e->hasError()) {
        try {
            return FLSliceResult(ENCODER_DO(e, finish()));       // finish() can throw
        } catch (const std::exception &x) {
            e->recordException(x);
        }
    }
    // Failure:
    if (outError)
        *outError = e->errorCode;
    e->reset();
    return {nullptr, 0};
}
