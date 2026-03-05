#ifndef _DIAGNOSTIC_HPP
#define _DIAGNOSTIC_HPP

#include "macros.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <vector>

namespace mylang {
namespace diagnostic {

class DiagnosticEngine {
public:
    enum class Severity : int {
        NOTE,
        FATAL,
        ERROR,
        WARNING
    };

    struct Diagnostic {
        Severity severity;
        std::int32_t line, column;
        std::int32_t length;
        std::string message;
        std::string code; // Error code like E0001
        std::vector<std::string> suggestions;
        std::vector<std::pair<std::int32_t, std::string>> notes; // Additional context
    };

    DiagnosticEngine() = default;

    constexpr void emit(std::string const& msg, Severity const sv = Severity::ERROR) { emitError(msg, sv); }

    [[noreturn]] constexpr void panic(std::string const& msg) { _panic(msg); }

    void report(Severity const sev, std::int32_t const line, std::int32_t const col,
        std::int32_t const len, std::string const& msg, std::string const& code = "");

    void addSuggestion(std::string const& suggestion);

    void addNote(std::int32_t line, std::string const& note);

    std::string toJSON() const;

    // Beautiful terminal output with colors
    void prettyPrint() const;

private:
    std::vector<Diagnostic> Diagnostics_;

    void emitError(std::string const& msg, Severity const sv);

    [[noreturn]] void _panic(std::string const& msg) const;

    static std::string svToStr(Severity const sv);

    std::vector<std::string> splitLines(std::string const& text) const;
};

inline DiagnosticEngine engine;

}
}

#endif // _DIAGNOSTIC_HPP
