// diagnostics_engine.h
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <iostream>


namespace diagnostics {

// ============================================================================
// Core Types and Enumerations
// ============================================================================

enum class Severity {
    Note,
    Hint,
    Warning,
    Error,
    Fatal,
    ICE  // Internal Compiler Error
};

enum class Category { Syntax, Semantic, Type, Lexical, Optimization, Deprecation, Performance, Style, Custom };

// Source location representing a position in source code
struct SourceLocation
{
    std::string filename;
    uint32_t line;
    uint32_t column;
    uint32_t offset;  // Byte offset from file start

    SourceLocation(std::string file = "", uint32_t l = 0, uint32_t c = 0, uint32_t off = 0) :
        filename(std::move(file)),
        line(l),
        column(c),
        offset(off)
    {
    }

    bool is_valid() const { return line > 0 && column > 0; }

    bool operator<(const SourceLocation& other) const
    {
        if (filename != other.filename)
            return filename < other.filename;
        if (line != other.line)
            return line < other.line;
        return column < other.column;
    }
};

// Source range representing a span in source code
struct SourceRange
{
    SourceLocation start;
    SourceLocation end;

    SourceRange() = default;
    SourceRange(SourceLocation s, SourceLocation e) :
        start(std::move(s)),
        end(std::move(e))
    {
    }
    explicit SourceRange(SourceLocation loc) :
        start(loc),
        end(loc)
    {
    }

    bool is_valid() const { return start.is_valid() && end.is_valid(); }
    bool is_single_line() const { return start.line == end.line; }
};

// Fix-it hint for automatic code correction
struct FixItHint
{
    enum class Kind { Insert, Remove, Replace };

    Kind kind;
    SourceRange range;
    std::string replacement_text;
    std::string description;

    static FixItHint insert(SourceLocation loc, std::string text, std::string desc = "")
    {
        return {Kind::Insert, SourceRange(loc), std::move(text), std::move(desc)};
    }

    static FixItHint remove(SourceRange range, std::string desc = "")
    {
        return {Kind::Remove, range, "", std::move(desc)};
    }

    static FixItHint replace(SourceRange range, std::string text, std::string desc = "")
    {
        return {Kind::Replace, range, std::move(text), std::move(desc)};
    }
};

// Highlighting style for source code snippets
enum class HighlightStyle {
    Primary,  // Main issue location
    Secondary,  // Related code
    Note,  // Additional context
    Suggestion,  // Suggested fix
    Error,  // Error location
    Warning  // Warning location
};

struct HighlightRange
{
    SourceRange range;
    HighlightStyle style;
    std::string label;

    HighlightRange(SourceRange r, HighlightStyle s, std::string l = "") :
        range(std::move(r)),
        style(s),
        label(std::move(l))
    {
    }
};

// ============================================================================
// Diagnostic Message
// ============================================================================

class Diagnostic
{
   public:
    Diagnostic(Severity sev, Category cat, std::string msg, SourceLocation loc) :
        severity_(sev),
        category_(cat),
        message_(std::move(msg)),
        primary_location_(std::move(loc))
    {
    }

    // Fluent API for building diagnostics
    Diagnostic& with_code(std::string code)
    {
        error_code_ = std::move(code);
        return *this;
    }

    Diagnostic& with_range(SourceRange range)
    {
        primary_range_ = std::move(range);
        return *this;
    }

    Diagnostic& add_highlight(SourceRange range, HighlightStyle style, std::string label = "")
    {
        highlights_.emplace_back(std::move(range), style, std::move(label));
        return *this;
    }

    Diagnostic& add_note(std::string note, std::optional<SourceLocation> loc = std::nullopt)
    {
        notes_.emplace_back(std::move(note), std::move(loc));
        return *this;
    }

    Diagnostic& add_fixit(FixItHint hint)
    {
        fixits_.push_back(std::move(hint));
        return *this;
    }

