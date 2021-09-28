//
//  CaseListReporter.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 8/26/16.
//  Copyright 2016-Present Couchbase, Inc.
//
//  Use of this software is governed by the Business Source License included
//  in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
//  in that file, in accordance with the Business Source License, use of this
//  software will be governed by the Apache License, Version 2.0, included in
//  the file licenses/APL2.txt.
//

#include "catch.hpp"
#include "Stopwatch.hh"
#include <chrono>
#include <iostream>
#include <time.h>

#ifdef CASE_LIST_BACKTRACE
#include "Backtrace.hh"
#endif


/** Custom reporter that logs a line for every test file and every test case being run.
    Use CLI args "-r list" to use it. */
struct CaseListReporter : public Catch::ConsoleReporter {
    CaseListReporter( Catch::ReporterConfig const& _config )
    :   Catch::ConsoleReporter( _config )
    {
        _start = time(nullptr);
        stream << "STARTING TESTS AT " << ctime(&_start);
        stream.flush();
    }

    virtual ~CaseListReporter() override {
        auto now = time(nullptr);
        stream << "ENDED TESTS IN " << (now - _start) << "sec, AT " << ctime(&now);
        stream.flush();
    }

    static std::string getDescription() {
        return "Logs a line for every test case";
    }

    virtual void testCaseStarting( Catch::TestCaseInfo const& _testInfo ) override {
        std::string file = _testInfo.lineInfo.file;
        if (file != _curFile) {
            _curFile = file;
            auto slash = file.rfind('/');
            stream << "## " << file.substr(slash+1) << ":\n";
        }
        stream << "\t>>> " << _testInfo.name << "\n";
        _firstSection = true;
        _sectionNesting = 0;
        stream.flush();
        ConsoleReporter::testCaseStarting(_testInfo);
        _stopwatch.reset();
    }
    virtual void testCaseEnded( Catch::TestCaseStats const& _testCaseStats ) override {
        stream << "\t    [" << _stopwatch.elapsed() << " sec]\n";
        stream.flush();
        ConsoleReporter::testCaseEnded(_testCaseStats);
    }

    virtual void sectionStarting( Catch::SectionInfo const& _sectionInfo ) override {
        if (_firstSection)
            _firstSection = false;
        else {
            for (unsigned i = 0; i < _sectionNesting; ++i)
                stream << "\t";
            stream << "\t--- " << _sectionInfo.name << "\n";
            stream.flush();
        }
        ++_sectionNesting;
        ConsoleReporter::sectionStarting(_sectionInfo);
    }

    void sectionEnded( Catch::SectionStats const& sectionStats ) override {
        --_sectionNesting;
        ConsoleReporter::sectionEnded(sectionStats);
    }

#ifdef CASE_LIST_BACKTRACE
    virtual bool assertionEnded( Catch::AssertionStats const& stats ) override {
        if (stats.assertionResult.getResultType() == Catch::ResultWas::FatalErrorCondition) {
            std::cerr << "\n\n********** CRASH: "
                      << stats.assertionResult.getMessage()
                      << " **********";
            fleece::Backtrace bt(5);
            bt.writeTo(std::cerr);
            std::cerr << "\n********** CRASH **********\n";
        }
        return Catch::ConsoleReporter::assertionEnded(stats);
    }
#endif

    std::string _curFile;
    bool _firstSection;
    unsigned _sectionNesting;
    time_t _start;
    fleece::Stopwatch _stopwatch;
};

CATCH_REGISTER_REPORTER("list", CaseListReporter )
