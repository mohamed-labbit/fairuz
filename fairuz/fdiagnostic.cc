//
// fdiagnostic.cc
//

#include "fdiagnostic.hpp"
#include "flexer.hpp" // for FileManager::get_line_at() — kept out of the
#include "fmacros.hpp"
// header to avoid the circular include (flexer.hpp
// includes fdiagnostic.hpp)

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unistd.h>
#include <wchar.h>

namespace fairuz::diagnostic {

Source::Source(std::string file_path, std::string contents)
    : path(file_path.empty() ? "<input>" : std::move(file_path))
    , text(std::move(contents))
    , lines { 0 }
{
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n')
            lines.push_back(i + 1);
}

std::string_view Source::line(u32 number) const
{
    if (number == 0 || number > lines.size())
        return { };
    size_t from = lines[number - 1];
    size_t to = number < lines.size() ? lines[number] - 1 : text.size();
    if (to > from && text[to - 1] == '\r')
        --to;
    return std::string_view(text).substr(from, to - from);
}

void DiagnosticEngine::set_source(lex::FileManager const* fm)
{
    if (fm == nullptr) {
        m_source.reset();
        return;
    }
    auto const& buffer = fm->buffer();
    m_source = std::make_shared<Source>(fm->get_path(), buffer.empty() ? "" : std::string(buffer.data(), buffer.len()));
}

char const* error_type_for(ErrorCode code)
{
    /// TODO:
    (void)code;
    return "";
}

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

// Diagnostics must never throw another diagnostic while decoding bad input.
u32 next_codepoint(std::string_view text, size_t& offset)
{
    auto first = static_cast<unsigned char>(text[offset++]);
    if (first < 0x80)
        return first;
    int extra = first >= 0xC2 && first <= 0xDF ? 1 : first >= 0xE0 && first <= 0xEF ? 2
        : first >= 0xF0 && first <= 0xF4                                            ? 3
                                                                                    : 0;
    if (!extra || offset + extra > text.size())
        return 0xFFFD;
    u32 cp = first & ((1u << (6 - extra)) - 1);
    for (int i = 0; i < extra; ++i) {
        auto byte = static_cast<unsigned char>(text[offset + i]);
        if ((byte & 0xC0) != 0x80)
            return 0xFFFD;
        cp = (cp << 6) | (byte & 0x3F);
    }
    offset += extra;
    if (cp < (extra == 1 ? 0x80u : extra == 2 ? 0x800u
                                              : 0x10000u)
        || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return 0xFFFD;
    return cp;
}

std::string json_string(std::string_view text)
{
    std::ostringstream out;
    out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\')
            out << '\\' << c;
        else if (c < 0x20)
            out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else
            out << c;
    }
    out << '"';
    return out.str();
}

// Use a private UTF-8 locale for terminal cell widths, without changing the
// process locale (number parsing and embedding applications rely on it).
int cell_width(u32 cp)
{
    static locale_t locale = [] {
        auto result = newlocale(LC_CTYPE_MASK, "C.UTF-8", nullptr);
        if (!result)
            result = newlocale(LC_CTYPE_MASK, "en_US.UTF-8", nullptr);
        return result;
    }();
    if (!locale)
        return 1;
    auto previous = uselocale(locale);
    int width = wcwidth(static_cast<wchar_t>(cp));
    uselocale(previous);
    return std::max(0, width);
}

} // namespace

