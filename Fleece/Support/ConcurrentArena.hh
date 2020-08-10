//
// ConcurrentArena.hh
//
// Copyright © 2020 Couchbase. All rights reserved.
//

#pragma once
#include "PlatformCompat.hh"
#include "betterassert.hh"
#include <atomic>
#include <memory>

namespace fleece {

    /** A simple memory allocator that carves blocks out of a pre-allocated fixed-size heap block.
        To allocate a new block it simply bumps a pointer forward by the size requested.
        It is not generally possible to free blocks, although the _last_ allocated block can be freed
        by bumping the pointer backwards.
        Obviously all blocks are freed/invalidated when the ConcurrentArena itself is destructed. */
    class ConcurrentArena {
    public:
        /** Constructs an arena with the given byte capacity. This allocates a block of that size
            from the default heap using ::operator new. */
        ConcurrentArena(size_t capacity);

        /** Constructs an empty arena, without allocating any space.
            This is only provided so that an arena can be initialized for real after its constructor,
            by assigning it a new instance with operator=. */
        ConcurrentArena();

        ConcurrentArena(ConcurrentArena&&);
        ConcurrentArena& operator=(ConcurrentArena &&);

        size_t capacity() const FLPURE      {return _heapEnd - _heap.get();}
        size_t allocated() const FLPURE     {return _nextBlock - _heap.get();}
        size_t available() const FLPURE     {return _heapEnd - _nextBlock;}

        /** Allocates a new block of the given size.
            @return The new block, or nullptr if there's no space. */
        void* alloc(size_t size);

        /** Allocates and zeroes a new block of the given size.
            @return The new block, or nullptr if there's no space. */
        void* calloc(size_t size);

        /** _Attempts_ to free the given block. This only works if it's the latest allocated block.
             @param allocatedBlock  A block allocated by `alloc` or `calloc`.
             @param size  The exact size of the block, as given to `alloc` or `calloc`.
             @return  True if freed, false if not. */
        bool free(void *allocatedBlock, size_t size);

        /** Frees all allocated blocks, resetting the arena to its empty state.
            (Does not free the arena heap itself!) */
        void freeAll()                      {_nextBlock = _heap.get();}

        /** Converts a block pointer to an integer offset from the start of the heap.
            The offset will be less than the arena's capacity.
            (This also works for interior pointers within blocks.) */
        size_t toOffset(const void *ptr) const {
            assert(ptr >= _heap.get() && ptr < _heapEnd);
            return (uint8_t*)ptr - _heap.get();
        }

        /** Converts a heap offset back into a pointer. */
        void* toPointer(size_t off) const {
            void *ptr = _heap.get() + off;
            assert(ptr < _heapEnd);
            return ptr;
        }

    private:
        std::unique_ptr<uint8_t[]>  _heap;      // The heap block used for storage
        uint8_t*                    _heapEnd;   // Points just past the end of the heap
        std::atomic<uint8_t*>       _nextBlock; // Points to the next available byte
    };


    /** C++ allocator using a ConcurrentArena. */
    template <class T, bool zeroing =false>
    class ConcurrentArenaAllocator {
    public:
        typedef T value_type;

        ConcurrentArenaAllocator(ConcurrentArena &arena) :_arena(arena) { }

        [[nodiscard]] T* allocate(size_t n) {
            if (zeroing)
                return (T*) _arena.calloc(n * sizeof(T));
            else
                return (T*) _arena.alloc(n * sizeof(T));
        }

        void deallocate(T* p, size_t n) noexcept    {return _arena.free(p, n * sizeof(T));}

    private:
        ConcurrentArena& _arena;
    };

}
