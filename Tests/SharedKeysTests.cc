//
// SharedKeysTests.cc
//
// Copyright 2016-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

#include "FleeceTests.hh"
#include "FleeceImpl.hh"
#include "Path.hh"
#include "Doc.hh"
#include <iostream>
#include <limits.h>

using namespace std;
using namespace fleece::impl;


TEST_CASE("basic", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    CHECK(sk->count() == 0);
}


TEST_CASE("eligibility", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    int key;
    CHECK( sk->encodeAndAdd(""_sl, key));
    CHECK( sk->encodeAndAdd("x"_sl, key));
    CHECK( sk->encodeAndAdd("aZ_019-"_sl, key));
    CHECK( sk->encodeAndAdd("abcdefghijklmnop"_sl, key));
    CHECK( sk->encodeAndAdd("-"_sl, key));
    CHECK(!sk->encodeAndAdd("@"_sl, key));
    CHECK(!sk->encodeAndAdd("abc.jpg"_sl, key));
    CHECK(!sk->encodeAndAdd("abcdefghijklmnopq"_sl, key));
    CHECK(!sk->encodeAndAdd("two words"_sl, key));
    CHECK(!sk->encodeAndAdd("aççents"_sl, key));
    CHECK(!sk->encodeAndAdd("☠️"_sl, key));
}


TEST_CASE("encode", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    int key;
    CHECK( sk->encodeAndAdd("zero"_sl, key));
    CHECK(key == 0);
    CHECK(sk->count() == 1);
    CHECK( sk->encodeAndAdd("one"_sl, key));
    CHECK(key == 1);
    CHECK(sk->count() == 2);
    CHECK( sk->encodeAndAdd("two"_sl, key));
    CHECK(key == 2);
    CHECK(sk->count() == 3);
    CHECK(!sk->encodeAndAdd("@"_sl, key));
    CHECK(sk->count() == 3);
    CHECK( sk->encodeAndAdd("three"_sl, key));
    CHECK(key == 3);
    CHECK(sk->count() == 4);
    CHECK( sk->encodeAndAdd("four"_sl, key));
    CHECK(key == 4);
    CHECK(sk->count() == 5);
    CHECK( sk->encodeAndAdd("two"_sl, key));
    CHECK(key == 2);
    CHECK(sk->count() == 5);
    CHECK( sk->encodeAndAdd("zero"_sl, key));
    CHECK(key == 0);
    CHECK(sk->count() == 5);
    CHECK(sk->byKey() == (std::vector<slice>{"zero", "one", "two", "three", "four"}));
}


TEST_CASE("decode", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    int key;
    CHECK( sk->encodeAndAdd("zero"_sl, key));
    CHECK( sk->encodeAndAdd("one"_sl, key));
    CHECK( sk->encodeAndAdd("two"_sl, key));
    CHECK( sk->encodeAndAdd("three"_sl, key));
    CHECK( sk->encodeAndAdd("four"_sl, key));

    CHECK(sk->decode(2) == "two"_sl);
    CHECK(sk->decode(0) == "zero"_sl);
    CHECK(sk->decode(3) == "three"_sl);
    CHECK(sk->decode(1) == "one"_sl);
    CHECK(sk->decode(4) == "four"_sl);

    CHECK(sk->decode(5) == nullslice);
    CHECK(sk->decode(2047) == nullslice);
    CHECK(sk->decode(INT_MAX) == nullslice);
}


TEST_CASE("revertToCount", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    int key;
    CHECK( sk->encodeAndAdd("zero"_sl, key));
    CHECK( sk->encodeAndAdd("one"_sl, key));
    CHECK( sk->encodeAndAdd("two"_sl, key));
    CHECK( sk->encodeAndAdd("three"_sl, key));
    CHECK( sk->encodeAndAdd("four"_sl, key));

    sk->revertToCount(3);

    CHECK(sk->count() == 3);
    CHECK(sk->decode(3) == nullslice);
    CHECK(sk->decode(4) == nullslice);
    CHECK(sk->byKey() == (std::vector<slice>{"zero", "one", "two"}));
    CHECK( sk->encodeAndAdd("zero"_sl, key));
    CHECK(key == 0);
    CHECK( sk->encodeAndAdd("three"_sl, key));
    CHECK(key == 3);

    sk->revertToCount(3); // no-op
    CHECK(sk->count() == 3);
    CHECK(sk->byKey() == (std::vector<slice>{"zero", "one", "two"}));

    sk->revertToCount(0);
    CHECK(sk->count() == 0);
    CHECK(sk->byKey() == (std::vector<slice>{}));
    CHECK( sk->encodeAndAdd("three"_sl, key));
    CHECK(key == 0);
}


