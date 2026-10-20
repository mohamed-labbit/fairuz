//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Provides a standalone diagnostic JSON probe.
//===----------------------------------------------------------------------===//

#include "Diag/diagnostic.hpp"

#include <iostream>
#include <locale>

struct GroupedNumbers : std::numpunct<char> {
    char do_thousands_sep() const override { return ','; }
    std::string do_grouping() const override { return "\3"; }
};

int main()
{
    using namespace fairuz;
    std::locale::global(std::locale(std::locale::classic(), new GroupedNumbers));
    diagnostic::DiagnosticEngine engine;
    auto source = std::make_shared<SourceManager>("path\"\\\nم", "نص\t\"\\\r\n");
    engine.set_source(source);
    std::string detail = "detail\"\\\n\t";
    detail.push_back('\0');
    detail += "م😀";
    auto id = engine.report_deferred(diagnostic::Severity::ERROR, { 1, 2, 3, 0 }, ErrorCode::INVALID_CHARACTER, detail);
    engine.add_note(id, -12345, "note\n\"\\");
    engine.add_suggestion(id, "suggestion\t\"\\");
    engine.add_frame(id, source, { 1, 2, 3, 0 }, "function\"\nم");
    for (auto code : all_error_codes) engine.report_deferred(diagnostic::Severity::FATAL, { }, code);
    engine.set_source(std::make_shared<SourceManager>(std::string("bad\xff"), std::string("\xed\xa0\x80")));
    engine.report_deferred(diagnostic::Severity::FATAL, { 1, 1, 1, 0 }, static_cast<ErrorCode>(0xFFFF), std::string("\xc0\xaf"));
    std::cout << engine.to_json();
}
