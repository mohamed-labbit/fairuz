/// diagnostic.cc

#include "fdiagnostic.hpp"
#include "flexer.hpp" // for Fa_FileManager::get_line_at() — kept out of the
#include "fmacros.hpp"
// header to avoid the circular include (flexer.hpp
// includes fdiagnostic.hpp)

#include <exception>
#include <iostream>
#include <sstream>

namespace fairuz::diagnostic {

Fa_DiagnosticEngine::DiagnosticId Fa_DiagnosticEngine::report(
    Severity const sev, Fa_SourceLocation const loc, u16 err_code, std::string const& code)
{
    DiagnosticId const id = report_deferred(sev, loc, err_code, code);

    if (sev == Severity::ERROR || sev == Severity::FATAL || sev == Severity::WARNING) {
        pretty_print();
        m_diagnostics.clear();
        if (sev == Severity::FATAL)
            panic("exited on fatal error");
    }

    return id;
}

Fa_DiagnosticEngine::DiagnosticId Fa_DiagnosticEngine::report_deferred(
    Severity const sev, Fa_SourceLocation const loc, u16 err_code, std::string const& code)
{
    if (sev == Severity::ERROR && m_error_count >= LIMIT)
        _panic("Too many errors (error limit = 20)");

    /*
    if (sev == Severity::WARNING && m_warning_count >= LIMIT)
        return INVALID_ID;
    */

    DiagnosticId const id = static_cast<DiagnosticId>(m_diagnostics.size());
    m_diagnostics.push_back({ sev, loc, err_code, code, { }, { } });

    if (sev == Severity::ERROR || sev == Severity::FATAL) {
        m_error_count += 1;
    } else if (sev == Severity::WARNING) {
        m_warning_count += 1;
    }

    return id;
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

void Fa_DiagnosticEngine::add_suggestion(DiagnosticId id, std::string const& suggestion)
{
    if (id == INVALID_ID || id >= m_diagnostics.size())
        return;
    m_diagnostics[id].suggestions.push_back(suggestion);
}

void Fa_DiagnosticEngine::add_note(DiagnosticId id, i32 line, std::string const& note)
{
    if (id == INVALID_ID || id >= m_diagnostics.size())
        return;
    m_diagnostics[id].notes.push_back({ line, note });
}

void Fa_DiagnosticEngine::emit_error(std::string const& msg, Severity const sv)
{
    std::cerr << sv_to_str(sv) << ": " << msg << "\n";
    if (sv == Severity::FATAL)
        panic("");
}

[[noreturn]] void Fa_DiagnosticEngine::_panic(std::string const& msg) const
{
    std::cerr << Color::BOLD << Color::RED << "fatal" << Color::RESET << ": " << msg << "\n";
    std::terminate();
}

std::string Fa_DiagnosticEngine::sv_to_str(Severity const sv)
{
    switch (sv) {
    case Severity::NOTE: return Color::BOLD + Color::CYAN + "note";
    case Severity::FATAL: return Color::BOLD + Color::RED + "fatal";
    case Severity::ERROR: return Color::BOLD + Color::RED + "error";
    case Severity::WARNING: return Color::BOLD + Color::YELLOW + "warning";
    default: return Color::BOLD + "unknown";
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

// Renders the offending source line with a caret (or underline, for
// spans wider than one column) beneath the error location, e.g.:
//
//   12 |     نتيجة := ١٠ / صفر
//      |                  ^^^^
//
// No-op if no source has been registered (set_source() never called) or
// the location is empty/out of range — callers always get at least the
// existing "--> line N:col" text either way, this is purely additive.
void Fa_DiagnosticEngine::print_snippet(Fa_SourceLocation const& loc) const
{
    if (m_source == nullptr || loc.line == 0)
        return;

    Fa_StringRef line_text = m_source->get_line_at(loc.line);
    if (line_text.empty())
        return; // line out of range, or file has no such line — say nothing
                // rather than print a misleading blank snippet

    std::string line_str(line_text.data(), line_text.len());
    // Fa_FileManager::get_line_at() slices on '\n'; a trailing '\r' from
    // CRLF source files would otherwise print as a stray character after
    // the line and misalign the caret row beneath it.
    if (!line_str.empty() && line_str.back() == '\r')
        line_str.pop_back();

    std::string line_num_str = std::to_string(loc.line);
    std::string gutter(line_num_str.size(), ' ');

    std::cerr << "  " << Color::BOLD << Color::BLUE << line_num_str << " |" << Color::RESET
              << " " << line_str << "\n";

    // column is 1-based (matches how the lexer/parser report it
    // elsewhere in this file, e.g. the "--> line N:col" text above);
    // guard against 0 so the caret math below can't underflow.
    u32 caret_col = loc.column > 0 ? loc.column - 1 : 0;
    u32 caret_len = loc.length > 0 ? loc.length : 1;

    // Clamp the underline so a stale/mismatched length (e.g. a
    // multi-line span whose stored `length` outruns this single
    // printed line) can't spill past the actual line content.
    if (caret_col < line_str.size() && caret_col + caret_len > line_str.size())
        caret_len = static_cast<u32>(line_str.size() - caret_col);

    std::cerr << "  " << gutter << " |" << Color::RESET << " " << std::string(caret_col, ' ')
              << Color::BOLD << Color::RED << std::string(caret_len, '^') << Color::RESET << "\n";
}

std::string Fa_DiagnosticEngine::to_json() const
{
    std::stringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < m_diagnostics.size(); i += 1) {
        Diagnostic const& d = m_diagnostics[i];
        ss << "  {\n";
        ss << "    \"severity\": " << static_cast<i32>(d.severity) << ",\n";
        ss << "    \"line\": " << d.src_loc.line << ",\n";
        ss << "    \"column\": " << d.src_loc.column << ",\n";
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
        std::string sev_str = sv_to_str(diag.severity);

        if (m_source != nullptr)
            std::cerr << Color::BOLD << Color::RESET << m_source->get_path() << ": " << Color::RESET;
        std::cerr << sev_str << Color::RESET << ":";

        /*
        if (!diag.code.empty())
            std::cerr << "[" << diag.code << "]";
        */

        std::cerr << Color::RESET << " " << error_message_for(diag.err_code) << "\n";

        if (diag.src_loc.line > 0) {
            std::cerr << "  --> line " << diag.src_loc.line << ":" << diag.src_loc.column << "\n";
            print_snippet(diag.src_loc);
        }

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
        std::cerr << Color::BOLD << Color::YELLOW << "warning" << Color::RESET << ": " << m_error_count << " errors reported, "
                  << "further errors suppressed (limit: " << LIMIT << ")\n\n";
}

} // namespace fairuz::diagnostic
