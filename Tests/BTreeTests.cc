//
// BTreeTests.cc
//
// Copyright © 2018 Couchbase. All rights reserved.
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

#include "FleeceTests.hh"
#include "Fleece.hh"
#include "MutableBTree.hh"
#include "Encoder.hh"

using namespace std;
using namespace fleece;

static const char* kDigits[10] = {"zero", "one", "two", "three", "four", "five", "six",
                                  "seven", "eight", "nine"};


class BTreeTests {
public:
    MutableBTree tree;
    vector<alloc_slice> keys;
    const Array* values;
    alloc_slice _valueBuf;

    void createItems(size_t N) {
        Encoder enc;
        enc.beginArray(N);
        for (size_t i = 0; i < N; i++)
            enc.writeInt(i);
        enc.endArray();
        _valueBuf = enc.extractOutput();
        values = Value::fromTrustedData(_valueBuf)->asArray();

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

    void insertItem(size_t i, bool verbose, bool check) {
        if (verbose)
            cerr << "\n##### Inserting #" << (i)
            << ", " << hex << keys[i].hash() << dec << "\n";
        tree.set(keys[i], values->get(uint32_t(i)));
        if (verbose)
            tree.dump(cerr);
        if (check) {
            CHECK(tree.count() == i + 1);
            for (ssize_t j = i; j >= 0; --j)
                CHECK(tree.get(keys[j]) == values->get(uint32_t(i)));
        }
    }

    void insertItems(size_t N =0, bool verbose= false, bool check =false) {
        if (N == 0)
            N = keys.size();
        for (size_t i = 0; i < N; i++)
            insertItem(i, verbose, check);
    }

    void insertItemsReverse(size_t N =0, bool verbose= false, bool check =false) {
        if (N == 0)
            N = keys.size();
        for (size_t i = N; i > 0; i--)
            insertItem(i-1, verbose, check);
    }

    void checkTree(size_t N) {
        CHECK(tree.count() == N);
        for (size_t i = 0; i < N; i++) {
            auto value = tree.get(keys[i]);
            REQUIRE(value);
            CHECK(value->isInteger());
            CHECK(value->asInt() == values->get(uint32_t(i))->asInt());
        }
    }

    void checkIterator(size_t N) {
        /*
        set<slice> keysSeen;
        for (MutableBTree::iterator i(tree); i; ++i) {
            //cerr << "--> " << i.key().asString() << "\n";
            CHECK(keysSeen.insert(i.key()).second); // insert, and make sure it's unique
            REQUIRE(i.value() != nullptr);
            CHECK(i.value()->type() == kNumber);
        }
        CHECK(keysSeen.size() == N);
         */
    }

    alloc_slice encodeTree() {
        Encoder enc;
        tree.writeTo(enc);
        return enc.extractOutput();
    }

};


#pragma mark - TEST CASES:



TEST_CASE_METHOD(BTreeTests, "Empty MutableBTree", "[BTree]") {
    CHECK(tree.count() == 0);
    CHECK(tree.get(alloc_slice("foo")) == 0);
    CHECK(!tree.remove(alloc_slice("foo")));
}


TEST_CASE_METHOD(BTreeTests, "Tiny MutableBTree Insert", "[BTree]") {
    createItems(1);
    auto key = keys[0];
    auto val = values->get(0);
    tree.set(key, val);

    auto storedVal = tree.get(key);
    CHECK(val->isEqual(storedVal));
    CHECK(tree.count() == 1);

    tree.dump(cerr);

    // Check that insertion-with-callback passes value to callback and supports failure:
    const Value *existingVal = nullptr;
    CHECK(!tree.insert(key, [&](const Value *val) {existingVal = val; return nullptr;}));
    CHECK(existingVal == storedVal);
}


TEST_CASE_METHOD(BTreeTests, "Bigger MutableBTree Insert", "[BTree]") {
    static constexpr int N = 1000;
    createItems(N);
    insertItems();
    tree.dump(cerr);
    checkTree(N);
}


TEST_CASE_METHOD(BTreeTests, "Bigger MutableBTree Insert Reverse Order", "[BTree]") {
    static constexpr int N = 1000;
    createItems(N);
    insertItemsReverse();
    tree.dump(cerr);
    checkTree(N);
}


TEST_CASE_METHOD(BTreeTests, "Tiny MutableBTree Remove", "[BTree]") {
    createItems(1);
    auto key = keys[0];
    auto val = values->get(0);

    tree.set(key, val);
    CHECK(tree.remove(key));
    CHECK(tree.get(key) == 0);
    CHECK(tree.count() == 0);
}


TEST_CASE_METHOD(BTreeTests, "Bigger MutableBTree Remove", "[BTree]") {
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
        auto value = tree.get(keys[i]);
        if (i % 3)
            CHECK(values->get(uint32_t(i))->isEqual(value));
        else
            CHECK(value == nullptr);
    }
    CHECK(tree.count() == N - 1 - (N / 3));
}


