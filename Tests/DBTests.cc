//
//  DBTests.cc
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
#include "MutableDict.hh"
#include "DB.hh"
#include <functional>

using namespace std;
using namespace fleece;


#ifdef FL_ESP32
extern const uint8_t k1000PeopleStart[] asm("_binary_1000people_fleece_start");
extern const uint8_t k1000PeopleEnd[]   asm("_binary_1000people_fleece_end");
#endif


class DBTests {
public:
    unique_ptr<DB> db;
#ifndef FL_ESP32
    alloc_slice populatedFrom;
#endif
    vector<alloc_slice> names;

#ifdef FL_ESP32
    static constexpr const char* kDBPath = "mmap";
#else
    static constexpr const char* kDBPath = "/tmp/DB_test.fleecedb";
    static constexpr const char* kAltDBPath = "/tmp/DB_test_alt.fleecedb";
#endif

    DBTests() {
        reopen(DB::kEraseAndWrite);
    }

    void reopen(DB::OpenMode mode =DB::kWrite) {
        db.reset();
        db.reset( new DB(kDBPath, mode) );
    }

    void populate() {
#ifdef FL_ESP32
        slice populatedFrom(k1000PeopleStart, k1000PeopleEnd);
#else
        populatedFrom = readTestFile("1000people.fleece");
#endif
        auto people = Value::fromTrustedData(populatedFrom)->asArray();

        int n = 0;
        for (Array::iterator i(people); i; ++i) {
#if FL_EMBEDDED
            if (++n > 200)
                break;
#endif
            auto person = i.value()->asDict();
            auto key = person->get("guid"_sl)->asString();
            names.emplace_back(key);
            db->put(key, DB::Insert, person);
        }
        db->commitChanges();
    }

#if FL_EMBEDDED
    static constexpr size_t kPopulatedCheckpoint = 0x37000; // because only 200 people added
#else
    static constexpr size_t kPopulatedCheckpoint = 0x10e000;
#endif

    void iterateAndCheck() {
        set<alloc_slice> keys;
        for (DB::iterator i(db.get()); i; ++i) {
            CHECK(keys.insert(alloc_slice(i.key())).second == true);

            REQUIRE(i.value());
            auto guid = i.value()->get("guid"_sl);
            REQUIRE(guid);
            CHECK(guid->asString() == i.key());
        }
        CHECK(keys == set<alloc_slice>(names.begin(), names.end()));
    }

    void update(bool verbose =false) {
        reopen();
        auto dbSize = db->checkpoint();
        if (verbose)
            cerr << "Database is " << dbSize << " bytes\n";

        db->remove(names[123]);
        names.erase(names.begin() + 123);

        MutableDict *eleven = db->getMutable(names[11]);
        REQUIRE(eleven);
        if (verbose)
            cerr << "Eleven was: " << eleven->toJSONString() << "\n";
        REQUIRE(eleven->get("name"_sl) != nullptr);
        CHECK(eleven->get("name"_sl)->asString() == "Dollie Reyes"_sl);

        eleven->set("name"_sl, "Eleven"_sl);
        eleven->set("age"_sl, 12);
        eleven->set("about"_sl, "REDACTED"_sl);
        if (verbose)
            cerr << "\nEleven is now: " << eleven->toJSONString() << "\n\n";
        db->commitChanges();
    }

    void modifyFile(function<void(FILE*)> callback) {
        db.reset(); // close DB

#ifdef FL_ESP32
        esp_mapped_slice map(kDBPath);
        FILE *f = map.open("r+*");
#else
        FILE *f = fopen(kDBPath, "r+");
#endif
        REQUIRE(f);
        callback(f);
        fclose(f);
    }
};


constexpr size_t DBTests::kPopulatedCheckpoint;


TEST_CASE_METHOD(DBTests, "Create DB", "[DB]") {
    populate();

    reopen();

    for (auto name : names) {
        auto value = db->get(name);
        REQUIRE(value);
        REQUIRE(value->asDict());
        auto guid = value->asDict()->get("guid"_sl);
        REQUIRE(guid);
        CHECK(guid->asString() == name);
    }
}


