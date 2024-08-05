//
// slice.hh
//
// Copyright 2014-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#pragma once
#ifndef _FLEECE_SLICE_HH
#define _FLEECE_SLICE_HH

#include "FLSlice.h"
#include <algorithm>            // for std::min()
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>              // for fputs()
#include <string.h>             // for memcpy(), memcmp()
#include <string>

#ifndef assert
#   include <assert.h>
#endif
#   ifndef assert_precondition
#   define assert_precondition(e) assert(e)
#endif

#ifdef __APPLE__
    struct __CFString;
    struct __CFData;
#   ifdef __OBJC__
        @class NSData;
        @class NSString;
        @class NSMapTable;
#   endif
#endif

// Figure out whether and how string_view is available
#ifdef __has_include
#   if __has_include(<string_view>)
#       include <string_view>
#       define SLICE_SUPPORTS_STRING_VIEW
#   endif
#endif


// Utility for using slice with printf-style formatting.
// Use "%.*" in the format string; then for the corresponding argument put FMTSLICE(theslice).
// NOTE: The argument S will be evaluated twice.
#define FMTSLICE(S)    (int)(S).size, (const char*)(S).buf


FL_ASSUME_NONNULL_BEGIN


namespace fleece {
    struct slice;
    struct alloc_slice;
    struct nullslice_t;

#ifdef SLICE_SUPPORTS_STRING_VIEW
    using string_view = std::string_view; // create typedef with correct namespace
#endif

#ifdef __APPLE__
    using CFStringRef = const struct ::__CFString *;
    using CFDataRef   = const struct ::__CFData *;
#endif
    
    /** Adds a byte offset to a pointer. */
    template <typename T>
    FLCONST constexpr14 inline const T* FL_NONNULL offsetby(const T * FL_NONNULL t, ptrdiff_t offset) noexcept {
        return (const T*)((uint8_t*)t + offset);
    }

    /** Adds a byte offset to a pointer. */
    template <typename T>
    FLCONST constexpr14 inline T* FL_NONNULL offsetby(T * FL_NONNULL t, ptrdiff_t offset) noexcept {
        return (T*)((uint8_t*)t + offset);
    }

    /** Subtracts the 2nd pointer from the 1st, returning the difference in addresses. */
    FLCONST constexpr inline ptrdiff_t _pointerDiff(const void* FL_NULLABLE a, const void* FL_NULLABLE b) noexcept {
        return (uint8_t*)a - (uint8_t*)b;
    }

    /** Subtracts the 2nd pointer from the 1st, returning the difference in addresses. */
    FLCONST constexpr inline ptrdiff_t pointerDiff(const void* a, const void* b) noexcept {
        return _pointerDiff(a, b);
    }


#pragma mark - PURE_SLICE:


    /** Abstract superclass of `slice` and `alloc_slice`.
        A simple pointer to a range of memory: `size` bytes starting at address `buf`.

        \note Not generally used directly; instead, use subclasses \ref slice and \ref alloc_slice.
        `pure_slice` mostly serves to factor out their common API.

        * `buf` may be NULL, but only if `size` is zero; this is called `nullslice`.
        * `size` may be zero with a non-NULL `buf`; that's called an "empty slice".
        * **No ownership is implied!** Just like a regular pointer, it's the client's responsibility
          to ensure the memory buffer remains valid. The `alloc_slice` subclass does provide
          ownership: it manages a ref-counted heap-allocated buffer.
        * Instances are immutable: `buf` and `size` cannot be changed. The `slice` subclass
          changes this.
        * The memory pointed to cannot be modified through this class. `slice` has some
          methods that allow writes. */
    struct pure_slice {
        const void* FL_NULLABLE const buf;
        size_t                  const size;

        pure_slice(const pure_slice &) noexcept = default;
        /// True if the slice's length is zero.
        bool empty() const noexcept FLPURE                          {return size == 0;}

        /// Testing a slice as a bool results in false for nullslice, true for anything else.
        explicit operator bool() const noexcept FLPURE              {return buf != nullptr;}

        // These methods allow iterating a slice's bytes with a `for(:)` loop:
        constexpr const uint8_t* FL_NULLABLE begin() const noexcept FLPURE      {return (uint8_t*)buf;}
        constexpr const uint8_t* FL_NULLABLE end() const noexcept FLPURE        {return begin() + size;}

        /// Returns true if the address is within this slice or equal to its \ref end.
        inline bool validAddress(const void * FL_NULLABLE addr) const noexcept FLPURE;

