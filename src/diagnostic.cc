/// diagnostic.cc

#include "../include/diagnostic.hpp"
#include "../include/macros.hpp"

#include <exception>
#include <iostream>
#include <sstream>

namespace fairuz::diagnostic {

void Fa_DiagnosticEngine::report(Severity const sev, u32 const line, u16 const col, u16 err_code, std::string const& code)
{
    if (sev == Severity::ERROR && error_count_ >= LIMIT)
        return;
    if (sev == Severity::WARNING && m_warning_count >= LIMIT)
        return;

    m_diagnostics.push_back({ sev, line, col, err_code, code, { }, { } });

    if (sev == Severity::ERROR || sev == Severity::FATAL)
        error_count_ += 1;
    else if (sev == Severity::WARNING)
        m_warning_count += 1;
}

void Fa_DiagnosticEngine::add_suggestion(std::string const& suggestion)
{
    if (!m_diagnostics.empty())
        m_diagnostics.back().suggestions.push_back(suggestion);
}

void Fa_DiagnosticEngine::add_note(i32 line, std::string const& note)
{
    if (!m_diagnostics.empty())
        m_diagnostics.back().notes.push_back({ line, note });
}

void Fa_DiagnosticEngine::emit_error(std::string const& msg, Severity const sv)
{
    std::cerr << sv_to_str(sv) << ": " << msg << "\n";
    if (sv == Severity::FATAL)
        throw std::runtime_error(msg);
}

[[noreturn]] void Fa_DiagnosticEngine::_panic(std::string const& msg) const
{
    std::cerr << Color::BOLD << Color::RED << "fatal" << Color::RESET << ": " << msg << "\n";
    std::terminate();
}

std::string Fa_DiagnosticEngine::sv_to_str(Severity const sv)
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

std::vector<std::string> Fa_DiagnosticEngine::split_lines(std::string const& text) const
{
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;

    while (std::getline(ss, line))
        lines.push_back(line);

    return lines;
}

std::string Fa_DiagnosticEngine::to_json() const
{
    std::stringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < m_diagnostics.size(); i += 1) {
        Diagnostic const& d = m_diagnostics[i];
        ss << "  {\n";
        ss << "    \"severity\": " << static_cast<i32>(d.severity) << ",\n";
        ss << "    \"line\": " << d.line << ",\n";
        ss << "    \"column\": " << d.m_column << ",\n";
        ss << "    \"message\": \"" << error_message_for(d.err_code) << "\",\n";
        ss << "    \"code\": \"" << d.code << "\"\n";
        ss << "  }";
        if (i + 1 < m_diagnostics.size())
            ss << ",";
        ss << "\n";
    }
    ss << "]\n";
    return ss.str();
}

void Fa_DiagnosticEngine::pretty_print() const
{
    if (m_diagnostics.empty())
        return;

    for (Diagnostic const& diag : m_diagnostics) {
        std::string color;
        std::string sev_str;

        switch (diag.severity) {
        case Severity::NOTE:
            sev_str = "note";
            color = Color::CYAN;
            break;
        case Severity::WARNING:
            sev_str = "warning";
            color = Color::YELLOW;
            break;
        case Severity::ERROR:
            sev_str = "error";
            color = Color::RED;
            break;
        case Severity::FATAL:
            sev_str = "fatal";
            color = Color::RED;
            break;
        default:
            sev_str = "unknown";
            color = Color::RESET;
            break;
        }

        std::cerr << Color::BOLD << color << sev_str << Color::RESET;
        if (!diag.code.empty())
            std::cerr << "[" << diag.code << "]";
        std::cerr << ": " << error_message_for(diag.err_code) << "\n";

        if (diag.line > 0)
            std::cerr << "  --> line " << diag.line << ":" << diag.m_column << "\n";

        if (!diag.suggestions.empty()) {
            std::cerr << Color::BOLD << Color::CYAN << "help" << Color::RESET << ":\n";
            for (std::string const& sugg : diag.suggestions)
                std::cerr << "    • " << sugg << "\n";
        }

        for (auto const& [note_line, note_msg] : diag.notes) {
            std::cerr << Color::BOLD << Color::CYAN << "note" << Color::RESET << ": " << note_msg << "\n";
            if (note_line > 0)
                std::cerr << "  --> line " << note_line << "\n";
        }

        std::cerr << "\n";
    }

    if (is_saturated())
        std::cerr << Color::BOLD << Color::YELLOW << "warning" << Color::RESET << ": " << error_count_ << " errors reported, "
                  << "further errors suppressed (limit: " << LIMIT << ")\n\n";
}

} // namespace fairuz::diagnostic