TEST_CASE_METHOD(DBTests, "Iterate DB", "[DB]") {
    populate();
    iterateAndCheck();
}


TEST_CASE_METHOD(DBTests, "Small Update DB", "[DB]") {
    populate();
    auto checkpoint1 = db->checkpoint();
    update(true);
    iterateAndCheck();

    auto checkpoint2 = db->checkpoint();
    CHECK(checkpoint2 > checkpoint1);
    CHECK(db->previousCheckpoint() == checkpoint1);

    cerr << "Looking at previous checkpoint\n";
    DB olderdb(*db, db->previousCheckpoint());
    CHECK(olderdb.checkpoint() == checkpoint1);
    CHECK(olderdb.previousCheckpoint() == 0);
    const Dict *eleven = olderdb.get(names[11]);
    REQUIRE(eleven);
    cerr << "\nEleven was: " << eleven->toJSONString() << "\n";
    auto name = eleven->get("name"_sl);
    REQUIRE(name);
    CHECK(name->asString() == "Dollie Reyes"_sl);
}


#ifndef FL_ESP32
TEST_CASE_METHOD(DBTests, "Export DB to new file", "[DB]") {
    populate();
    cerr << "Original database is " << db->checkpoint() << " bytes\n";
    update();
    cerr << "Updated database is " << db->checkpoint() << " bytes\n";
    db->writeTo(kAltDBPath);
    db.reset();
    db.reset( new DB(kAltDBPath, DB::kReadOnly) );
    cerr << "Exported database is " << db->checkpoint() << " bytes\n";
    iterateAndCheck();
}
#endif


TEST_CASE_METHOD(DBTests, "Corrupt DB header", "[DB]") {
    populate();
    update(false);

    modifyFile([](FILE *f) {
        fseeko(f, 0, SEEK_SET);
        fputc(0x00, f);
    });

    CHECK_THROWS_AS(reopen(), FleeceException);
}

TEST_CASE_METHOD(DBTests, "Corrupt DB all trailers", "[DB]") {
    populate();

    modifyFile([](FILE *f) {
        fseeko(f, -1, SEEK_END);
        fputc(0x00, f);
    });

    CHECK_THROWS_AS(reopen(), FleeceException);
}

TEST_CASE_METHOD(DBTests, "Corrupt DB by appending", "[DB]") {
    populate();
    update(false);
    CHECK(db->checkpoint() == kPopulatedCheckpoint);

    modifyFile([](FILE *f) {
        fseeko(f, 0, SEEK_END);
        fputs("O HAI! IM IN UR DATABASE, APPENDIN UR DATAZ", f);
    });

    reopen();
    CHECK(db->isDamaged());
    CHECK(db->checkpoint() == kPopulatedCheckpoint);

    MutableDict *eleven = db->getMutable(names[11]);
    REQUIRE(eleven);
    CHECK(eleven->get("name"_sl)->asString() == "Eleven"_sl);
}


TEST_CASE_METHOD(DBTests, "Corrupt DB by overwriting trailer", "[DB]") {
    populate();
    auto checkpoint1 = db->checkpoint();
    update(false);
    auto checkpoint2 = db->checkpoint();
    CHECK(checkpoint2 > checkpoint1);

    modifyFile([](FILE *f) {
        fseeko(f, -1, SEEK_END);
        fputc(0x00, f);
    });

    // Verify file reopens to previous (first) checkpoint:
    reopen();
    CHECK(db->isDamaged());
    CHECK(db->checkpoint() == checkpoint1);

    // The changes should be gone since that checkpoint was damaged:
    MutableDict *eleven = db->getMutable(names[11]);
    REQUIRE(eleven);
    CHECK(eleven->get("name"_sl)->asString() == "Dollie Reyes"_sl);
}
