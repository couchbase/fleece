//
// ExternResolver.hh
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"

namespace fleece {
    class Value;
    
    /** Registers a way to resolve extern pointers in a specific Fleece document.
        While an ExternResolver is in scope, it will be used whenever an extern pointer is
        dereferenced in the memory range of its document. This allows a delta document to be
        logically appended to the base document, without having to actually concatenate the
        two documents in memory. */
    class ExternResolver {
    public:
        /** Constructs a resolver for a Fleece document in memory. Extern pointers in it will be
            mapped into the `destination` as though the `destination` preceded the `document` in
            memory. */
        ExternResolver(slice document, slice destination);

        ~ExternResolver();

        /** The source document for which this instance provides resolution. */
        slice source() const                    {return _document;}

        /** The destination document that pointers will end up in. */
        slice destination() const               {return _destinationDoc;}

        /** Resolves a pointer that's already known to come from this document.
            @param address  The unresolved destination of the pointer, i.e. where it would point
                            to without any fixing up. This is of course a bogus address.
            @return  The resolved address, which must lie within the destination doc, or null. */
        const Value* resolvePointerTo(const void* address) const;

        /** Finds an in-scope resolver for the given source address, or null if none. */
        static const ExternResolver* resolverForPointerFrom(const void *src);

        /** Resolves a pointer at `src` whose unresolved destination is `dst`. */
        static const Value* resolvePointerFrom(const void* src, const void *dst);

    private:
        const slice _document;
        const slice _destinationDoc;
    };

}