        /// Returns true if the byte at this address is in this slice; does _not_ include \ref end.
        inline bool containsAddress(const void * FL_NULLABLE addr) const noexcept FLPURE;

        /// Returns true if the given slice is a subset of me.
        inline bool containsAddressRange(pure_slice) const noexcept FLPURE;

        const void* offset(size_t o) const noexcept FLPURE;
        size_t offsetOf(const void* ptr) const noexcept FLPURE;

        inline const uint8_t& operator[](size_t i) const noexcept FLPURE;
        inline slice operator()(size_t i, size_t n) const noexcept FLPURE;

        inline slice upTo(const void* pos) const noexcept FLPURE;
        inline slice from(const void* pos) const noexcept FLPURE;
        inline slice upTo(size_t offset) const noexcept FLPURE;
        inline slice from(size_t offset) const noexcept FLPURE;

        inline bool containsBytes(pure_slice bytes) const noexcept FLPURE;
        inline slice find(pure_slice target) const noexcept FLPURE;
        inline const uint8_t* FL_NULLABLE findByte(uint8_t b) const FLPURE;
        inline const uint8_t* FL_NULLABLE findByteOrEnd(uint8_t byte) const noexcept FLPURE;
        inline const uint8_t* FL_NULLABLE findAnyByteOf(pure_slice targetBytes) const noexcept FLPURE;
        inline const uint8_t* FL_NULLABLE findByteNotIn(pure_slice targetBytes) const noexcept FLPURE;

        inline int compare(pure_slice s) const noexcept FLPURE    {return FLSlice_Compare(*this,s);}
        inline int caseEquivalentCompare(pure_slice) const noexcept FLPURE;
        inline bool caseEquivalent(pure_slice) const noexcept FLPURE;

        // Relational operators, implemented with FLSlice_Equal and compare():

        bool operator==(const pure_slice &s) const noexcept FLPURE  {return FLSlice_Equal(*this,s);}
        bool operator!=(const pure_slice &s) const noexcept FLPURE  {return !(*this == s);}
        bool operator<(pure_slice s) const noexcept FLPURE          {return compare(s) < 0;}
        bool operator>(pure_slice s) const noexcept FLPURE          {return compare(s) > 0;}
        bool operator<=(pure_slice s) const noexcept FLPURE         {return compare(s) <= 0;}
        bool operator>=(pure_slice s) const noexcept FLPURE         {return compare(s) >= 0;}

        inline bool hasPrefix(pure_slice) const noexcept FLPURE;
        inline bool hasSuffix(pure_slice) const noexcept FLPURE;
        bool hasPrefix(uint8_t b) const noexcept FLPURE   {return size > 0 && (*this)[0] == b;}
        bool hasSuffix(uint8_t b) const noexcept FLPURE   {return size > 0 && (*this)[size-1] == b;}

        /** Computes a 32-bit non-cryptographic hash of the slice's contents. */
        uint32_t hash() const noexcept FLPURE       {return FLSlice_Hash(*this);}

        /// Copies my contents to memory starting at `dst`, using `memcpy`.
        void copyTo(void *dst) const noexcept       {FLMemCpy(dst, buf, size);}

        /// Returns new malloc'ed slice containing same data. Call free() on it when done.
        inline slice copy() const;

        // String conversions:

        explicit operator std::string() const       {return std::string((const char*)buf, size);}
        std::string asString() const                {return (std::string)*this;}

        std::string hexString() const;

        /** Copies into a C string buffer of the given size. Result is always NUL-terminated and
            will not overflow the buffer. Returns false if the slice was truncated. */
        inline bool toCString(char *buf, size_t bufSize) const noexcept;

        // FLSlice interoperability:
        constexpr operator FLSlice () const noexcept {return {buf, size};}

#ifdef SLICE_SUPPORTS_STRING_VIEW
        // std::string_view interoperability:
        constexpr pure_slice(string_view str) noexcept            :pure_slice(str.data(), str.length()) {}
        operator string_view() const noexcept STEPOVER {return string_view((const char*)buf, size);}
#endif

#ifdef __APPLE__
        // Implementations in slice+CoreFoundation.cc and slice+ObjC.mm
        explicit pure_slice(CFDataRef FL_NULLABLE data) noexcept;
        CFStringRef createCFString() const;
        CFDataRef createCFData() const;
#   ifdef __OBJC__
        explicit pure_slice(NSData* FL_NULLABLE data) noexcept;
        NSData* copiedNSData() const;
        /** Creates an NSData using initWithBytesNoCopy and freeWhenDone:NO.
            The data is not copied and does not belong to the NSData object, so make sure it
            remains valid for the lifespan of that object!. */
        NSData* uncopiedNSData() const;
        NSString* asNSString() const;
        NSString* asNSString(NSMapTable* FL_NULLABLE sharedStrings) const;
#   endif
#endif

