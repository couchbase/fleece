//
//  MutableHashTreeTests.cc
//  Fleece
//
// Copyright © 2018 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "MutableHashTree.hh"
#include "PlatformCompat.hh"

using namespace std;
using namespace fleece;

static const char* kDigits[10] = {"zero", "one", "two", "three", "four", "five", "six",
                                  "seven", "eight", "nine"};


class HashTreeTests {
public:
    MutableHashTree tree;
    vector<alloc_slice> keys;
    Array values;
    alloc_slice _valueBuf;

    void createItems(size_t N) {
        Encoder enc;
        enc.beginArray(N);
        for (size_t i = 0; i < N; i++)
            enc.writeInt(i);
        enc.endArray();
        _valueBuf = enc.finish();
        values = Value::fromData(_valueBuf, kFLTrusted).asArray();

        keys.clear();
        for (size_t i = 0; i < N; i++) {
            char buf[100];
            if (i < 100)
                sprintf(buf, "%s %s", kDigits[i/10], kDigits[i%10]);
            else
                sprintf(buf, "%zd %s", i/10, kDigits[i%10]);
            keys.push_back(alloc_slice(buf));
        }
    }

    void insertItems(size_t N =0, bool verbose= false, bool check =false) {
        if (N == 0)
            N = keys.size();
        for (size_t i = 0; i < N; i++) {
            if (verbose)
                cerr << "\n##### Inserting #" << (i)
                          << ", " << hex << keys[i].hash() << dec << "\n";
            tree.set(keys[i], values.get(uint32_t(i)));
            if (verbose)
                tree.dump(cerr);
            if (check) {
                CHECK(tree.count() == i + 1);
                for (ssize_t j = i; j >= 0; --j)
                    CHECK(tree.get(keys[j]) == values.get(uint32_t(i)));
            }
        }
    }

    void checkTree(size_t N) {
        CHECK(tree.count() == N);
        for (size_t i = 0; i < N; i++) {
            auto value = tree.get(keys[i]);
            REQUIRE(value);
            CHECK(value.isInteger());
            CHECK(value.asInt() == values.get(uint32_t(i)).asInt());
        }
    }

    void checkIterator(size_t N) {
        set<slice> keysSeen;
        for (MutableHashTree::iterator i(tree); i; ++i) {
            //cerr << "--> " << i.key().asString() << "\n";
            CHECK(keysSeen.insert(i.key()).second); // insert, and make sure it's unique
            REQUIRE(i.value() != nullptr);
            CHECK(i.value().type() == kFLNumber);
        }
        CHECK(keysSeen.size() == N);
    }

    alloc_slice encodeTree() {
        Encoder enc;
        enc.suppressTrailer();
        tree.writeTo(enc);
        return enc.finish();
    }

};


#pragma mark - TEST CASES:



TEST_CASE_METHOD(HashTreeTests, "Empty MutableHashTree", "[HashTree]") {
    CHECK(tree.count() == 0);
    CHECK(tree.get(alloc_slice("foo")) == nullptr);
    CHECK(!tree.remove(alloc_slice("foo")));
}


TEST_CASE_METHOD(HashTreeTests, "Tiny MutableHashTree Insert", "[HashTree]") {
    createItems(1);
    auto key = keys[0];
    auto val = values.get(0);
    tree.set(key, val);

    CHECK(tree.get(key) == val);
    CHECK(tree.count() == 1);

    tree.dump(cerr);

    // Check that insertion-with-callback passes value to callback and supports failure:
    Value existingVal = nullptr;
    CHECK(!tree.insert(key, [&](Value val) {existingVal = val; return nullptr;}));
    CHECK(existingVal == val);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger MutableHashTree Insert", "[HashTree]") {
    static constexpr int N = 1000;
    createItems(N);
    insertItems();
//    tree.dump(cerr);
    checkTree(N);
}


