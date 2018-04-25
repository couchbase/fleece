//
//  MHashTreeTests.cc
//  Fleece
//
//  Copyright © 2018 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "Fleece.hh"
#include "MHashTree.hh"
#include "Writer.hh"

using namespace fleece;


class HashTreeTests {
public:
    MHashTree tree;
    std::vector<alloc_slice> keys;
    const Array* values;
    alloc_slice _valueBuf;

    void createItems(size_t N) {
        Encoder enc;
        enc.beginArray(N);
        for (size_t i = 0; i < N; i++)
            enc.writeInt(i + 1);
        enc.endArray();
        _valueBuf = enc.extractOutput();
        values = Value::fromTrustedData(_valueBuf)->asArray();

        keys.resize(N);
        for (size_t i = 0; i < N; i++) {
            char buf[100];
            sprintf(buf, "Key %zu, %zu", i, i*i);
            keys[i] = alloc_slice(buf);
        }
    }

    void insertItems(bool verbose= false, bool check =false) {
        const size_t N = keys.size();
        for (size_t i = 0; i < N; i++) {
            if (verbose)
                std::cerr << "\n##### Inserting #" << (i+1)
                          << ", " << std::hex << keys[i].hash() << "\n";
            tree.insert(keys[i], values->get(uint32_t(i)));
            if (verbose)
                tree.dump(std::cerr);
            if (check) {
                CHECK(tree.count() == i + 1);
                for (ssize_t j = i; j >= 0; --j)
                    CHECK(tree.get(keys[j]) == values->get(uint32_t(i)));
            }
        }
    }

};


#pragma mark - TEST CASES:



TEST_CASE_METHOD(HashTreeTests, "Empty MHashTree", "[HashTree]") {
    CHECK(tree.count() == 0);
    CHECK(tree.get(alloc_slice("foo")) == 0);
    CHECK(!tree.remove(alloc_slice("foo")));
}


TEST_CASE_METHOD(HashTreeTests, "Tiny MHashTree Insert", "[HashTree]") {
    auto key = alloc_slice("foo");
    auto val = (const Value*)123;
    tree.insert(key, val);

    CHECK(tree.get(key) == val);
    CHECK(tree.count() == 1);

    tree.dump(std::cerr);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger MHashTree Insert", "[HashTree]") {
    static constexpr int N = 1000;
    createItems(N);
    insertItems();
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == values->get(uint32_t(i)));
    }
    tree.dump(std::cerr);
}


TEST_CASE_METHOD(HashTreeTests, "Tiny MHashTree Remove", "[HashTree]") {
    auto key = alloc_slice("foo");
    auto val = (const Value*)123;

    tree.insert(key, val);
    CHECK(tree.remove(key));
    CHECK(tree.get(key) == 0);
    CHECK(tree.count() == 0);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger MHashTree Remove", "[HashTree]") {
    static constexpr int N = 10000;
    createItems(N);
    insertItems();

    for (int i = 0; i < N; i += 3) {
        tree.remove(keys[i]);
    }
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == ((i%3) ? values->get(uint32_t(i)) : nullptr));
    }
    CHECK(tree.count() == N - 1 - (N / 3));
}


TEST_CASE_METHOD(HashTreeTests, "Tiny MHashTree Write", "[HashTree]") {
    createItems(10);
    auto key = keys[8];
    auto val = values->get(8);
    tree.insert(key, val);

    Writer w;
    auto offset = tree.writeTo(w);
    alloc_slice data = w.extractOutput();
    REQUIRE(data.size == 28); // could change if encoding changes
    std::cerr << "Data: " << data.hexString()
              << "\noffset = " << offset << " of " << data.size << "\n";

    // Now read it as an immutable HashTree:
    const HashTree *tree = HashTree::fromData(data);
    CHECK(tree->count() == 1);
    auto value = tree->get(key);
    REQUIRE(value);
    CHECK(value->asInt() == 9);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger MHashTree Write", "[HashTree]") {
    static constexpr int N = 100;
    createItems(N);
    insertItems();

    Writer w;
    auto offset = tree.writeTo(w);
    alloc_slice data = w.extractOutput();
//    std::cerr << "Data: " << data.hexString() << "\noffset = " << offset << " of " << data.size << "\n";

    // Now read it as an immutable HashTree:
    const HashTree *itree = HashTree::fromData(data);
    CHECK(itree->count() == N);
}


TEST_CASE_METHOD(HashTreeTests, "Tiny HashTree Mutate", "[HashTree]") {
    createItems(10);
    tree.insert(keys[9], values->get(9));

    Writer w;
    auto offset = tree.writeTo(w);
    alloc_slice data = w.extractOutput();
    std::cerr << "Data: " << data.hexString()
    << "\noffset = " << offset << " of " << data.size << "\n";

    const HashTree *itree = HashTree::fromData(data);
    itree->dump(std::cerr);

    // Wrap in a MHashTree and get the key:
    MHashTree tree2(itree);
    
    tree2.dump(std::cerr);
    CHECK(tree2.count() == 1);
    auto value = tree2.get(keys[9]);
    REQUIRE(value);
    CHECK(value->isInteger());
    CHECK(value->asInt() == 10);

    // Modify the value for the key:
    tree2.insert(keys[9], values->get(3));

    tree2.dump(std::cerr);
    CHECK(tree2.count() == 1);
    value = tree2.get(keys[9]);
    REQUIRE(value);
    CHECK(value->asInt() == 4);
}