        constexpr pure_slice(std::nullptr_t) noexcept             :pure_slice() {}
        constexpr pure_slice(const char* FL_NULLABLE str) noexcept :buf(str), size(_strlen(str)) {}
        pure_slice(const std::string& str) noexcept               :buf(&str[0]), size(str.size()) {}

        // Raw memory allocation. These throw std::bad_alloc on failure.
        RETURNS_NONNULL inline static void* newBytes(size_t sz);
        template <typename T> RETURNS_NONNULL
            static inline T* FL_NONNULL reallocBytes(T* FL_NULLABLE bytes, size_t newSz);

    protected:
        constexpr pure_slice() noexcept                           :buf(nullptr), size(0) {}
        inline constexpr pure_slice(const void* FL_NULLABLE b, size_t s) noexcept;

        inline void setBuf(const void *b) noexcept;
        inline void setSize(size_t s) noexcept;
        inline void set(const void * FL_NULLABLE, size_t) noexcept;

        // (Assignment must be custom because `buf` is declared as const/immutable.)
        pure_slice& operator=(const pure_slice &s) & noexcept  {set(s.buf, s.size); return *this;}
        static inline constexpr size_t _strlen(const char* FL_NULLABLE str) noexcept FLPURE;
        // Throws std::bad_alloc, or if exceptions are disabled calls std::terminate()
        [[noreturn]] static void failBadAlloc();
        // Sanity-checks `buf` and `size`
        inline constexpr void checkValidSlice() const;
        // Calls `assert_precondition(validAddress(addr))`, then returns `addr`
        inline const void* check(const void *addr) const;
        // Calls `assert_precondition(offset <= size)`, then returns `addr`
        inline size_t check(size_t offset) const;
    };


#pragma mark - SLICE:


    /** A simple pointer to a range of memory: `size` bytes starting at address `buf`.
        \warning **No ownership is implied!** Just like a regular pointer, it's the client's
                 responsibility to ensure the memory buffer remains valid.
        Some invariants:
        * `buf` may be NULL, but only if `size` is zero; this is called `nullslice`.
        * `size` may be zero with a non-NULL `buf`; that's called an "empty slice". */
    struct slice : public pure_slice {
        constexpr slice() noexcept STEPOVER                           :pure_slice() {}
        constexpr slice(std::nullptr_t) noexcept STEPOVER             :pure_slice() {}
        inline constexpr slice(nullslice_t) noexcept STEPOVER;
        constexpr slice(const void* FL_NULLABLE b, size_t s) noexcept STEPOVER    :pure_slice(b, s) {}
        inline constexpr slice(const void* start, const void* end) noexcept STEPOVER;
        inline constexpr slice(const alloc_slice&) noexcept STEPOVER;

        slice(const std::string& str) noexcept STEPOVER               :pure_slice(str) {}
        constexpr slice(const char* FL_NULLABLE str) noexcept STEPOVER  :pure_slice(str) {}

        slice& operator= (const char* s) & noexcept           {return *this = slice(s);}
        slice& operator= (const std::string &s) & noexcept    {return *this = slice(s);}
        slice& operator= (const alloc_slice &s) & noexcept    {return *this = slice(s);}
        slice& operator= (std::nullptr_t) & noexcept          {set(nullptr, 0); return *this;}
        inline slice& operator= (nullslice_t) & noexcept;

        /// Sets `size`.
        void setSize(size_t s) noexcept                     {pure_slice::setSize(s);}
        /// Sets `size`; asserts that the new size is not larger.
        inline void shorten(size_t s);

