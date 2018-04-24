//
//  MHashTreeTests.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "Value.hh"
#include "MHashTree.hh"

using namespace fleece;

TEST_CASE("Empty MHashTree", "[MHashTree]") {
    MHashTree tree;
    CHECK(tree.count() == 0);
    CHECK(tree.get(alloc_slice("foo")) == 0);
    CHECK(!tree.remove(alloc_slice("foo")));
}

TEST_CASE("Tiny MHashTree Insert", "[MHashTree]") {
    auto key = alloc_slice("foo");
    auto val = (const Value*)123;

    MHashTree tree;
    tree.insert(key, val);
    CHECK(tree.get(key) == val);
    CHECK(tree.count() == 1);

    tree.dump(std::cerr);
}

TEST_CASE("Bigger MHashTree Insert", "[MHashTree]") {
    static constexpr int N = 1000;
    std::vector<alloc_slice> keys(N);
    std::vector<const Value*> values(N);
    
    for (int i = 0; i < N; i++) {
        char buf[100];
        sprintf(buf, "Key %d, squared is %d", i, i*i);
        keys[i] = alloc_slice(buf);
        values[i] = (const Value*)size_t(1+i);
    }

    MHashTree tree;
    for (unsigned i = 0; i < N; i++) {
//        std::cerr << "\n##### Inserting #" << (i+1) << ", " << std::hex << keys[i].hash() << "\n";
        tree.insert(keys[i], values[i]);
//        tree.dump(std::cerr);
        CHECK(tree.count() == i + 1);
#if 0
        for (int j = i; j >= 0; --j)
            CHECK(tree.get(keys[j]) == values[j]);
#endif
    }
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == values[i]);
    }
    tree.dump(std::cerr);
}

TEST_CASE("Tiny MHashTree Remove", "[MHashTree]") {
    auto key = alloc_slice("foo");
    auto val = (const Value*)123;

    MHashTree tree;
    tree.insert(key, val);
    CHECK(tree.remove(key));
    CHECK(tree.get(key) == 0);
    CHECK(tree.count() == 0);
}

TEST_CASE("Bigger MHashTree Remove", "[MHashTree]") {
    static constexpr int N = 10000;
    std::vector<alloc_slice> keys(N);
    std::vector<const Value*> values(N);

    for (int i = 0; i < N; i++) {
        char buf[100];
        sprintf(buf, "Key %d, squared is %d", i, i*i);
        keys[i] = alloc_slice(buf);
        values[i] = (const Value*)size_t(1+i);
    }

    MHashTree tree;
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

