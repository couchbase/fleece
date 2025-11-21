//
// Doc.hh
//
// Copyright 2018-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#include "fleece/RefCounted.hh"
#include "Value.hh"
#include "fleece/slice.hh"
#include <atomic>
#include <utility>

namespace fleece { namespace impl {
    class SharedKeys;
    class Value;
    namespace internal {
        class Pointer;
    }


    class Scope {
    public:
        Scope(slice fleeceData,
              SharedKeys*,
              slice externDestination =nullslice,
              bool isDoc =false) noexcept;
        Scope(const alloc_slice &fleeceData,
              SharedKeys*,
              slice externDestination =nullslice,
              bool isDoc =false) noexcept;
        Scope(const Scope &parentScope,
              slice subData,
              bool isDoc =false) noexcept;

        // Scope doesn't need a vtable, but it's useful for identifying subclasses, for example:
        //     auto s = dynamic_cast<const MyScope*>(Scope::containing(val));
        // The dynamic_cast will safely check whether the returned Scope is a MyScope instance.
        virtual ~Scope();

        static const Scope* containing(const Value* NONNULL) noexcept;

        slice data() const FLPURE                      {return _data;}
        alloc_slice allocedData() const FLPURE         {return _alloced;}

        SharedKeys* sharedKeys() const FLPURE          {return _sk;}
        slice externDestination() const FLPURE         {return _externDestination;}

        // For internal use:

        static SharedKeys* sharedKeys(const Value* NONNULL v) noexcept;
        const Value* resolveExternPointerTo(const void* NONNULL) const noexcept;
        static const Value* resolvePointerFrom(const internal::Pointer* NONNULL src,
                                               const void* NONNULL dst) noexcept;
        static std::pair<const Value*,slice> resolvePointerFromWithRange(
                                                                         const internal::Pointer* NONNULL src,
                                                                         const void* NONNULL dst) noexcept;
        static void dumpAll();

    protected:
        static const Scope* _containing(const Value* NONNULL) noexcept;
        void unregister() noexcept;

    private:
        Scope(const Scope&) =delete;
        void registr() noexcept;

        Retained<SharedKeys> _sk;                       // SharedKeys used for this Fleece data
        slice const         _externDestination;         // Extern ptr destination for this data
        slice const         _data;                      // The memory range I represent
        alloc_slice const   _alloced;                   // Retains data if it's an alloc_slice
        std::atomic_flag    _unregistered ATOMIC_FLAG_INIT; // False if registered in sMemoryMap
#if DEBUG
        uint32_t            _dataHash;                  // hash of _data, for troubleshooting
#endif
    protected:
        bool                _isDoc {false};             // True if I am a field of a Doc
        friend class Doc;
    };


    /** A container for Fleece data in memory. Every Value belongs to the Doc whose memory range
        contains it. The Doc keeps track of the SharedKeys used by its Dicts, and where to resolve
        external pointers to. */
    class Doc : public RefCounted, public Scope {
    public:
        enum Trust {
            kUntrusted, kTrusted,
            kDontParse = -1
        };

        explicit
        Doc(const alloc_slice &fleeceData,
            Trust =kUntrusted,
            SharedKeys* =nullptr,
            slice externDest =nullslice) noexcept;

        Doc(const Doc *parentDoc NONNULL,
            slice subData,
            Trust =kUntrusted) noexcept;

        Doc(const Scope &parentScope,
            slice subData,
            Trust =kUntrusted) noexcept;

        static Ref<Doc> fromFleece(const alloc_slice &fleece, Trust =kUntrusted);
        static Ref<Doc> fromJSON(slice json, SharedKeys* =nullptr);

        static RetainedConst<Doc> containing(const Value* NONNULL) noexcept;

        const Value* root() const FLPURE               {return _root;}
        const Dict* asDict() const FLPURE              {return _root ? _root->asDict() : nullptr;}
        const Array* asArray() const FLPURE            {return _root ? _root->asArray() : nullptr;}

        /// Allows client code to associate its own pointer with this Doc and its Values,
        /// which can later be retrieved with \ref getAssociated.
        /// For example, this could be a pointer to an `app::Document` object, of which this Doc's
        /// root Dict is its properties. You would store it by calling
        /// `doc->setAssociatedObject(appDocument, "app::Document")`.
        /// @param pointer  The pointer to store in this Doc.
        /// @param type  A C string literal identifying the type. This is used to avoid collisions
        ///              with unrelated code that might try to store a different type of value.
        /// @return  True if the pointer was stored, false if a pointer of a different type is
        ///          already stored.
        /// @warning  Be sure to clear this before the associated object is freed/invalidated!
        /// @warning  This method is not thread-safe. Do not concurrently get & set objects.
        bool setAssociated(void *pointer, const char *type);

        /// Returns a pointer previously stored in this Doc by \ref setAssociated.
        /// For example, you would look up an `app::Document` object by calliing
        /// `(app::Document*)Doc::containing(value)->associatedObject("app::Document")`.
        /// @param type  The type of object expected, i.e. the same string literal passed to the
        ///              \ref setAssociatedObject method.
        /// @return  The associated pointer of that type, if any.
        void* getAssociated(const char *type) const;

    protected:
        virtual ~Doc() =default;

    private:
        void init(Trust) noexcept;

        const Value*        _root {nullptr};            // The root object of the Fleece
        RetainedConst<Doc>  _parent;
        void*               _associatedPointer {nullptr};
        const char*         _associatedType {nullptr};
    };

} }


extern "C" {
    // For debugging only; to be called from a debugger
    void FLDumpScopes();
}
