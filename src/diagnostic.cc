#include "../include/diagnostic.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace mylang::diagnostic {

void DiagnosticEngine::report(Severity const sev, std::int32_t const line, std::int32_t const col,
    std::int32_t const len, std::string const& msg, std::string const& code)
{
    Diagnostics_.push_back({ sev, line, col, len, msg, code });
}

void DiagnosticEngine::addSuggestion(std::string const& suggestion)
{
    if (!Diagnostics_.empty())
        Diagnostics_.back().suggestions.push_back(suggestion);
}

void DiagnosticEngine::addNote(std::int32_t line, std::string const& note)
{
    if (!Diagnostics_.empty())
        Diagnostics_.back().notes.push_back({ line, note });
}

void DiagnosticEngine::emitError(std::string const& msg, Severity const sv)
{
    std::cerr << svToStr(sv) << ": " << msg << std::endl;
    if (sv == Severity::FATAL)
        throw std::runtime_error("");
}

[[noreturn]] void DiagnosticEngine::_panic(std::string const& msg) const
{
    std::cerr << "fatal: Program panic: " << msg << std::endl;
    std::terminate(); // Or throw std::runtime_error(msg);
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
        Diagnostic const& diag = Diagnostics_[i];
        ss << "  {\n";
        ss << "    \"severity: " << static_cast<std::int32_t>(diag.severity) << ",\n";
        ss << "    \"line\": " << diag.line << ",\n";
        ss << "    \"column\": " << diag.column << ",\n";
        ss << "    \"message\": \"" << diag.message << "\",\n";
        ss << "    \"code\": \"" << diag.code << "\"\n";
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
    for (Diagnostic const& diag : Diagnostics_) {
        std::string sevStr;
        std::string color;

        switch (diag.severity) {
        case Severity::NOTE:
            sevStr = "note";
            color = Color::CYAN;
            break;
        case Severity::WARNING:
            sevStr = "warning";
            color = Color::MAGENTA;
            break;
        case Severity::ERROR:
            sevStr = "error";
            color = Color::RED;
            break;
        case Severity::FATAL:
            sevStr = "fatal";
            color = Color::RED;
            break;
        }

        std::cout << color << sevStr << Color::RESET;
        if (!diag.code.empty())
            std::cout << "[" << diag.code << "]";

        std::cout << ": " << diag.message << "\n";
        std::cout << "  --> line " << diag.line << ":" << diag.column << "\n";

        /*
        std::vector<std::string> lines = splitLines(SourceCode_);
        if (diag.line > 0 && diag.line <= lines.size())
        {
          std::cout << "   |\n";
          std::cout << std::setw(3) << diag.line << " | " << lines[diag.line - 1] << "\n";
          std::cout << "   | " << std::string(diag.column - 1, ' ') << color << std::string(std::max(1, diag.length), '^')
          << Color::RESET << '\n';
        }
        */

        if (!diag.suggestions.empty()) {
            std::cout << Color::MAGENTA << "Help" << Color::RESET << std::endl;
            for (std::string const& sugg : diag.suggestions)
                std::cout << "    • " << sugg << "\n";
        }

        for (auto const& [noteLine, noteMsg] : diag.notes) {
            std::cout << Color::CYAN << "note:" << Color::RESET << std::endl;
            std::cout << "  --> line " << noteLine << "\n";
        }

        std::cout << "\n";
    }
}

} // namespace mylang::diagnostic
