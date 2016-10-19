//
//  SharedKeysTests.cc
//  Fleece
//
//  Created by Jens Alfke on 10/19/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include <iostream>

using namespace std;


TEST_CASE("basic") {
    SharedKeys sk;
    CHECK(sk.count() == 0);
}


TEST_CASE("eligibility") {
    SharedKeys sk;
    int key;
    CHECK( sk.encode(""_sl, key));
    CHECK( sk.encode("x"_sl, key));
    CHECK( sk.encode("aZ_019-"_sl, key));
    CHECK( sk.encode("abcdefghijklmnop"_sl, key));
    CHECK( sk.encode("-"_sl, key));
    CHECK(!sk.encode("@"_sl, key));
    CHECK(!sk.encode("abc.jpg"_sl, key));
    CHECK(!sk.encode("abcdefghijklmnopq"_sl, key));
    CHECK(!sk.encode("two words"_sl, key));
    CHECK(!sk.encode("aççents"_sl, key));
    CHECK(!sk.encode("☠️"_sl, key));
}


TEST_CASE("encode") {
    SharedKeys sk;
    int key;
    CHECK( sk.encode("zero"_sl, key));
    CHECK(key == 0);
    CHECK(sk.count() == 1);
    CHECK( sk.encode("one"_sl, key));
    CHECK(key == 1);
    CHECK(sk.count() == 2);
    CHECK( sk.encode("two"_sl, key));
    CHECK(key == 2);
    CHECK(sk.count() == 3);
    CHECK(!sk.encode("@"_sl, key));
    CHECK(sk.count() == 3);
    CHECK( sk.encode("three"_sl, key));
    CHECK(key == 3);
    CHECK(sk.count() == 4);
    CHECK( sk.encode("four"_sl, key));
    CHECK(key == 4);
    CHECK(sk.count() == 5);
    CHECK( sk.encode("two"_sl, key));
    CHECK(key == 2);
    CHECK(sk.count() == 5);
    CHECK( sk.encode("zero"_sl, key));
    CHECK(key == 0);
    CHECK(sk.count() == 5);
    CHECK(sk.byKey() == (std::vector<alloc_slice>{alloc_slice("zero"), alloc_slice("one"), alloc_slice("two"), alloc_slice("three"), alloc_slice("four")}));
}


TEST_CASE("decode") {
    SharedKeys sk;
    int key;
    CHECK( sk.encode("zero"_sl, key));
    CHECK( sk.encode("one"_sl, key));
    CHECK( sk.encode("two"_sl, key));
    CHECK( sk.encode("three"_sl, key));
    CHECK( sk.encode("four"_sl, key));

    CHECK(sk.decode(2) == "two"_sl);
    CHECK(sk.decode(0) == "zero"_sl);
    CHECK(sk.decode(3) == "three"_sl);
    CHECK(sk.decode(1) == "one"_sl);
    CHECK(sk.decode(4) == "four"_sl);

    CHECK(sk.decode(5) == nullslice);
    CHECK(sk.decode(2047) == nullslice);
    CHECK(sk.decode(INT_MAX) == nullslice);
}


TEST_CASE("revertToCount") {
    SharedKeys sk;
    int key;
    CHECK( sk.encode("zero"_sl, key));
    CHECK( sk.encode("one"_sl, key));
    CHECK( sk.encode("two"_sl, key));
    CHECK( sk.encode("three"_sl, key));
    CHECK( sk.encode("four"_sl, key));

    sk.revertToCount(3);

    CHECK(sk.count() == 3);
    CHECK(sk.decode(3) == nullslice);
    CHECK(sk.decode(4) == nullslice);
    CHECK(sk.byKey() == (std::vector<alloc_slice>{alloc_slice("zero"), alloc_slice("one"), alloc_slice("two")}));
    CHECK( sk.encode("zero"_sl, key));
    CHECK(key == 0);
    CHECK( sk.encode("three"_sl, key));
    CHECK(key == 3);

    sk.revertToCount(3); // no-op
    CHECK(sk.count() == 3);
    CHECK(sk.byKey() == (std::vector<alloc_slice>{alloc_slice("zero"), alloc_slice("one"), alloc_slice("two")}));

    sk.revertToCount(0);
    CHECK(sk.count() == 0);
    CHECK(sk.byKey() == (std::vector<alloc_slice>{}));
    CHECK( sk.encode("three"_sl, key));
    CHECK(key == 0);
}


TEST_CASE("many keys") {
    SharedKeys sk;
    sk.setMaxCount(1000);
    for (int i = 0; i < 1000; i++) {
        CHECK(sk.count() == (size_t)i);
        char str[10];
        sprintf(str, "K%d", i);
        int key;
        sk.encode(slice(str), key);
        REQUIRE(key == i);
    }

    // Check that max capacity reached:
    int key;
    CHECK(!sk.encode("foo"_sl, key));

    // Read them back:
    for (int i = 0; i < 1000; i++) {
        char str[10];
        sprintf(str, "K%d", i);
        CHECK(sk.decode(i) == slice(str));
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
    bool _written;
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


TEST_CASE("basic persistence") {
    Client::reset();
    Client client1;
    MockPersistentSharedKeys sk1(client1);
    Client client2;
    MockPersistentSharedKeys sk2(client2);
    int key;

    // Client 1 in a transaction...
    client1.begin();
    sk1.transactionBegan();
    REQUIRE(sk1.encode("zero"_sl, key));
    CHECK(key == 0);
    REQUIRE(sk1.encode("one"_sl, key));
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
            REQUIRE(sk2.encode("two"_sl, key));
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