    Diagnostic& suggest_fixit(std::string description, SourceRange range, std::string replacement)
    {
        return add_fixit(FixItHint::replace(range, std::move(replacement), std::move(description)));
    }

    Diagnostic& add_related_location(SourceLocation loc, std::string msg)
    {
        related_locations_.emplace_back(std::move(loc), std::move(msg));
        return *this;
    }

    Diagnostic& with_help_text(std::string help)
    {
        help_text_ = std::move(help);
        return *this;
    }

    Diagnostic& with_url(std::string url)
    {
        documentation_url_ = std::move(url);
        return *this;
    }

    // Getters
    Severity severity() const { return severity_; }
    Category category() const { return category_; }
    const std::string& message() const { return message_; }
    const std::optional<std::string>& error_code() const { return error_code_; }
    const SourceLocation& location() const { return primary_location_; }
    const std::optional<SourceRange>& range() const { return primary_range_; }
    const std::vector<HighlightRange>& highlights() const { return highlights_; }
    const std::vector<std::pair<std::string, std::optional<SourceLocation>>>& notes() const { return notes_; }
    const std::vector<FixItHint>& fixits() const { return fixits_; }
    const std::vector<std::pair<SourceLocation, std::string>>& related_locations() const { return related_locations_; }
    const std::optional<std::string>& help_text() const { return help_text_; }
    const std::optional<std::string>& documentation_url() const { return documentation_url_; }

   private:
    Severity severity_;
    Category category_;
    std::string message_;
    std::optional<std::string> error_code_;

    SourceLocation primary_location_;
    std::optional<SourceRange> primary_range_;

    std::vector<HighlightRange> highlights_;
    std::vector<std::pair<std::string, std::optional<SourceLocation>>> notes_;
    std::vector<FixItHint> fixits_;
    std::vector<std::pair<SourceLocation, std::string>> related_locations_;

    std::optional<std::string> help_text_;
    std::optional<std::string> documentation_url_;
};

// ============================================================================
// Source Manager - Manages source code and line information
// ============================================================================

class SourceManager
{
   public:
    struct FileInfo
    {
        std::string content;
        std::vector<size_t> line_offsets;  // Start offset of each line

        void index_lines()
        {
            line_offsets.clear();
            line_offsets.push_back(0);
            for (size_t i = 0; i < content.size(); ++i)
            {
                if (content[i] == '\n')
                {
                    line_offsets.push_back(i + 1);
                }
            }
        }

        std::string get_line(uint32_t line_num) const
        {
            if (line_num == 0 || line_num > line_offsets.size())
                return "";

            size_t start = line_offsets[line_num - 1];
            size_t end = (line_num < line_offsets.size()) ? line_offsets[line_num] - 1 : content.size();

            // Remove trailing newline if present
            if (end > start && content[end - 1] == '\n')
                --end;
            if (end > start && content[end - 1] == '\r')
                --end;

            return content.substr(start, end - start);
        }
    };

    void add_source_file(const std::string& filename, std::string content)
    {
        FileInfo info{std::move(content), {}};
        info.index_lines();
        files_[filename] = std::move(info);
    }

    const FileInfo* get_file_info(const std::string& filename) const
    {
        auto it = files_.find(filename);
        return (it != files_.end()) ? &it->second : nullptr;
    }

    std::string get_line(const SourceLocation& loc) const
    {
        auto* info = get_file_info(loc.filename);
        return info ? info->get_line(loc.line) : "";
    }

   private:
    std::unordered_map<std::string, FileInfo> files_;
};

// ============================================================================
// Diagnostic Renderer - Formats and outputs diagnostics
// ============================================================================

class DiagnosticRenderer
{
   public:
    enum class ColorMode { None, ANSI, HTML };

    struct RenderOptions
    {
        ColorMode color_mode = ColorMode::ANSI;
        bool show_line_numbers = true;
        bool show_column_markers = true;
        bool show_source_snippets = true;
        bool show_fixits = true;
        bool show_notes = true;
        bool show_help = true;
        bool show_urls = true;
        uint32_t context_lines = 2;  // Lines of context around error
        uint32_t max_snippet_width = 120;
    };

