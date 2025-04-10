//
// CaseListReporter.hh
//
// Copyright 2024-Present Couchbase, Inc.
//
// Use of this software is governed by the Business Source License included
// in the file licenses/BSL-Couchbase.txt.  As of the Change Date specified
// in that file, in accordance with the Business Source License, use of this
// software will be governed by the Apache License, Version 2.0, included in
// the file licenses/APL2.txt.
//

// This is a modified version of ConsoleReporter.hh from the Catch2 source.
//
//              Copyright Catch2 Authors
// Distributed under the Boost Software License, Version 1.0.
//   (See accompanying file LICENSE.txt or copy at
//        https://www.boost.org/LICENSE_1_0.txt)

// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "catch_amalgamated.hpp"

namespace fleece_Catch {
    using namespace Catch;
    
    // Fwd decls
    class TablePrinter;

    class CaseListReporter : public StreamingReporterBase {
        Detail::unique_ptr<TablePrinter> m_tablePrinter;

    public:
        CaseListReporter(ReporterConfig&& config, bool quiet = false);
        ~CaseListReporter() override;
        static std::string getDescription();

        void noMatchingTestCases( StringRef unmatchedSpec ) override;
        void reportInvalidTestSpec( StringRef arg ) override;

        void assertionStarting(AssertionInfo const&) override;

        void assertionEnded(AssertionStats const& _assertionStats) override;

        void sectionStarting(SectionInfo const& _sectionInfo) override;
        void sectionEnded(SectionStats const& _sectionStats) override;

        void benchmarkPreparing( StringRef name ) override;
        void benchmarkStarting(BenchmarkInfo const& info) override;
        void benchmarkEnded(BenchmarkStats<> const& stats) override;
        void benchmarkFailed( StringRef error ) override;

        void testCasePartialStarting(TestCaseInfo const& testInfo,
                                     uint64_t partNumber) override;

        void testCaseStarting(TestCaseInfo const& _testInfo) override;
        void testCaseEnded(TestCaseStats const& _testCaseStats) override;

        void testRunEnded(TestRunStats const& _testRunStats) override;
        void testRunStarting(TestRunInfo const& _testRunInfo) override;

    private:
        void lazyPrint();

        void lazyPrintWithoutClosingBenchmarkTable();
        void lazyPrintRunInfo();
        void printTestCaseAndSectionHeader();

        void printClosedHeader(std::string const& _name);
        void printOpenHeader(std::string const& _name);

        // if string has a : in first line will set indent to follow it on
        // subsequent lines
        void printHeaderString(std::string const& _string, std::size_t indent = 0);

        void printTotalsDivider(Totals const& totals);

        std::string_view lineOfChars(char c, size_t count = 0) const;

        std::vector<std::string> m_failedTestCases;
        bool m_headerPrinted = false;
        bool m_testRunInfoPrinted = false;
        bool m_ignoreNextPartial = false;
        bool m_quiet = false;
    };


    class QuietCaseListReporter : public CaseListReporter {
    public:
        QuietCaseListReporter(ReporterConfig&& config)
        :CaseListReporter(std::move(config), true) { }

        static std::string getDescription();
    };

} 