TEST_CASE("many keys", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    for (int i = 0; i < SharedKeys::kMaxCount; i++) {
        CHECK(sk->count() == (size_t)i);
        char str[10];
        sprintf(str, "K%d", i);
        int key;
        sk->encodeAndAdd(slice(str), key);
        REQUIRE(key == i);
    }

    // Check that max capacity reached:
    int key;
    CHECK(!sk->encodeAndAdd("foo"_sl, key));

    // Read them back:
    for (int i = 0; i < SharedKeys::kMaxCount; i++) {
        char str[10];
        sprintf(str, "K%d", i);
        CHECK(sk->decode(i) == slice(str));
    }
}


#pragma mark - PERSISTENCE:



// Very simple single-writer transactional storage of a single blob.
class Client {
public:
    static void reset() {
        sTransactionOwner = nullptr;
        sCommittedStorage = nullslice;
        sNumberOfWrites = 0;
    }
    static unsigned numberOfWrites() {return sNumberOfWrites;}

    slice read()  {
        return _written ? _pendingStorage : sCommittedStorage;
    }

    void write(slice data) {
        REQUIRE(sTransactionOwner == this);
        _written = true;
        _pendingStorage = data;
    }

    void begin() {
        REQUIRE(sTransactionOwner == nullptr);
        sTransactionOwner = this;
        _written = false;
    }

    void end(bool commit) {
        REQUIRE(sTransactionOwner == this);
        if (commit && _written) {
            sCommittedStorage = _pendingStorage;
            ++sNumberOfWrites;
        }
        _written = false;
        sTransactionOwner = nullptr;
    }

private:
    bool _written {false};
    alloc_slice _pendingStorage;

    static Client* sTransactionOwner;
    static alloc_slice sCommittedStorage;
    static unsigned sNumberOfWrites;
};

Client* Client::sTransactionOwner = nullptr;
alloc_slice Client::sCommittedStorage;
unsigned Client::sNumberOfWrites = 0;


// PersistentSharedKeys implementation that stores data in a Client
class MockPersistentSharedKeys : public PersistentSharedKeys {
public:
    MockPersistentSharedKeys(Client &client)
    :_client(client)
    { }

protected:
    virtual bool read() override {
        //fprintf(stderr, "SK %p: READ\n", this);
        return loadFrom(_client.read());
    }

    virtual void write(slice encodedData) override {
        //fprintf(stderr, "SK %p: WRITE\n", this);
        _client.write(encodedData);
    }

private:
    Client &_client;
};


