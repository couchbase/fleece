//
//  DBTests.cc
//  Fleece
//
//  Created by Jens Alfke on 4/30/18.
//Copyright © 2018 Couchbase. All rights reserved.
//

#include "FleeceTests.hh"
#include "Fleece.hh"
#include "DB.hh"
#include <unistd.h>

using namespace std;
using namespace fleece;


class DBTests {
public:
    unique_ptr<DB> db;
    alloc_slice populatedFrom;
    vector<alloc_slice> names;

    static constexpr const char* kDBPath = "/tmp/DB_test.fleecedb";

    DBTests() {
        unlink(kDBPath);
        db.reset( new DB(kDBPath) );
    }

    void populate() {
        populatedFrom = readFile(kTestFilesDir "1000people.fleece");
        auto people = Value::fromTrustedData(populatedFrom)->asArray();

        for (Array::iterator i(people); i; ++i) {
            auto person = i.value()->asDict();
            auto key = person->get("guid"_sl)->asString();
            names.emplace_back(key);
            db->put(key, DB::Insert, person);
        }
    }
};


TEST_CASE_METHOD(DBTests, "Create DB", "[DB]") {
    populate();
    db->saveChanges();

    db.reset( new DB(kDBPath) );

    for (auto name : names) {
        auto value = db->get(name)->asDict();
        auto guid = value->get("guid"_sl);
        REQUIRE(guid);
        CHECK(guid->asString() == name);
    }
}


TEST_CASE_METHOD(DBTests, "Small Update DB", "[DB]") {
    populate();
    db->saveChanges();
    db.reset( new DB(kDBPath) );
    cerr << "Database is " << db->dataSize() << " bytes\n";

    db->remove(names[123]);
    db->saveChanges();
    cerr << "Database is " << db->dataSize() << " bytes after save\n";
}
