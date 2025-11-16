// diagnostics_engine.h
#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


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
        {
            return filename < other.filename;
        }
        if (line != other.line)
        {
            return line < other.line;
        }
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
    Diagnostic& with_code(std::string code);

    Diagnostic& with_range(SourceRange range);

    Diagnostic& add_highlight(SourceRange range, HighlightStyle style, std::string label = "");

    Diagnostic& add_note(std::string note, std::optional<SourceLocation> loc = std::nullopt);

    Diagnostic& add_fixit(FixItHint hint);

    Diagnostic& suggest_fixit(std::string description, SourceRange range, std::string replacement);

    Diagnostic& add_related_location(SourceLocation loc, std::string msg);

    Diagnostic& with_help_text(std::string help);

    Diagnostic& with_url(std::string url);

    // Getters
    Severity severity() const;

    Category category() const;

    const std::string& message() const;

    const std::optional<std::string>& error_code() const;

    const SourceLocation& location() const;

    const std::optional<SourceRange>& range() const;

    const std::vector<HighlightRange>& highlights() const;

    const std::vector<std::pair<std::string, std::optional<SourceLocation>>>& notes() const;

    const std::vector<FixItHint>& fixits() const;

    const std::vector<std::pair<SourceLocation, std::string>>& related_locations() const;

    const std::optional<std::string>& help_text() const;

    const std::optional<std::string>& documentation_url() const;

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
            {
                return "";
            }

            size_t start = line_offsets[line_num - 1];
            size_t end = (line_num < line_offsets.size()) ? line_offsets[line_num] - 1 : content.size();

            // Remove trailing newline if present
            if (end > start && content[end - 1] == '\n')
            {
                --end;
            }
            if (end > start && content[end - 1] == '\r')
            {
                --end;
            }

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

    std::string render(const Diagnostic& diag) const;

   private:
    enum class Color { Reset, Bold, Red, Green, Yellow, Blue, Magenta, Cyan, White };

    std::string colorize(const std::string& text, Color color) const;

    std::string severity_string(Severity sev) const;

    void render_header(std::ostringstream& out, const Diagnostic& diag) const;

    void render_source_snippet(std::ostringstream& out, const Diagnostic& diag) const;

    void render_fixits(std::ostringstream& out, const Diagnostic& diag) const;

    void render_notes(std::ostringstream& out, const Diagnostic& diag) const;

    void render_related_location(std::ostringstream& out, const SourceLocation& loc, const std::string& msg) const;

    void render_help(std::ostringstream& out, const std::string& help) const;

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
    void emit(Diagnostic diag);

    // Builder methods for common diagnostic types
    Diagnostic error(Category cat, std::string msg, SourceLocation loc);

    Diagnostic warning(Category cat, std::string msg, SourceLocation loc);

    Diagnostic note(Category cat, std::string msg, SourceLocation loc);

    // Configuration
    void set_options(Options opts);

    const Options& get_options() const;

    void set_custom_handler(std::function<void(const Diagnostic&, const std::string&)> handler);

    // Statistics
    const Statistics& statistics() const;

    void reset_statistics();

    // Access diagnostics
    const std::vector<Diagnostic>& diagnostics() const;

    void clear_diagnostics();

    // Batch operations
    void emit_all(std::vector<Diagnostic> diags);

   private:
    bool should_suppress(const Diagnostic& diag) const;

    void update_statistics(const Diagnostic& diag);

    SourceManager& source_manager_;
    DiagnosticRenderer renderer_;
    Options options_;
    Statistics stats_;
    std::vector<Diagnostic> diagnostics_;
    std::function<void(const Diagnostic&, const std::string&)> custom_handler_;
    bool error_limit_reached_ = false;
};

}  // namespace diag