TEST_CASE("basic persistence", "[SharedKeys]") {
    Client::reset();
    Client client1;
    MockPersistentSharedKeys sk1(client1);
    Client client2;
    MockPersistentSharedKeys sk2(client2);
    int key;

    // Client 1 in a transaction...
    client1.begin();
    sk1.transactionBegan();
    REQUIRE(sk1.encodeAndAdd("zero"_sl, key));
    CHECK(key == 0);
    REQUIRE(sk1.encodeAndAdd("one"_sl, key));
    CHECK(key == 1);
    CHECK(sk1.decode(0) == "zero"_sl);
    CHECK(sk1.decode(1) == "one"_sl);

    // Client 2 can't see the changes yet
    CHECK(sk2.decode(0) == nullslice);
    CHECK(sk2.decode(1) == nullslice);

    SECTION("commit") {
        // Client 1 commits:
        sk1.save();
        client1.end(true);
        sk1.transactionEnded();
        CHECK(Client::numberOfWrites() == 1);

        SECTION("just checking") {
            CHECK(sk1.decode(0) == "zero"_sl);
            CHECK(sk1.decode(1) == "one"_sl);
            CHECK(sk2.decode(0) == "zero"_sl);
            CHECK(sk2.decode(1) == "one"_sl);
        }

        SECTION("second transaction") {
            // Now client 2 starts a transaction (without having seen client 1's changes yet.)
            client2.begin();
            sk2.transactionBegan();
            REQUIRE(sk2.encodeAndAdd("two"_sl, key));
            CHECK(key == 2);
            CHECK(sk2.decode(2) == "two"_sl);

            SECTION("second commit") {
                // Client 2 commits:
                sk2.save();
                client2.end(true);
                sk2.transactionEnded();
                CHECK(Client::numberOfWrites() == 2);

                CHECK(sk1.decode(0) == "zero"_sl);
                CHECK(sk1.decode(1) == "one"_sl);
                CHECK(sk1.decode(2) == "two"_sl);
                CHECK(sk2.decode(0) == "zero"_sl);
                CHECK(sk2.decode(1) == "one"_sl);
                CHECK(sk2.decode(2) == "two"_sl);
            }
            SECTION("second aborts") {
                // Client 2 aborts:
                sk2.revert();
                client2.end(false);
                sk2.transactionEnded();
                CHECK(Client::numberOfWrites() == 1);

                CHECK(sk1.decode(0) == "zero"_sl);
                CHECK(sk1.decode(1) == "one"_sl);
                CHECK(sk1.decode(2) == nullslice);
                CHECK(sk2.decode(0) == "zero"_sl);
                CHECK(sk2.decode(1) == "one"_sl);
                CHECK(sk2.decode(2) == nullslice);
            }
        }

    }
    SECTION("abort") {
        // Client 1 aborts:
        sk1.revert();
        client1.end(false);
        sk1.transactionEnded();
        CHECK(Client::numberOfWrites() == 0);

        CHECK(sk1.decode(0) == nullslice);
        CHECK(sk1.decode(1) == nullslice);
        CHECK(sk2.decode(0) == nullslice);
        CHECK(sk2.decode(1) == nullslice);
    }
    SECTION("failed commit") {
        // Client 1 tries to commit but fails:
        sk1.save();
        client1.end(false);
        sk1.revert();
        sk1.transactionEnded();
        CHECK(Client::numberOfWrites() == 0);

        CHECK(sk1.decode(0) == nullslice);
        CHECK(sk1.decode(1) == nullslice);
        CHECK(sk2.decode(0) == nullslice);
        CHECK(sk2.decode(1) == nullslice);
    }
}


TEST_CASE("preserve existing keys on abort", "[SharedKeys]") {
    // Issue CBL-1707: "Keys in SharedKeys were reverted and released while they are still in used"
    Client::reset();
    Client client1;
    MockPersistentSharedKeys sk1(client1);
    int key;

    // Create stable keys
    client1.begin();
    sk1.transactionBegan();
    REQUIRE(sk1.encodeAndAdd("zero"_sl, key));
    CHECK(key == 0);
    REQUIRE(sk1.encodeAndAdd("one"_sl, key));
    CHECK(key == 1);
    slice zeroString = sk1.decode(0);
    CHECK(zeroString == "zero"_sl);
    slice oneString = sk1.decode(1);
    CHECK(oneString == "one"_sl);

    // and commit them
    sk1.save();
    client1.end(true);
    sk1.transactionEnded();

    // create unstable keys
    client1.begin();
    sk1.transactionBegan();
    REQUIRE(sk1.encodeAndAdd("Zorro"_sl, key));
    CHECK(key == 2);
    REQUIRE(sk1.encodeAndAdd("Oona"_sl, key));
    CHECK(key == 3);

    // Client aborts, obliterating the unstable keys:
    sk1.revert();
    client1.end(false);
    sk1.transactionEnded();

    // Check that the stable key strings still exist at the same addresses:
    CHECK(zeroString == "zero"_sl);
    CHECK(oneString == "one"_sl);
    CHECK(sk1.decode(0).buf == zeroString.buf);
    CHECK(sk1.decode(1).buf == oneString.buf);
}