        /// Adjusts `size` so that \ref end returns the given value.
        void setEnd(const void* e) noexcept         {setSize(pointerDiff(e, buf));}
        /// Sets `buf` without moving the end, adjusting `size` accordingly.
        inline void setStart(const void* s) noexcept;
        /// Moves `buf` without moving the end, adjusting `size` accordingly.
        void moveStart(ptrdiff_t delta) noexcept            {set(offsetby(buf, delta), size - delta);}
        /// Like \ref moveStart but returns false if the move is illegal.
        bool checkedMoveStart(size_t delta) noexcept        {if (size<delta) return false;
                                                             else {moveStart(delta); return true;}}
        // FLSlice interoperability:
        constexpr slice(const FLSlice &s) noexcept STEPOVER           :pure_slice(s.buf,s.size) { }
        inline explicit operator FLSliceResult () const noexcept;
        explicit slice(const FLSliceResult &sr) STEPOVER            :pure_slice(sr.buf, sr.size) { }
        slice& operator= (FLHeapSlice s) & noexcept           {set(s.buf, s.size); return *this;} // disambiguation

#ifdef SLICE_SUPPORTS_STRING_VIEW
        constexpr slice(string_view str) noexcept STEPOVER            :pure_slice(str) {}
#endif

#ifdef __APPLE__
        explicit slice(CFDataRef data) noexcept                       :pure_slice(data) {}
#   ifdef __OBJC__
        explicit slice(NSData* data) noexcept                         :pure_slice(data) {}
#   endif
#endif

        slice& operator= (std::string&&) =delete;   // Disallowed: might lead to ptr to freed buf
        slice& operator= (alloc_slice&&) =delete;   // Disallowed: might lead to ptr to freed buf
    };


    /** An awkwardly unrelated struct for when the bytes need to be writeable. */
    struct mutable_slice {
        void* FL_NULLABLE buf;
        size_t size;

        constexpr mutable_slice() noexcept                      :buf(nullptr), size(0) {}
        explicit constexpr mutable_slice(pure_slice s) noexcept :buf((void*)s.buf), size(s.size) {}
        constexpr mutable_slice(void* FL_NULLABLE b, size_t s) noexcept     :buf(b), size(s) {}
        constexpr mutable_slice(void* b, void* e) noexcept      :buf(b),
                                                                size(pointerDiff(e, b)) {}

        operator slice() const noexcept                         {return slice(buf, size);}

        /// Securely zeroes the bytes; use this for passwords or encryption keys.
        void wipe() noexcept                                   {if (size) FL_WipeMemory(buf, size);}
    };


    struct nullslice_t : public slice {
        constexpr nullslice_t() noexcept   :slice() {}
    };
    
    /** A null/empty slice. (You can also use `nullptr` for this purpose.) */
    constexpr nullslice_t nullslice;


    /** Literal syntax for slices: `"foo"_sl` */
    inline constexpr slice operator "" _sl (const char *str, size_t length) noexcept {
        return slice(str, length);
    }


#pragma mark - ALLOC_SLICE:

    /** A \ref slice that owns a heap-allocated, ref-counted block of memory. */
    struct [[nodiscard]] alloc_slice : public pure_slice {
        constexpr alloc_slice() noexcept STEPOVER                             {}
        constexpr alloc_slice(std::nullptr_t) noexcept STEPOVER               {}
        constexpr alloc_slice(nullslice_t) noexcept STEPOVER                  {}

        inline explicit alloc_slice(size_t sz) STEPOVER;

        alloc_slice(const void* FL_NULLABLE b, size_t s)    :alloc_slice(slice(b, s)) {}
        alloc_slice(const void* start,
                    const void* end)                        :alloc_slice(slice(start, end)) {}
        explicit alloc_slice(const char* FL_NULLABLE str)   :alloc_slice(slice(str)) {}
        explicit alloc_slice(const std::string &str)        :alloc_slice(slice(str)) {}

        inline explicit alloc_slice(pure_slice s) STEPOVER;
        explicit alloc_slice(FLSlice s)                     :alloc_slice(s.buf, s.size) { }
        alloc_slice(const alloc_slice &s) noexcept STEPOVER     :pure_slice(s) {retain();}
        alloc_slice(alloc_slice&& s) noexcept STEPOVER          :pure_slice(s) {s.set(nullptr, 0);}

        ~alloc_slice() STEPOVER                                 {_FLBuf_Release(buf);}

        inline alloc_slice& operator=(const alloc_slice&) & noexcept STEPOVER;

        alloc_slice& operator=(alloc_slice&& s) & noexcept  {
            std::swap((slice&)*this, (slice&)s);
            return *this;
        }

        /** Creates an alloc_slice that has an extra null (0) byte immediately after the end of the
            data. This allows the contents of the alloc_slice to be used as a C string. */
        inline static alloc_slice nullPaddedString(pure_slice);

        alloc_slice& operator= (pure_slice s) &               {return *this = alloc_slice(s);}
        alloc_slice& operator= (FLSlice s) &                  {return operator=(slice(s.buf,s.size));}
        alloc_slice& operator= (std::nullptr_t) & noexcept    {reset(); return *this;}

