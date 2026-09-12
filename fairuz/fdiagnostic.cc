//
// fdiagnostic.cc
//

#include "fdiagnostic.hpp"
#include "flexer.hpp" // for Fa_FileManager::get_line_at() — kept out of the
#include "fmacros.hpp"
// header to avoid the circular include (flexer.hpp
// includes fdiagnostic.hpp)

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unistd.h>

namespace fairuz::diagnostic {

namespace {

std::string const& terminal_color(std::string const& color)
{
    static std::string const empty;
    static bool const enabled = ::isatty(STDERR_FILENO) != 0
        && std::getenv("NO_COLOR") == nullptr;
    return enabled ? color : empty;
}

std::string escape_terminal(std::string_view text)
{
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string escaped;
    escaped.reserve(text.size());

    for (unsigned char ch : text) {
        if (ch < 0x20 || ch == 0x7f) {
            escaped += "\\x";
            escaped += hex[ch >> 4];
            escaped += hex[ch & 0x0f];
        } else {
            escaped += static_cast<char>(ch);
        }
    }
    return escaped;
}

} // namespace

/*Fa_DiagnosticEngine::DiagnosticId Fa_DiagnosticEngine::report(
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
}*/

Fa_DiagnosticEngine::DiagnosticId Fa_DiagnosticEngine::report_deferred(
    Severity const sev, Fa_SourceLocation const loc, u16 err_code, std::string const& code)
{
    if (sev == Severity::ERROR && m_error_count >= LIMIT)
        _panic("Too many errors (error limit = 20)");

    DiagnosticId const id = static_cast<DiagnosticId>(m_diagnostics.size());
    m_diagnostics.push_back({ sev, loc, err_code, code, { }, { } });

    if (sev == Severity::FATAL) {
        _panic("");
    }

    if (sev == Severity::ERROR) {
        m_error_count++;
    } else if (sev == Severity::WARNING) {
        m_warning_count++;
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
    std::cerr << sv_to_str(sv) << ": " << escape_terminal(msg) << "\n";
    if (sv == Severity::FATAL)
        panic("");
}

[[noreturn]] void Fa_DiagnosticEngine::_panic(std::string const& msg) const
{
    pretty_print();
    if (!msg.empty())
        std::cerr << terminal_color(Color::RESET) << escape_terminal(msg) << "\n";
    throw Fa_DiagnosticAbort();
}

std::string Fa_DiagnosticEngine::sv_to_str(Severity const sv)
{
    switch (sv) {
    case Severity::NOTE: return terminal_color(Color::BOLD) + terminal_color(Color::CYAN) + "note";
    case Severity::FATAL: return terminal_color(Color::BOLD) + terminal_color(Color::RED) + "fatal";
    case Severity::ERROR: return terminal_color(Color::BOLD) + terminal_color(Color::RED) + "error";
    case Severity::WARNING: return terminal_color(Color::BOLD) + terminal_color(Color::YELLOW) + "warning";
    default: return terminal_color(Color::BOLD) + "unknown";
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

    size_t source_col = std::min<size_t>(caret_col, line_str.size());
    size_t source_len = std::min<size_t>(caret_len, line_str.size() - source_col);
    size_t display_col = escape_terminal(
        std::string_view(line_str).substr(0, source_col)).size();
    size_t display_len = std::max<size_t>(1, escape_terminal(
        std::string_view(line_str).substr(source_col, source_len)).size());

    std::cerr << "  " << terminal_color(Color::BOLD) << terminal_color(Color::BLUE)
              << line_num_str << " |" << terminal_color(Color::RESET)
              << " " << escape_terminal(line_str) << "\n";
    std::cerr << "  " << gutter << " |" << terminal_color(Color::RESET) << " "
              << std::string(display_col, ' ') << terminal_color(Color::BOLD)
              << terminal_color(Color::RED) << std::string(display_len, '^')
              << terminal_color(Color::RESET) << "\n";
}

std::string Fa_DiagnosticEngine::to_json() const
{
    std::stringstream ss;
    ss << "[\n";
    for (size_t i = 0; i < m_diagnostics.size(); i++) {
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
            std::cerr << terminal_color(Color::BOLD) << terminal_color(Color::RESET)
                      << escape_terminal(m_source->get_path()) << ": "
                      << terminal_color(Color::RESET);

        std::cerr << sev_str << terminal_color(Color::RESET) << ":" << " "
                  << error_message_for(diag.err_code) << " "
                  << escape_terminal(diag.code) << "\n";

        if (diag.src_loc.line > 0) {
            std::cerr << "  --> line " << diag.src_loc.line << ":" << diag.src_loc.column << "\n";
            print_snippet(diag.src_loc);
        }

        if (!diag.suggestions.empty()) {
            std::cerr << terminal_color(Color::BOLD) << terminal_color(Color::CYAN)
                      << "help" << terminal_color(Color::RESET) << ":\n";
            for (std::string const& sugg : diag.suggestions)
                std::cerr << "    • " << escape_terminal(sugg) << "\n";
        }

        for (auto const& [note_line, note_msg] : diag.notes) {
            std::cerr << terminal_color(Color::BOLD) << terminal_color(Color::CYAN)
                      << "note" << terminal_color(Color::RESET) << ": "
                      << escape_terminal(note_msg) << "\n";
            if (note_line > 0)
                std::cerr << "  --> line " << note_line << "\n";
        }

        std::cerr << "\n";
    }

    if (is_saturated())
        std::cerr << terminal_color(Color::BOLD) << terminal_color(Color::YELLOW)
                  << "warning" << terminal_color(Color::RESET) << ": " << m_error_count << " errors reported, "
                  << "further errors suppressed (limit: " << LIMIT << ")\n\n";
}

} // namespace fairuz::diagnostic