    DiagnosticRenderer(const SourceManager& sm, RenderOptions opts = {}) :
        source_manager_(sm),
        options_(std::move(opts))
    {
    }

    std::string render(const Diagnostic& diag) const
    {
        std::ostringstream out;

        // Render header: severity + location + message
        render_header(out, diag);

        // Render source snippet with highlights
        if (options_.show_source_snippets && diag.location().is_valid())
        {
            render_source_snippet(out, diag);
        }

        // Render fix-it suggestions
        if (options_.show_fixits && !diag.fixits().empty())
        {
            render_fixits(out, diag);
        }

        // Render notes
        if (options_.show_notes && !diag.notes().empty())
        {
            render_notes(out, diag);
        }

        // Render related locations
        for (const auto& [loc, msg] : diag.related_locations())
        {
            render_related_location(out, loc, msg);
        }

        // Render help text
        if (options_.show_help && diag.help_text())
        {
            render_help(out, *diag.help_text());
        }

        // Render documentation URL
        if (options_.show_urls && diag.documentation_url())
        {
            out << colorize("  For more information: ", Color::Cyan) << *diag.documentation_url() << "\n";
        }

        return out.str();
    }

   private:
    enum class Color { Reset, Bold, Red, Green, Yellow, Blue, Magenta, Cyan, White };

    std::string colorize(const std::string& text, Color color) const
    {
        if (options_.color_mode != ColorMode::ANSI)
            return text;

        const char* code = "";
        switch (color)
        {
        case Color::Reset :
            code = "\033[0m";
            break;
        case Color::Bold :
            code = "\033[1m";
            break;
        case Color::Red :
            code = "\033[31m";
            break;
        case Color::Green :
            code = "\033[32m";
            break;
        case Color::Yellow :
            code = "\033[33m";
            break;
        case Color::Blue :
            code = "\033[34m";
            break;
        case Color::Magenta :
            code = "\033[35m";
            break;
        case Color::Cyan :
            code = "\033[36m";
            break;
        case Color::White :
            code = "\033[37m";
            break;
        }

        return std::string(code) + text + "\033[0m";
    }

    std::string severity_string(Severity sev) const
    {
        switch (sev)
        {
        case Severity::Note :
            return colorize("note", Color::Cyan);
        case Severity::Hint :
            return colorize("hint", Color::Blue);
        case Severity::Warning :
            return colorize("warning", Color::Yellow);
        case Severity::Error :
            return colorize("error", Color::Red);
        case Severity::Fatal :
            return colorize("fatal error", Color::Red);
        case Severity::ICE :
            return colorize("internal compiler error", Color::Magenta);
        }
        return "unknown";
    }

    void render_header(std::ostringstream& out, const Diagnostic& diag) const
    {
        const auto& loc = diag.location();

        // Format: filename:line:col: severity: message [code]
        if (loc.is_valid())
        {
            out << colorize(loc.filename, Color::Bold) << ":" << colorize(std::to_string(loc.line), Color::Bold) << ":"
                << colorize(std::to_string(loc.column), Color::Bold) << ": ";
        }

        out << severity_string(diag.severity()) << ": " << colorize(diag.message(), Color::Bold);

        if (diag.error_code())
        {
            out << " " << colorize("[" + *diag.error_code() + "]", Color::Cyan);
        }

        out << "\n";
    }