        // disambiguation:
        alloc_slice& operator= (const char *str) &            {*this = (slice)str; return *this;}
        alloc_slice& operator= (const std::string &str) &     {*this = (slice)str; return *this;}

        /// Releases and clears; same as assigning `nullslice`.
        void reset() noexcept                               {release(); assignFrom(nullslice);}
        /// Resets to an uninitialized block of the given size.
        void reset(size_t sz)                               {*this = alloc_slice(sz);}
        /// Changes the size of the block by re-allocating. This changes `buf`!
        inline void resize(size_t newSize);
        /// Grows the block and appends data to the end.
        inline void append(pure_slice);
        /// Shortens the block. (Like \ref resize but asserts that the new size is not larger.)
        inline void shorten(size_t s);

        // FLSliceResult interoperability:
        explicit alloc_slice(const FLSliceResult &s) noexcept STEPOVER :pure_slice(s.buf, s.size) {retain();}
        alloc_slice(FLSliceResult &&sr) noexcept STEPOVER  :pure_slice(sr.buf, sr.size) { }
        explicit operator FLSliceResult () & noexcept       {retain(); return {(void*)buf, size};}
        explicit operator FLSliceResult () && noexcept      {FLSliceResult r {(void*)buf, size};
                                                             set(nullptr, 0); return r;}
        alloc_slice& operator= (FLSliceResult &&sr) & noexcept {release(); set(sr.buf, sr.size);
                                                              return *this;}
        // FLHeapSlice interoperability:
        alloc_slice(FLHeapSlice s) noexcept STEPOVER        :pure_slice(s.buf, s.size) {retain();}
        alloc_slice& operator= (FLHeapSlice) & noexcept;
        operator FLHeapSlice () const noexcept              {return {buf, size};}

        // std::string_view interoperability:
#ifdef SLICE_SUPPORTS_STRING_VIEW
        explicit alloc_slice(string_view str) STEPOVER          :alloc_slice(slice(str)) {}
        alloc_slice& operator=(string_view str) &             {*this = (slice)str; return *this;}
#endif

        // CFData / CFString / NSData / NSString interoperability:
#ifdef __APPLE__
        // Implementations in slice+CoreFoundation.cc and slice+ObjC.mm
        explicit alloc_slice(CFDataRef FL_NULLABLE);
        explicit alloc_slice(CFStringRef FL_NULLABLE);
        /** Creates a CFDataDataRef. The data is not copied: the CFDataRef points to the same
            bytes as this alloc_slice, which is retained until the CFDataRef is freed. */
        [[nodiscard]] CFDataRef createCFData() const;
#   ifdef __OBJC__
        explicit alloc_slice(NSData* FL_NULLABLE data);
        /** Creates an NSData using initWithBytesNoCopy and a deallocator that releases this
            alloc_slice. The data is not copied and does not belong to the NSData object. */
        NSData* uncopiedNSData() const;
#   endif
#endif

        // Manual ref-count management. Use with extreme caution! You probably don't need this.
        alloc_slice& retain() noexcept                      {_FLBuf_Retain(buf); return *this;}
        inline void release() noexcept                      {_FLBuf_Release(buf);}
        static void retain(slice s) noexcept                {static_cast<alloc_slice &>(static_cast<pure_slice &>(s)).retain();}
        static void release(slice s) noexcept               {static_cast<alloc_slice &>(static_cast<pure_slice &>(s)).release();}

    private:
        void assignFrom(pure_slice s)                       {set(s.buf, s.size);}
    };



