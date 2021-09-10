//
// ConcurrentArena.cc
//
// Copyright 2020-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "ConcurrentArena.hh"
#include <string.h>

using namespace std;

namespace fleece {

    ConcurrentArena::ConcurrentArena()
    :_heapEnd(nullptr)
    ,_nextBlock(nullptr)
    { }


    ConcurrentArena::ConcurrentArena(size_t capacity)
    :_heap(new uint8_t[capacity])
    ,_heapEnd(&_heap[capacity])
    ,_nextBlock(&_heap[0])
    { }


    ConcurrentArena::ConcurrentArena(ConcurrentArena &&other) {
        *this = move(other);
    }


    ConcurrentArena& ConcurrentArena::operator=(ConcurrentArena &&other) {
        _heap = move(other._heap);
        _heapEnd = other._heapEnd;
        _nextBlock = other._nextBlock.load();
        return *this;
    }


    __hot
    void* ConcurrentArena::alloc(size_t size) {
        uint8_t *result, *newNext, *nextBlock = _nextBlock;
        do {
            result = nextBlock;
            newNext = nextBlock + size;
            if (newNext > _heapEnd)
                return nullptr;  // overflow
        } while (!_nextBlock.compare_exchange_weak(nextBlock, newNext, memory_order_acq_rel));
        return result;
    }


    __hot
    void* ConcurrentArena::calloc(size_t size) {
        auto block = alloc(size);
        if (_usuallyTrue(block != nullptr))
            memset(block, 0, size);
        return block;
    }


    bool ConcurrentArena::free(void *block, size_t size) {
        uint8_t *nextBlock = (uint8_t*)block + size;
        uint8_t *newNext = (uint8_t*)block;
        return _nextBlock.compare_exchange_weak(nextBlock, newNext, memory_order_acq_rel);
    }

}