    void render_source_snippet(std::ostringstream& out, const Diagnostic& diag) const
    {
        const auto& loc = diag.location();
        std::string line = source_manager_.get_line(loc);

        if (line.empty())
            return;

        // Show line number
        if (options_.show_line_numbers)
        {
            out << colorize(std::to_string(loc.line), Color::Blue) << " | ";
        }

        out << line << "\n";

        // Show column marker
        if (options_.show_column_markers)
        {
            if (options_.show_line_numbers)
            {
                out << std::string(std::to_string(loc.line).length(), ' ') << " | ";
            }

            // Calculate spaces before marker
            uint32_t spaces = loc.column - 1;
            out << std::string(spaces, ' ');

            // Determine marker length
            uint32_t marker_len = 1;
            if (diag.range() && diag.range()->is_single_line())
            {
                marker_len = diag.range()->end.column - diag.range()->start.column;
                marker_len = std::max(1u, marker_len);
            }

            // Render marker based on severity
            char marker_char = '^';
            Color marker_color = Color::Red;

            switch (diag.severity())
            {
            case Severity::Warning :
                marker_color = Color::Yellow;
                marker_char = '~';
                break;
            case Severity::Note :
                marker_color = Color::Cyan;
                marker_char = '~';
                break;
            case Severity::Hint :
                marker_color = Color::Blue;
                marker_char = '^';
                break;
            default :
                break;
            }

            out << colorize(std::string(marker_len, marker_char), marker_color) << "\n";
        }
    }

    void render_fixits(std::ostringstream& out, const Diagnostic& diag) const
    {
        for (const auto& fixit : diag.fixits())
        {
            out << colorize("  help: ", Color::Green);

            switch (fixit.kind)
            {
            case FixItHint::Kind::Insert :
                out << "insert '" << colorize(fixit.replacement_text, Color::Green) << "'";
                break;
            case FixItHint::Kind::Remove :
                out << "remove this";
                break;
            case FixItHint::Kind::Replace :
                out << "replace with '" << colorize(fixit.replacement_text, Color::Green) << "'";
                break;
            }

            if (!fixit.description.empty())
            {
                out << " (" << fixit.description << ")";
            }

            out << "\n";
        }
    }

    void render_notes(std::ostringstream& out, const Diagnostic& diag) const
    {
        for (const auto& [note, loc] : diag.notes())
        {
            out << colorize("  note: ", Color::Cyan) << note;
            if (loc && loc->is_valid())
            {
                out << " at " << loc->filename << ":" << loc->line << ":" << loc->column;
            }
            out << "\n";
        }
    }

    void render_related_location(std::ostringstream& out, const SourceLocation& loc, const std::string& msg) const
    {
        out << colorize(loc.filename, Color::Bold) << ":" << loc.line << ":" << loc.column << ": "
            << colorize("note: ", Color::Cyan) << msg << "\n";
    }

    void render_help(std::ostringstream& out, const std::string& help) const
    {
        out << colorize("  help: ", Color::Green) << help << "\n";
    }

    const SourceManager& source_manager_;
    RenderOptions options_;
};

// ============================================================================
// Diagnostics Engine - Central management and emission
// ============================================================================

class DiagnosticsEngine
{
   public:
    struct Statistics
    {
        size_t note_count = 0;
        size_t warning_count = 0;
        size_t error_count = 0;
        size_t fatal_count = 0;
        size_t ice_count = 0;

        bool has_errors() const { return error_count > 0 || fatal_count > 0 || ice_count > 0; }
        size_t total() const { return note_count + warning_count + error_count + fatal_count + ice_count; }
    };

    struct Options
    {
        bool warnings_as_errors = false;
        bool suppress_warnings = false;
        bool suppress_notes = false;
        uint32_t max_errors = 0;  // 0 = unlimited
        std::vector<std::string> enabled_warnings;
        std::vector<std::string> disabled_warnings;
    };

    DiagnosticsEngine(SourceManager& sm, DiagnosticRenderer::RenderOptions render_opts = {}) :
        source_manager_(sm),
        renderer_(sm, std::move(render_opts))
    {
    }