    /** A slice whose `buf` may not be NULL. For use as a parameter type. */
    struct slice_NONNULL : public slice {
        constexpr slice_NONNULL(const void* b, size_t s)            :slice(b, s) {}
        constexpr slice_NONNULL(slice s)                            :slice_NONNULL(s.buf, s.size) {}
        constexpr slice_NONNULL(FLSlice s)                          :slice_NONNULL(s.buf,s.size) {}
        constexpr slice_NONNULL(const char *str NONNULL)            :slice(str) {}
        slice_NONNULL(alloc_slice s)                                :slice_NONNULL(s.buf,s.size) {}
        slice_NONNULL(const std::string &str)               :slice_NONNULL(str.data(),str.size()) {}
#ifdef SLICE_SUPPORTS_STRING_VIEW
        slice_NONNULL(string_view str)                      :slice_NONNULL(str.data(),str.size()) {}
#endif
        slice_NONNULL(std::nullptr_t) =delete;
        slice_NONNULL(nullslice_t) =delete;
    };



#ifdef __APPLE__
    /** A slice holding the UTF-8 data of an NSString. If possible, it gets a pointer directly into
        the NSString in O(1) time -- so don't modify or release the NSString while this is in scope.
        Alternatively it will copy the string's UTF-8 into a small internal buffer, or allocate
        a larger buffer on the heap (and free it in its destructor.) */
    struct nsstring_slice : public slice {
        nsstring_slice(CFStringRef FL_NULLABLE);
#   ifdef __OBJC__
        nsstring_slice(NSString* FL_NULLABLE str)   :nsstring_slice((__bridge CFStringRef)str) { }
#   endif
        ~nsstring_slice();
    private:
        long getBytes(CFStringRef, long lengthInChars);
        char _local[127];
        bool _needsFree;
    };
#endif


    /** Functor class for hashing the contents of a slice.
        \note The below declarations of `std::hash` usually make it unnecessary to use this. */
    struct sliceHash {
        std::size_t operator() (pure_slice const& s) const {return s.hash();}
    };



#pragma mark - PURE_SLICE METHOD BODIES:


    // like strlen but can run at compile time
#if __cplusplus >= 201400L || _MSVC_LANG >= 201400L
    inline constexpr size_t pure_slice::_strlen(const char* FL_NULLABLE str) noexcept {
        if (!str)
            return 0;
        auto c = str;
        while (*c) ++c;
        return c - str;
    }
#else
    // (In C++11, constexpr functions couldn't contain loops; use (tail-)recursion instead)
    inline constexpr size_t pure_slice::_strlen(const char* FL_NULLABLE str) noexcept {
        return str ? _strlen(str, 0) : 0;
    }
    inline constexpr size_t pure_slice::_strlen(const char *str, size_t n) noexcept {
        return *str ? _strlen(str + 1, n + 1) : n;
    }
#endif


    inline constexpr pure_slice::pure_slice(const void* FL_NULLABLE b, size_t s) noexcept
    :buf(b), size(s)
    {
        checkValidSlice();
    }


    inline void pure_slice::setBuf(const void *b) noexcept {
        const_cast<const void*&>(buf) = b;
        checkValidSlice();
    }

    inline void pure_slice::setSize(size_t s) noexcept {
        const_cast<size_t&>(size) = s;
        checkValidSlice();
    }

    inline void pure_slice::set(const void * FL_NULLABLE b, size_t s) noexcept {
        const_cast<const void*&>(buf) = b;
        const_cast<size_t&>(size) = s;
        checkValidSlice();
    }


    inline bool pure_slice::validAddress(const void * FL_NULLABLE addr) const noexcept {
        // Note: unsigned comparison handles case when addr < buf
        return size_t(_pointerDiff(addr, buf)) <= size;
    }


    inline bool pure_slice::containsAddress(const void * FL_NULLABLE addr) const noexcept {
        return size_t(_pointerDiff(addr, buf)) < size;
    }


    inline bool pure_slice::containsAddressRange(pure_slice s) const noexcept {
        return s.buf >= buf && s.end() <= end();
    }


    inline constexpr void pure_slice::checkValidSlice() const {
        assert_precondition(buf != nullptr || size == 0);
        assert_precondition(size < (1ull << (8*sizeof(void*)-1)));   // check accidental negative size
    }


    inline const void* pure_slice::check(const void *addr) const {
        assert_precondition(validAddress(addr));
        return addr;
    }

    inline size_t pure_slice::check(size_t offset) const {
        assert_precondition(offset <= size);
        return offset;
    }


    inline const void* pure_slice::offset(size_t o) const noexcept {
        return (uint8_t*)buf + check(o);
    }

    inline size_t pure_slice::offsetOf(const void* ptr NONNULL) const noexcept {
        return pointerDiff(check(ptr), buf);
    }


    inline slice pure_slice::upTo(const void* pos) const noexcept {
        return slice(buf, check(pos));
    }

    inline slice pure_slice::from(const void* pos) const noexcept {
        return slice(check(pos), end());
    }

    inline slice pure_slice::upTo(size_t off) const noexcept {
        return slice(buf, check(off));
    }

    inline slice pure_slice::from(size_t off) const noexcept {
        return slice(offset(check(off)), end());
    }