/*DiagnosticEngine::DiagnosticId DiagnosticEngine::report(
    Severity const sev, SrcLoc const loc, u16 err_code, std::string const& code)
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

DiagnosticEngine::DiagnosticId DiagnosticEngine::report_deferred(
    Severity const sev, SrcLoc const loc, ErrorCode err_code, std::string const& code)
{
    if (sev != Severity::FATAL && m_error_count >= LIMIT)
        return INVALID_ID;

    DiagnosticId const id = static_cast<DiagnosticId>(m_diagnostics.size());
    m_diagnostics.push_back({ sev, loc, err_code, code, { }, { }, m_source, { } });

    if (sev == Severity::FATAL) {
        m_error_count++;
        _panic("");
    }

    if (sev == Severity::ERROR) {
        m_error_count++;
    } else if (sev == Severity::WARNING) {
        m_warning_count++;
    }

    return id;
}

void DiagnosticEngine::add_suggestion(std::string const& suggestion)
{
    if (!m_diagnostics.empty())
        m_diagnostics.back().suggestions.push_back(suggestion);
}

void DiagnosticEngine::add_note(i32 line, std::string const& note)
{
    if (!m_diagnostics.empty())
        m_diagnostics.back().notes.push_back({ line, note });
}

void DiagnosticEngine::add_suggestion(DiagnosticId id, std::string const& suggestion)
{
    if (id == INVALID_ID || id >= m_diagnostics.size())
        return;
    m_diagnostics[id].suggestions.push_back(suggestion);
}

void DiagnosticEngine::add_note(DiagnosticId id, i32 line, std::string const& note)
{
    if (id == INVALID_ID || id >= m_diagnostics.size())
        return;
    m_diagnostics[id].notes.push_back({ line, note });
}

void DiagnosticEngine::add_frame(DiagnosticId id, SourcePtr source, SrcLoc loc, std::string function)
{
    if (id == INVALID_ID || id >= m_diagnostics.size())
        return;
    m_diagnostics[id].traceback.push_back({ std::move(source), loc, std::move(function) });
}

void DiagnosticEngine::emit_error(std::string const& msg, Severity const sv)
{
    std::cerr << sv_to_str(sv) << ": " << escape_terminal(msg) << "\n";
    if (sv == Severity::FATAL)
        panic("");
}

[[noreturn]] void DiagnosticEngine::_panic(std::string const& msg) const
{
    pretty_print();
    if (!msg.empty())
        std::cerr << terminal_color(Color::RESET) << escape_terminal(msg) << "\n";
    throw DiagnosticAbort();
}

std::string DiagnosticEngine::sv_to_str(Severity const sv)
{
    switch (sv) {
    case Severity::NOTE: return terminal_color(Color::BOLD) + terminal_color(Color::CYAN) + "note";
    case Severity::FATAL: return terminal_color(Color::BOLD) + terminal_color(Color::RED) + "fatal";
    case Severity::ERROR: return terminal_color(Color::BOLD) + terminal_color(Color::RED) + "error";
    case Severity::WARNING: return terminal_color(Color::BOLD) + terminal_color(Color::YELLOW) + "warning";
    default: return terminal_color(Color::BOLD) + "unknown";
    }
}

std::vector<std::string> DiagnosticEngine::split_lines(std::string const& text) const
{
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;

    while (std::getline(ss, line))
        lines.push_back(line);

    return lines;
}

// Columns and lengths are Unicode code points, not UTF-8 byte offsets.
void DiagnosticEngine::print_snippet(SourcePtr const& source, SrcLoc const& loc) const
{
    if (!source || loc.line == 0 || loc.line > source->lines.size())
        return;
    auto line = source->line(loc.line);
    std::string rendered;
    size_t cells = 0, start_cells = 0, end_cells = 0, column = 0;
    size_t target = loc.column ? loc.column - 1 : 0;
    size_t end = target + std::max<u16>(loc.length, 1);
    // Bound output even for generated source with enormous physical lines.
    size_t window_start = target > 100 ? target - 100 : 0;
    if (window_start) {
        rendered = "...";
        cells = 3;
    }
    for (size_t offset = 0; offset < line.size();) {
        size_t from = offset;
        u32 cp = next_codepoint(line, offset);
        if (column < window_start) {
            ++column;
            continue;
        }
        if (column == target)
            start_cells = cells;
        if (column == end)
            end_cells = cells;
        if (column > target + 140) {
            rendered += "...";
            break;
        }
        if (cp == '\t') {
            size_t width = 4 - cells % 4;
            rendered += std::string(width, ' ');
            cells += width;
        } else if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)
            || cp == 0x061C || cp == 0x200E || cp == 0x200F
            || (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2066 && cp <= 0x2069)) {
            std::ostringstream escaped;
            escaped << (cp <= 0xFF ? "\\x" : "\\u") << std::hex << std::uppercase
                    << std::setw(cp <= 0xFF ? 2 : 4) << std::setfill('0') << cp;
            rendered += escaped.str();
            cells += escaped.str().size();
        } else {
            rendered += cp == 0xFFFD ? "\xEF\xBF\xBD" : std::string(line.substr(from, offset - from));
            cells += cell_width(cp);
        }
        ++column;
    }
    if (target >= column)
        start_cells = cells;
    if (end >= column)
        end_cells = cells;
    size_t width = end_cells > start_cells ? end_cells - start_cells : 1;
    auto number = std::to_string(loc.line);
    std::cerr << "  " << number << " | " << rendered << '\n'
              << "  " << std::string(number.size(), ' ') << " | " << std::string(start_cells, ' ')
              << terminal_color(Color::RED) << std::string(std::min<size_t>(width, 140), '^')
              << terminal_color(Color::RESET) << '\n';
}

std::string DiagnosticEngine::to_json() const
{
    std::ostringstream out;
    out << '[';
    bool first = true;
    for (auto const& d : m_diagnostics) {
        if (!first)
            out << ',';
        first = false;
        out << "{\"severity\":" << static_cast<int>(d.severity)
            << ",\"type\":" << json_string(error_type_for(d.err_code))
            << ",\"errorCode\":" << error_type_for(d.err_code)
            << ",\"path\":" << json_string(d.source ? d.source->path : "")
            << ",\"line\":" << d.src_loc.line << ",\"column\":" << d.src_loc.column
            << ",\"length\":" << d.src_loc.length
            << ",\"message\":" << json_string(error_message_for(d.err_code))
            << ",\"code\":" << json_string(d.code)
            << ",\"source\":" << json_string(d.source ? d.source->line(d.src_loc.line) : "")
            << ",\"suggestions\":[";
        for (size_t i = 0; i < d.suggestions.size(); ++i) {
            if (i)
                out << ',';
            out << json_string(d.suggestions[i]);
        }
        out << "],\"notes\":[";
        for (size_t i = 0; i < d.notes.size(); ++i) {
            if (i)
                out << ',';
            out << "{\"line\":" << d.notes[i].first << ",\"message\":" << json_string(d.notes[i].second) << '}';
        }
        out << "],\"traceback\":[";
        for (size_t i = 0; i < d.traceback.size(); ++i) {
            if (i)
                out << ',';
            auto const& frame = d.traceback[i];
            out << "{\"path\":" << json_string(frame.source ? frame.source->path : "")
                << ",\"line\":" << frame.location.line << ",\"column\":" << frame.location.column
                << ",\"function\":" << json_string(frame.function)
                << ",\"source\":" << json_string(frame.source ? frame.source->line(frame.location.line) : "") << '}';
        }
        out << "]}";
    }
    out << "]\n";
    return out.str();
}

void DiagnosticEngine::pretty_print() const
{
    if (m_json_output)
        return; // The CLI emits one complete JSON array on exit.
    for (; m_printed < m_diagnostics.size(); ++m_printed) {
        auto const& diag = m_diagnostics[m_printed];
        if (!diag.traceback.empty()) {
            std::cerr << "Traceback (most recent call last):\n";
            for (auto const& frame : diag.traceback) {
                std::cerr << "  File " << json_string(frame.source ? frame.source->path : "<input>")
                          << ", line " << frame.location.line << ", in " << escape_terminal(frame.function) << '\n';
                print_snippet(frame.source, frame.location);
            }
        }
        if (diag.source)
            std::cerr << escape_terminal(diag.source->path) << ": ";
        std::cerr << sv_to_str(diag.severity) << terminal_color(Color::RESET) << ": "
                  << error_type_for(diag.err_code) << ": " << error_message_for(diag.err_code);
        if (!diag.code.empty())
            std::cerr << ": " << escape_terminal(diag.code);
        std::cerr << '\n';
        if (diag.src_loc.line > 0) {
            std::cerr << "  --> line " << diag.src_loc.line << ':' << diag.src_loc.column << '\n';
            if (diag.traceback.empty())
                print_snippet(diag.source, diag.src_loc);
        }
        for (auto const& suggestion : diag.suggestions)
            std::cerr << "help: " << escape_terminal(suggestion) << '\n';
        for (auto const& [line, message] : diag.notes) {
            std::cerr << "note: " << escape_terminal(message) << '\n';
            if (line > 0)
                std::cerr << "  --> line " << line << '\n';
        }
        std::cerr << '\n';
    }
    if (is_saturated() && !m_printed_limit) {
        m_printed_limit = true;
        std::cerr << "note: further errors suppressed (limit: " << LIMIT << ")\n";
    }
}

} // namespace fairuz::diagnostic