TEST_CASE_METHOD(HashTreeTests, "Tiny MutableHashTree Remove", "[HashTree]") {
    createItems(1);
    auto key = keys[0];
    auto val = values.get(0);

    tree.set(key, val);
    CHECK(tree.remove(key));
    CHECK(tree.get(key) == nullptr);
    CHECK(tree.count() == 0);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger MutableHashTree Remove", "[HashTree]") {
#if FL_EMBEDDED
    static constexpr int N = 1000;
#else
    static constexpr int N = 10000;
#endif
    createItems(N);
    insertItems();

    for (int i = 0; i < N; i += 3) {
        tree.remove(keys[i]);
    }
    for (int i = 0; i < N; i++) {
        CHECK(tree.get(keys[i]) == ((i%3) ? values.get(uint32_t(i)) : nullptr));
    }
    CHECK(tree.count() == N - 1 - (N / 3));
}


TEST_CASE_METHOD(HashTreeTests, "MutableHashTree Iterate", "[HashTree]") {
    static constexpr int N = 1000;
    createItems(N);

    cerr << "Empty tree...\n";
    checkIterator(0);

    cerr << "One item...\n";
    insertItems(1);
    checkIterator(1);

    cerr << "Removed item...\n";
    tree.remove(keys[0]);
    checkIterator(0);

    cerr << N << " items...\n";
    insertItems(N);
    checkIterator(N);
}


TEST_CASE_METHOD(HashTreeTests, "Tiny MutableHashTree Write", "[HashTree]") {
    createItems(10);
    auto key = keys[8];
    auto val = values.get(8);
    tree.set(key, val);

    alloc_slice data = encodeTree();
    REQUIRE(data.size == 30); // could change if encoding changes
    cerr << data.size << " bytes encoded: " << data.hexString() << "\n";

    // Now read it as an immutable HashTree:
    const HashTree *tree = HashTree::fromData(data);
    CHECK(tree->count() == 1);
    auto value = tree->get(key);
    REQUIRE(value);
    CHECK(value.isInteger());
    CHECK(value.asInt() == 8);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger MutableHashTree Write", "[HashTree]") {
    static constexpr int N = 100;
    createItems(N);
    insertItems();

    alloc_slice data = encodeTree();
//    cerr << "Data: " << data.hexString() << "\noffset = " << offset << " of " << data.size << "\n";

    // Now read it as an immutable HashTree:
    const HashTree *itree = HashTree::fromData(data);
    CHECK(itree->count() == N);
}


TEST_CASE_METHOD(HashTreeTests, "Tiny HashTree Mutate", "[HashTree]") {
    createItems(10);
    tree.set(keys[9], values.get(9));

    alloc_slice data = encodeTree();
    const HashTree *itree = HashTree::fromData(data);
    itree->dump(cerr);

    // Wrap in a MutableHashTree and get the key:
    tree = itree;
    
    tree.dump(cerr);
    CHECK(tree.count() == 1);
    auto value = tree.get(keys[9]);
    REQUIRE(value);
    CHECK(value.isInteger());
    CHECK(value.asInt() == 9);

    // Modify the value for the key:
    tree.set(keys[9], values.get(3));

    tree.dump(cerr);
    CHECK(tree.count() == 1);
    value = tree.get(keys[9]);
    REQUIRE(value);
    CHECK(value.asInt() == 3);
}


TEST_CASE_METHOD(HashTreeTests, "Bigger HashTree Mutate by replacing", "[HashTree]") {
    createItems(100);
    insertItems(100);

    alloc_slice data = encodeTree();
    const HashTree *itree = HashTree::fromData(data);
//    itree->dump(cerr);

    // Wrap in a MutableHashTree and get the key:
    tree = itree;

//    tree.dump(cerr);
    checkTree(100);

    for (int i = 0; i < 10; ++i) {
        // Modify the value for the key:
        int old = i*i, nuu = 99-old;
        //cerr << "\n#### Set key " << old << " to " << nuu << ":\n";
        tree.set(keys[old], values.get(nuu));

        //tree.dump(cerr);
        CHECK(tree.count() == 100);
        auto value = tree.get(keys[old]);
        REQUIRE(value);
        CHECK(value.asInt() == nuu);
    }
}


