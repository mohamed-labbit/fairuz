/// diagnostic.cc

#include "../include/diagnostic.hpp"
#include "../include/macros.hpp"

#include <exception>
#include <iostream>
#include <sstream>

namespace mylang::diagnostic {

void DiagnosticEngine::report(Severity const sev, uint32_t const line, uint16_t const col, uint16_t err_code, std::string const& code)
{
    if (sev == Severity::ERROR && ErrorCount_ >= LIMIT)
        return;
    if (sev == Severity::WARNING && WarningCount_ >= LIMIT)
        return;

    Diagnostics_.push_back({ sev, line, col, err_code, code, { }, { } });

    if (sev == Severity::ERROR || sev == Severity::FATAL)
        ++ErrorCount_;
    else if (sev == Severity::WARNING)
        ++WarningCount_;
}

void DiagnosticEngine::addSuggestion(std::string const& suggestion)
{
    if (!Diagnostics_.empty())
        Diagnostics_.back().suggestions.push_back(suggestion);
}

void DiagnosticEngine::addNote(int32_t line, std::string const& note)
{
    if (!Diagnostics_.empty())
        Diagnostics_.back().notes.push_back({ line, note });
}

void DiagnosticEngine::emitError(std::string const& msg, Severity const sv)
{
    std::cerr << svToStr(sv) << ": " << msg << "\n";
    if (sv == Severity::FATAL)
        throw std::runtime_error(msg);
}

[[noreturn]] void DiagnosticEngine::_panic(std::string const& msg) const
{
    std::cerr << Color::BOLD << Color::RED << "fatal" << Color::RESET << ": " << msg << "\n";
    std::terminate();
}

std::string DiagnosticEngine::svToStr(Severity const sv)
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

std::vector<std::string> DiagnosticEngine::splitLines(std::string const& text) const
{
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line))
        lines.push_back(line);
    return lines;
}

std::string DiagnosticEngine::toJSON() const
{
    std::stringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < Diagnostics_.size(); ++i) {
        Diagnostic const& d = Diagnostics_[i];
        ss << "  {\n";
        ss << "    \"severity\": " << static_cast<int32_t>(d.severity) << ",\n";
        ss << "    \"line\": " << d.line << ",\n";
        ss << "    \"column\": " << d.column << ",\n";
        ss << "    \"message\": \"" << errorMessageFor(d.err_code) << "\",\n";
        ss << "    \"code\": \"" << d.code << "\"\n";
        ss << "  }";
        if (i + 1 < Diagnostics_.size())
            ss << ",";
        ss << "\n";
    }
    ss << "]\n";
    return ss.str();
}

void DiagnosticEngine::prettyPrint() const
{
    if (Diagnostics_.empty())
        return;

    for (Diagnostic const& diag : Diagnostics_) {
        std::string color;
        std::string sevStr;

        switch (diag.severity) {
        case Severity::NOTE:
            sevStr = "note";
            color = Color::CYAN;
            break;
        case Severity::WARNING:
            sevStr = "warning";
            color = Color::YELLOW;
            break;
        case Severity::ERROR:
            sevStr = "error";
            color = Color::RED;
            break;
        case Severity::FATAL:
            sevStr = "fatal";
            color = Color::RED;
            break;
        default:
            sevStr = "unknown";
            color = Color::RESET;
            break;
        }

        std::cerr << Color::BOLD << color << sevStr << Color::RESET;
        if (!diag.code.empty())
            std::cerr << "[" << diag.code << "]";
        std::cerr << ": " << errorMessageFor(diag.err_code) << "\n";

        if (diag.line > 0)
            std::cerr << "  --> line " << diag.line << ":" << diag.column << "\n";

        if (!diag.suggestions.empty()) {
            std::cerr << Color::BOLD << Color::CYAN << "help" << Color::RESET << ":\n";
            for (std::string const& sugg : diag.suggestions)
                std::cerr << "    • " << sugg << "\n";
        }

        for (auto const& [noteLine, noteMsg] : diag.notes) {
            std::cerr << Color::BOLD << Color::CYAN << "note" << Color::RESET << ": " << noteMsg << "\n";
            if (noteLine > 0)
                std::cerr << "  --> line " << noteLine << "\n";
        }

        std::cerr << "\n";
    }

    if (isSaturated())
        std::cerr << Color::BOLD << Color::YELLOW << "warning" << Color::RESET << ": " << ErrorCount_ << " errors reported, "
                  << "further errors suppressed (limit: " << LIMIT << ")\n\n";
}

} // namespace mylang::diagnostic