TEST_CASE_METHOD(BTreeTests, "MutableBTree Iterate", "[BTree]") {
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


TEST_CASE_METHOD(BTreeTests, "Tiny MutableBTree Write", "[BTree]") {
    createItems(10);
    auto key = keys[8];
    auto val = values->get(8);
    tree.set(key, val);

    alloc_slice data = encodeTree();
    REQUIRE(data.size == 20); // could change if encoding changes
    cerr << data.size << " bytes encoded: " << data.hexString() << "\n";
    Value::dump(data, cerr);

    // Now read it as an immutable BTree:
    const BTree tree = BTree::fromData(data);
    CHECK(tree.count() == 1);
    auto value = tree.get(key);
    REQUIRE(value);
    CHECK(value->isInteger());
    CHECK(value->asInt() == 8);
}


TEST_CASE_METHOD(BTreeTests, "Bigger MutableBTree Write", "[BTree]") {
    static constexpr int N = 100;
    createItems(N);
    insertItems();

    alloc_slice data = encodeTree();
//    cerr << "Data: " << data.hexString() << "\noffset = " << offset << " of " << data.size << "\n";

    // Now read it as an immutable BTree:
    const BTree itree = BTree::fromData(data);
    CHECK(itree.count() == N);
}


TEST_CASE_METHOD(BTreeTests, "Tiny BTree Mutate", "[BTree]") {
    createItems(10);
    tree.set(keys[9], values->get(9));

    alloc_slice data = encodeTree();
    const BTree itree = BTree::fromData(data);
    itree.dump(cerr);

    // Wrap in a MutableBTree and get the key:
    tree = itree;

    tree.dump(cerr);
    CHECK(tree.count() == 1);
    auto value = tree.get(keys[9]);
    REQUIRE(value);
    CHECK(value->isInteger());
    CHECK(value->asInt() == 9);

    // Modify the value for the key:
    tree.set(keys[9], values->get(3));

    tree.dump(cerr);
    CHECK(tree.count() == 1);
    value = tree.get(keys[9]);
    REQUIRE(value);
    CHECK(value->asInt() == 3);
}


TEST_CASE_METHOD(BTreeTests, "Bigger BTree Mutate by replacing", "[BTree]") {
    createItems(100);
    insertItems(100);

    alloc_slice data = encodeTree();
    const BTree itree = BTree::fromData(data);
//    itree->dump(cerr);

    // Wrap in a MutableBTree and get the key:
    tree = itree;

//    tree.dump(cerr);
    checkTree(100);

    for (int i = 0; i < 10; ++i) {
        // Modify the value for the key:
        int old = i*i, nuu = 99-old;
        //cerr << "\n#### Set key " << old << " to " << nuu << ":\n";
        tree.set(keys[old], values->get(nuu));

        //tree.dump(cerr);
        CHECK(tree.count() == 100);
        auto value = tree.get(keys[old]);
        REQUIRE(value);
        CHECK(value->asInt() == nuu);
    }
}


TEST_CASE_METHOD(BTreeTests, "Bigger BTree Mutate by inserting", "[BTree]") {
    createItems(20);
    insertItems(10);

    alloc_slice data = encodeTree();
    const BTree itree = BTree::fromData(data);
    tree = itree;
    checkTree(10);

//    cerr << "#### Before:\n";
//    tree.dump(cerr);

    for (int i = 10; i < 20; i++) {
        //cerr << "\n#### Add " << i << ":\n";
        tree.set(keys[i], values->get(uint32_t(i)));
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


TEST_CASE_METHOD(BTreeTests, "BTree Re-Encode Delta", "[BTree]") {
    static const unsigned N = 50;
    createItems(2*N);
    insertItems(N);

    alloc_slice data = encodeTree();
    const BTree itree = BTree::fromData(data);
    tree = itree;

    for (unsigned i = N; i < N + 10; i++)
        tree.set(keys[i], values->get(uint32_t(i)));
    for (unsigned i = 2; i < N + 5; i += 3)
        CHECK(tree.remove(keys[i]));

    tree.dump(cerr);

    Encoder enc;
    enc.setBase(data);
    enc.reuseBaseStrings();
    tree.writeTo(enc);
    alloc_slice delta = enc.extractOutput();

    cerr << "Original is " << data.size << " bytes encoded:\t" << data.hexString() << "\n";
    cerr << "Delta is " << delta.size << " bytes encoded:\t" << data.hexString() << "\n";

    alloc_slice full = encodeTree();
    cerr << "Full rewrite would be " << full.size << " bytes encoded.\n";

    alloc_slice total(data.size + delta.size);
    memcpy((void*)&total[0],         data.buf, data.size);
    memcpy((void*)&total[data.size], delta.buf, delta.size);

    auto itree2 = BTree::fromData(total);
    cerr << "\nFinal immutable tree:\n";
    itree2.dump(cerr);
}