TEST_CASE_METHOD(HashTreeTests, "Bigger HashTree Mutate by inserting", "[HashTree]") {
    createItems(20);
    insertItems(10);

    alloc_slice data = encodeTree();
    const HashTree *itree = HashTree::fromData(data);
    tree = itree;
    checkTree(10);

//    cerr << "#### Before:\n";
//    tree.dump(cerr);

    for (int i = 10; i < 20; i++) {
        //cerr << "\n#### Add " << i << ":\n";
        tree.set(keys[i], values.get(uint32_t(i)));
//        tree.dump(cerr);
        checkTree(i+1);
    }

    for (int i = 0; i <= 5; ++i) {
//        cerr << "\n#### Remove " << (3*i + 2) << ":\n";
        CHECK(tree.remove(keys[3*i + 2]));
//        tree.dump(cerr);
        CHECK(tree.count() == 19 - i);
    }
    tree.dump(cerr);
}


TEST_CASE_METHOD(HashTreeTests, "HashTree Re-Encode Delta", "[HashTree]") {
    static const unsigned N = 50;
    createItems(2*N);
    insertItems(N);

    alloc_slice data = encodeTree();
    const HashTree *itree = HashTree::fromData(data);
    tree = itree;

    for (unsigned i = N; i < N + 10; i++)
        tree.set(keys[i], values.get(uint32_t(i)));
    for (unsigned i = 2; i < N + 5; i += 3)
        CHECK(tree.remove(keys[i]));

    tree.dump(cerr);

    Encoder enc;
    enc.amend(data, false);
    enc.suppressTrailer();
    tree.writeTo(enc);
    alloc_slice delta = enc.finish();

    cerr << "Original is " << data.size << " bytes encoded:\t" << data.hexString() << "\n";
    cerr << "Delta is " << delta.size << " bytes encoded:\t" << data.hexString() << "\n";

    alloc_slice full = encodeTree();
    cerr << "Full rewrite would be " << full.size << " bytes encoded.\n";

    alloc_slice total(data.size + delta.size);
    memcpy((void*)&total[0],         data.buf, data.size);
    memcpy((void*)&total[data.size], delta.buf, delta.size);

    itree = HashTree::fromData(total);
    cerr << "\nFinal immutable tree:\n";
    itree->dump(cerr);
}


TEST_CASE("Perf TreeSearch", "[.Perf]") {
    static const int kSamples = 500000;

    // Convert JSON array into a dictionary keyed by _id:
    auto input = readTestFile("1000people.fleece");
    if (!input)
        abort();
    std::vector<alloc_slice> names;
    auto people = Value::fromData(input, kFLTrusted).asArray();

    MutableHashTree tree;

    unsigned nPeople = 0;
    for (Array::iterator i(people); i; ++i) {
        auto person = i.value().asDict();
        auto key = person.get("guid"_sl).asString();
        names.emplace_back(key);
        tree.set(key, person);
        if (++nPeople >= 1000)
            break;
    }

    Encoder enc;
    enc.suppressTrailer();
    tree.writeTo(enc);
    alloc_slice treeData = enc.finish();
    const HashTree *imTree = HashTree::fromData(treeData);

    Benchmark bench;

    for (int i = 0; i < kSamples; i++) {
        slice keys[100];
        for (int k = 0; k < 100; k++)
            keys[k] = names[ random() % names.size() ];
        bench.start();
        {
            for (int k = 0; k < 100; k++) {
                Value person = imTree->get(keys[k]);
                //const Value *person = tree.get(keys[k]);
                if (!person)
                    abort();
            }
        }
        bench.stop();

        //std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    bench.printReport();
}
