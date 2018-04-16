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
    CHECK(tree.count() == 0);
    CHECK(tree.get(alloc_slice("foo")) == 0);
    CHECK(!tree.remove(alloc_slice("foo")));
}

TEST_CASE("Tiny HAMTree Insert", "[HAMTree]") {
    auto key = alloc_slice("foo");
    auto val = 123;

    HAMTree tree;
    tree.insert(key, val);
    CHECK(tree.get(key) == val);
    CHECK(tree.count() == 1);

    tree.dump(std::cerr);
}

TEST_CASE("Bigger HAMTree Insert", "[HAMTree]") {
    static constexpr int N = 1000;
    std::vector<alloc_slice> keys(N);
    std::vector<int> values(N);
    
    for (int i = 0; i < N; i++) {
        char buf[100];
        sprintf(buf, "Key %d, squared is %d", i, i*i);
        keys[i] = alloc_slice(buf);
        values[i] = 1+i;
    }

    HAMTree tree;
    for (int i = 0; i < N; i++) {
        tree.insert(keys[i], values[i]);
        CHECK(tree.count() == i + 1);
//        for (int j = i; j >= 0; --j)
//            CHECK(tree.get(keys[j]) == values[j]);
    }
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == values[i]);
    }
    tree.dump(std::cerr);
}

TEST_CASE("Tiny HAMTree Remove", "[HAMTree]") {
    auto key = alloc_slice("foo");
    auto val = 123;

    HAMTree tree;
    tree.insert(key, val);
    CHECK(tree.remove(key));
    CHECK(tree.get(key) == 0);
    CHECK(tree.count() == 0);
}

TEST_CASE("Bigger HAMTree Remove", "[HAMTree]") {
    static constexpr int N = 10000;
    std::vector<alloc_slice> keys(N);
    std::vector<int> values(N);

    for (int i = 0; i < N; i++) {
        char buf[100];
        sprintf(buf, "Key %d, squared is %d", i, i*i);
        keys[i] = alloc_slice(buf);
        values[i] = 1+i;
    }

    HAMTree tree;
    for (int i = 0; i < N; i++) {
        tree.insert(keys[i], values[i]);
    }
    for (int i = 0; i < N; i += 3) {
        tree.remove(keys[i]);
    }
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == ((i%3) ? values[i] : 0));
    }
    CHECK(tree.count() == N - 1 - (N / 3));
}