    inline const uint8_t& pure_slice::operator[](size_t off) const noexcept {
        assert_precondition(off < size);
        return ((const uint8_t*)buf)[off];
    }

    inline slice pure_slice::operator()(size_t off, size_t nBytes) const noexcept  {
        assert_precondition(off + nBytes <= size);
        return slice(offset(off), nBytes);
    }


    inline bool pure_slice::toCString(char *str, size_t bufSize) const noexcept {
        size_t n = std::min(size, bufSize-1);
        FLMemCpy(str, buf, n);
        str[n] = 0;
        return n == size;
    }


    inline std::string pure_slice::hexString() const {
        static const char kDigits[17] = "0123456789abcdef";
        std::string result;
        result.reserve(2 * size);
        for (size_t i = 0; i < size; i++) {
            uint8_t byte = (*this)[(unsigned)i];
            result += kDigits[byte >> 4];
            result += kDigits[byte & 0xF];
        }
        return result;
    }


#pragma mark  COMPARISON & FIND:


    __hot
    inline int pure_slice::caseEquivalentCompare(pure_slice b) const noexcept {
        size_t minSize = std::min(size, b.size);
        for (size_t i = 0; i < minSize; i++) {
            if ((*this)[i] != b[i]) {
                int cmp = ::tolower((*this)[i]) - ::tolower(b[i]);
                if (cmp != 0)
                    return cmp;
            }
        }
        return (int)size - (int)b.size;
    }


    __hot
    inline bool pure_slice::caseEquivalent(pure_slice b) const noexcept {
        if (size != b.size)
            return false;
        for (size_t i = 0; i < size; i++)
            if (::tolower((*this)[i]) != ::tolower(b[i]))
                return false;
        return true;
    }


    __hot
    inline slice pure_slice::find(pure_slice target) const noexcept {
        char* src = (char *)buf;
        char* search = (char *)target.buf;
        char* found = std::search(src, src + size, search, search + target.size);
        if(found == src + size) {
            return nullslice;
        }

        return {found, target.size};
    }


    inline bool pure_slice::containsBytes(pure_slice bytes) const noexcept {
        return bool(find(bytes));
    }


    __hot
    inline const uint8_t* FL_NULLABLE pure_slice::findByte(uint8_t b) const {
        if (_usuallyFalse(size == 0))
            return nullptr;
        return (const uint8_t*)::memchr(buf, b, size);
    }


    __hot
    inline const uint8_t* FL_NULLABLE pure_slice::findByteOrEnd(uint8_t byte) const noexcept {
        auto result = findByte(byte);
        return result ? result : (const uint8_t*)end();
    }


    __hot
    inline const uint8_t* FL_NULLABLE pure_slice::findAnyByteOf(pure_slice targetBytes) const noexcept {
        // this could totally be optimized, if it turns out to matter...
        const void* result = nullptr;
        for (size_t i = 0; i < targetBytes.size; ++i) {
            auto r = findByte(targetBytes[i]);
            if (r && (!result || r < result))
                result = r;
        }
        return (const uint8_t*)result;
    }


    __hot
    inline const uint8_t* FL_NULLABLE pure_slice::findByteNotIn(pure_slice targetBytes) const noexcept {
        for (auto c = (const uint8_t*)buf; c != end(); ++c) {
            if (!targetBytes.findByte(*c))
                return c;
        }
        return nullptr;
    }


    inline bool pure_slice::hasPrefix(pure_slice s) const noexcept {
        return s.size > 0 && size >= s.size && ::memcmp(buf, s.buf, s.size) == 0;
    }


    inline bool pure_slice::hasSuffix(pure_slice s) const noexcept {
        return s.size > 0 && size >= s.size
            && ::memcmp(offsetby(buf, size - s.size), s.buf, s.size) == 0;
    }


#pragma mark  MEMORY ALLOCATION


    /** Raw memory allocation. Just like malloc but throws/terminates on failure. */
    RETURNS_NONNULL
    inline void* pure_slice::newBytes(size_t sz) {
        void* result = ::malloc(sz);
        if (_usuallyFalse(!result)) failBadAlloc();
        return result;
    }


    /** Like realloc but throws/terminates on failure. */
    template <typename T>
    RETURNS_NONNULL
    inline T* FL_NONNULL pure_slice::reallocBytes(T* FL_NULLABLE bytes, size_t newSz) {
        T* newBytes = (T*)::realloc(bytes, newSz);
        if (_usuallyFalse(!newBytes)) failBadAlloc();
        return newBytes;
    }

