//
// ConcurrentArena.cc
//
// Copyright © 2020 Couchbase. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "ConcurrentArena.hh"

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
