#pragma once

#include "../macros.hpp"

#include <exception>
#include <iostream>
#include <sstream>
#include <vector>

namespace mylang {
namespace diagnostic {

class DiagnosticEngine {
public:
    enum class Severity : int { NOTE,
        FATAL,
        ERROR,
        WARNING };

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

    void emit(std::string const& msg, Severity sv = Severity::ERROR) { emitError(msg, sv); }

    [[noreturn]] void panic(std::string const& msg) { _panic(msg); }

    /*
     */
    void report(Severity sev,
        std::int32_t line,
        std::int32_t col,
        std::int32_t len,
        std::string const& msg,
        std::string const& code = "")
    {
        Diagnostics_.push_back({ sev, line, col, len, msg, code });
    }

    void addSuggestion(std::string const& suggestion)
    {
        if (!Diagnostics_.empty())
            Diagnostics_.back().suggestions.push_back(suggestion);
    }

    void addNote(std::int32_t line, std::string const& note)
    {
        if (!Diagnostics_.empty())
            Diagnostics_.back().notes.push_back({ line, note });
    }

    std::string toJSON() const;

    // Beautiful terminal output with colors
    void prettyPrint() const;

private:
    std::vector<Diagnostic> Diagnostics_;

    void emitError(std::string const& msg, Severity sv)
    {
        std::cerr << svToStr(sv) << ": " << msg << std::endl;
        if (sv == Severity::FATAL)
            throw std::runtime_error("");
    }

    [[noreturn]] void _panic(std::string const& msg) const
    {
        std::cerr << "fatal: Program panic: " << msg << std::endl;
        std::terminate(); // Or throw std::runtime_error(msg);
    }

    static std::string svToStr(Severity const sv)
    {
        switch (sv) {
        case Severity::NOTE:
            return Color::BOLD + Color::CYAN + "note" + Color::RESET;
        case Severity::FATAL:
            return Color::BOLD + Color::RED + "fatal" + Color::RESET;
        case Severity::ERROR:
            return Color::BOLD + Color::RED + "error" + Color::RESET;
        case Severity::WARNING:
            return Color::BOLD + Color::YELLOW + "warning" + Color::RESET;
        default:
            return Color::BOLD + "unknown" + Color::RESET;
        }
    }

    std::vector<std::string> splitLines(std::string const& text) const
    {
        std::vector<std::string> lines;
        std::stringstream ss(text);
        std::string line;

        while (std::getline(ss, line))
            lines.push_back(line);

        return lines;
    }
};

inline DiagnosticEngine engine;

}
}