    // Emit a diagnostic
    void emit(Diagnostic diag)
    {
        // Apply filtering
        if (should_suppress(diag))
            return;

        // Promote warnings to errors if requested
        if (options_.warnings_as_errors && diag.severity() == Severity::Warning)
        {
            diag = Diagnostic(Severity::Error, diag.category(), diag.message(), diag.location());
        }

        // Update statistics
        update_statistics(diag);

        // Check error limit
        if (options_.max_errors > 0 && stats_.error_count >= options_.max_errors)
        {
            if (!error_limit_reached_)
            {
                error_limit_reached_ = true;
                std::cerr << "Error limit reached. Stopping compilation.\n";
            }
            return;
        }

        // Store diagnostic
        diagnostics_.push_back(diag);

        // Render and output
        std::string rendered = renderer_.render(diag);

        // Output to appropriate stream
        if (diag.severity() >= Severity::Warning)
        {
            std::cerr << rendered;
        }
        else
        {
            std::cout << rendered;
        }

        // Call custom handler if registered
        if (custom_handler_)
        {
            custom_handler_(diag, rendered);
        }
    }

    // Builder methods for common diagnostic types
    Diagnostic error(Category cat, std::string msg, SourceLocation loc)
    {
        return Diagnostic(Severity::Error, cat, std::move(msg), std::move(loc));
    }

    Diagnostic warning(Category cat, std::string msg, SourceLocation loc)
    {
        return Diagnostic(Severity::Warning, cat, std::move(msg), std::move(loc));
    }

    Diagnostic note(Category cat, std::string msg, SourceLocation loc)
    {
        return Diagnostic(Severity::Note, cat, std::move(msg), std::move(loc));
    }

    // Configuration
    void set_options(Options opts) { options_ = std::move(opts); }
    const Options& get_options() const { return options_; }

    void set_custom_handler(std::function<void(const Diagnostic&, const std::string&)> handler)
    {
        custom_handler_ = std::move(handler);
    }

    // Statistics
    const Statistics& statistics() const { return stats_; }
    void reset_statistics()
    {
        stats_ = Statistics{};
        error_limit_reached_ = false;
    }

    // Access diagnostics
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    void clear_diagnostics() { diagnostics_.clear(); }

    // Batch operations
    void emit_all(std::vector<Diagnostic> diags)
    {
        for (auto& d : diags)
            emit(std::move(d));
    }

   private:
    bool should_suppress(const Diagnostic& diag) const
    {
        if (options_.suppress_notes && diag.severity() == Severity::Note)
            return true;
        if (options_.suppress_warnings && diag.severity() == Severity::Warning)
            return true;

        // Check warning filters
        if (diag.severity() == Severity::Warning && diag.error_code())
        {
            const auto& code = *diag.error_code();

            // Check if explicitly disabled
            if (std::find(options_.disabled_warnings.begin(), options_.disabled_warnings.end(), code)
              != options_.disabled_warnings.end())
            {
                return true;
            }

            // If enabled list exists, only show warnings in it
            if (!options_.enabled_warnings.empty())
            {
                return std::find(options_.enabled_warnings.begin(), options_.enabled_warnings.end(), code)
                  == options_.enabled_warnings.end();
            }
        }

        return false;
    }

    void update_statistics(const Diagnostic& diag)
    {
        switch (diag.severity())
        {
        case Severity::Note :
            ++stats_.note_count;
            break;
        case Severity::Hint :
            ++stats_.note_count;
            break;
        case Severity::Warning :
            ++stats_.warning_count;
            break;
        case Severity::Error :
            ++stats_.error_count;
            break;
        case Severity::Fatal :
            ++stats_.fatal_count;
            break;
        case Severity::ICE :
            ++stats_.ice_count;
            break;
        }
    }

    SourceManager& source_manager_;
    DiagnosticRenderer renderer_;
    Options options_;
    Statistics stats_;
    std::vector<Diagnostic> diagnostics_;
    std::function<void(const Diagnostic&, const std::string&)> custom_handler_;
    bool error_limit_reached_ = false;
};

}  // namespace diag