#pragma mark - TESTING WITH ENCODERS:


TEST_CASE("encoding", "[SharedKeys]") {
    fprintf(stderr,">>>>enter\n");
    Retained<SharedKeys> sk = new SharedKeys();
    Encoder enc;
    enc.setSharedKeys(sk);
    enc.beginDictionary();
    enc.writeKey("type");
    enc.writeString("animal");
    enc.writeKey("mass");
    enc.writeDouble(123.456);
    enc.writeKey("_attachments");
    enc.beginDictionary();
    enc.writeKey("thumbnail.jpg");
    enc.writeData("xxxxxx"_sl);
    enc.writeKey("type");
    enc.writeBool(true);
    enc.endDictionary();
    enc.endDictionary();
    Retained<Doc> doc = enc.finishDoc();

    REQUIRE(sk->byKey() == (vector<slice>{"type", "mass", "_attachments"}));

    //Value::dump(encoded, cerr);
    REQUIRE(doc->data().hexString() == "46616e696d616c00280077be9f1a2fdd5e404d7468756d626e61696c2e6a70675678787878787800700200003800800e800870030000801b000180190002800b8007");

    const Dict *root = doc->asDict();
    REQUIRE(root);
    CHECK(root->sharedKeys() == sk);

    SECTION("manual lookup") {
        int typeKey, attsKey;
        REQUIRE(sk->encode("type"_sl, typeKey));
        REQUIRE(sk->encode("_attachments"_sl, attsKey));

        const Value *v = root->get(typeKey);
        REQUIRE(v);
        REQUIRE(v->asString() == "animal"_sl);

        REQUIRE(root->get("type"_sl) == v);

        const Dict *atts = root->get(attsKey)->asDict();
        REQUIRE(atts);
        REQUIRE(atts->get(typeKey) != nullptr);
        REQUIRE(atts->get("thumbnail.jpg"_sl) != nullptr);
    }
    SECTION("Dict::key lookup") {
        // Use a Dict::key:
        Dict::key typeKey("type"_sl), attsKey("_attachments"_sl);

        const Value *v = root->get(typeKey);
        REQUIRE(v);
        REQUIRE(v->asString() == "animal"_sl);
        const Dict *atts = root->get(attsKey)->asDict();
        REQUIRE(atts);
        REQUIRE(atts->get("thumbnail.jpg"_sl) != nullptr);
        REQUIRE(atts->get(typeKey) != nullptr);
        REQUIRE(atts->get(attsKey) == nullptr);

        // Try a Dict::key that can't be mapped to an integer:
        Dict::key thumbKey("thumbnail.jpg"_sl);
        REQUIRE(atts->get(thumbKey) != nullptr);
    }
    SECTION("Path lookup") {
        Path attsTypePath("_attachments.type");
        const Value *t = attsTypePath.eval(root);
        REQUIRE(t != nullptr);
        REQUIRE(t->type() == kBoolean);
    }
    SECTION("One-shot path lookup") {
        const Value *t = Path::eval("_attachments.type"_sl, root);
        REQUIRE(t != nullptr);
        REQUIRE(t->type() == kBoolean);
    }
    fprintf(stderr,">>>>exit\n");
}


TEST_CASE("big JSON encoding", "[SharedKeys]") {
    Retained<SharedKeys> sk = new SharedKeys();
    Encoder enc;
    enc.setSharedKeys(sk);
    auto input = readTestFile(kBigJSONTestFileName);
    JSONConverter jr(enc);
    jr.encodeJSON(input);
    enc.end();
    alloc_slice encoded = enc.finish();

    REQUIRE(sk->count() == 22);

    int nameKey;
    REQUIRE(sk->encode("name"_sl, nameKey));

    auto root = Value::fromTrustedData(encoded)->asArray();
    auto person = root->get(33)->asDict();
    const Value *name = person->get(nameKey);
    std::string nameStr = (std::string)name->asString();
    REQUIRE(nameStr == std::string("Janet Ayala"));
}
