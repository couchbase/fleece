//
//  HAMTreeTests.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "Value.hh"
#include "HAMTree.hh"

using namespace fleece;

TEST_CASE("Empty HAMTree", "[HAMTree]") {
    HAMTree tree;
    CHECK(tree.get(alloc_slice("foo")) == 0);
}

TEST_CASE("Tiny HAMTree", "[HAMTree]") {
    auto key = alloc_slice("foo");
    auto val = 123;

    HAMTree tree;
    tree.insert(key, val);
    CHECK(tree.get(key) == val);
}

TEST_CASE("Bigger HAMTree", "[HAMTree]") {
    static constexpr int N = 10000;
    std::vector<alloc_slice> keys(N);
    std::vector<int> values(N);
    
    for (int i = 0; i < N; i++) {
        char buf[100];
        sprintf(buf, "Key %d", i);
        keys[i] = alloc_slice(buf);
        values[i] = 1+i;
    }

    HAMTree tree;
    for (int i = 0; i < N; i++) {
        tree.insert(keys[i], values[i]);
//        for (int j = i; j >= 0; --j)
//            CHECK(tree.get(keys[j]) == values[j]);
    }
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == values[i]);
    }
}