    inline slice pure_slice::copy() const {
        if (buf == nullptr)
            return nullslice;
        void* copied = newBytes(size);
        FLMemCpy(copied, buf, size);
        return slice(copied, size);
    }


    [[noreturn]]
    inline void pure_slice::failBadAlloc() {
#ifdef __cpp_exceptions
        throw std::bad_alloc();
#else
        ::fputs("*** FATAL ERROR: heap allocation failed (fleece/slice.cc) ***\n", stderr);
        std::terminate();
#endif
    }


#pragma mark - SLICE METHOD BODIES:


    inline constexpr slice::slice(nullslice_t) noexcept           :pure_slice() {}
    inline constexpr slice::slice(const alloc_slice &s) noexcept  :pure_slice(s) { }


    inline constexpr slice::slice(const void* start, const void* end) noexcept
    :slice(start, pointerDiff(end, start))
    {
        assert_precondition(end >= start);
    }


    inline slice& slice::operator= (nullslice_t) & noexcept {
        set(nullptr, 0);
        return *this;
    }


    inline slice::operator FLSliceResult () const noexcept {
        return FLSliceResult(alloc_slice(*this));
    }


    inline void slice::shorten(size_t s) {
        setSize(check(s));
    }


    inline void slice::setStart(const void *s) noexcept {
        check(s);
        set(s, pointerDiff(end(), s));
    }


#pragma mark - ALLOC_SLICE METHOD BODIES:


    __hot
    inline alloc_slice::alloc_slice(size_t sz)
    :alloc_slice(FLSliceResult_New(sz))
    {
        if (_usuallyFalse(!buf))
            failBadAlloc();
    }


    __hot
    inline alloc_slice::alloc_slice(pure_slice s)
    :alloc_slice(FLSlice_Copy(s))
    {
        if (_usuallyFalse(!buf) && s.buf)
            failBadAlloc();
    }


    inline alloc_slice alloc_slice::nullPaddedString(pure_slice str) {
        // Leave a trailing null byte after the end, so it can be used as a C string
        alloc_slice a(str.size + 1);
        str.copyTo((void*)a.buf);
        ((char*)a.buf)[str.size] = '\0';
        a.shorten(str.size);            // the null byte is not part of the slice
        return a;
    }


    __hot
    inline alloc_slice& alloc_slice::operator=(const alloc_slice& s) & noexcept {
        if (_usuallyTrue(s.buf != buf)) {
            release();
            assignFrom(s);
            retain();
        }
        return *this;
    }


    __hot
    inline alloc_slice& alloc_slice::operator=(FLHeapSlice s) & noexcept {
        if (_usuallyTrue(s.buf != buf)) {
            release();
            assignFrom(slice(s));
            retain();
        }
        return *this;
    }


    inline void alloc_slice::resize(size_t newSize) {
        if (newSize == size) {
            return;
        } else if (buf == nullptr) {
            reset(newSize);
        } else {
            // We don't realloc the current buffer; that would affect other alloc_slice objects
            // sharing the buffer, and possibly confuse them. Instead, alloc a new buffer & copy.
            alloc_slice newSlice(newSize);
            FLMemCpy((void*)newSlice.buf, buf, std::min(size, newSize));
            *this = std::move(newSlice);
        }
    }


    inline void alloc_slice::append(pure_slice source) {
        if (_usuallyFalse(source.size == 0))
            return;
        const void *src = source.buf;
        size_t oldSize = size;
        if (_usuallyFalse(containsAddress(src))) {
            // Edge case, where I contain the source bytes: update source address after realloc
            size_t srcOff = size_t(pointerDiff(src, buf));
            resize(oldSize + source.size);
            src = offset(srcOff);
        } else {
            resize(oldSize + source.size);
        }
        ::memcpy((void*)offset(oldSize), src, source.size);  // already checked source.size > 0
    }


    inline void alloc_slice::shorten(size_t s) {
        pure_slice::setSize(check(s));
    }

}


namespace std {
    // Declare the default hash function for `slice` and `alloc_slice`. This allows them to be
    // used in hashed collections like `std::unordered_map` and `std::unordered_set`.
    template<> struct hash<fleece::slice> {
        std::size_t operator() (fleece::pure_slice const& s) const {return s.hash();}
    };
    template<> struct hash<fleece::alloc_slice> {
        std::size_t operator() (fleece::pure_slice const& s) const {return s.hash();}
    };
}


FL_ASSUME_NONNULL_END

#endif // _FLEECE_SLICE_HH
