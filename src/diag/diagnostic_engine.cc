#include "../../include/diag/diagnostic_engine.hpp"
#include "../../include/lex/source_manager.hpp"


namespace diagnostics {

// Fluent API for building diagnostics
Diagnostic& Diagnostic::with_code(std::string code)
{
    error_code_ = std::move(code);
    return *this;
}

Diagnostic& Diagnostic::with_range(SourceRange range)
{
    primary_range_ = std::move(range);
    return *this;
}

Diagnostic& Diagnostic::add_highlight(SourceRange range, HighlightStyle style, std::string label = "")
{
    highlights_.emplace_back(std::move(range), style, std::move(label));
    return *this;
}

Diagnostic& Diagnostic::add_note(std::string note, std::optional<SourceLocation> loc = std::nullopt)
{
    notes_.emplace_back(std::move(note), std::move(loc));
    return *this;
}

Diagnostic& Diagnostic::add_fixit(FixItHint hint)
{
    fixits_.push_back(std::move(hint));
    return *this;
}

Diagnostic& Diagnostic::suggest_fixit(std::string description, SourceRange range, std::string replacement)
{
    return add_fixit(FixItHint::replace(range, std::move(replacement), std::move(description)));
}

Diagnostic& Diagnostic::add_related_location(SourceLocation loc, std::string msg)
{
    related_locations_.emplace_back(std::move(loc), std::move(msg));
    return *this;
}

Diagnostic& Diagnostic::with_help_text(std::string help)
{
    help_text_ = std::move(help);
    return *this;
}

Diagnostic& Diagnostic::with_url(std::string url)
{
    documentation_url_ = std::move(url);
    return *this;
}

// Getters
Severity Diagnostic::severity() const { return severity_; }

Category Diagnostic::category() const { return category_; }

const std::string& Diagnostic::message() const { return message_; }

const std::optional<std::string>& Diagnostic::error_code() const { return error_code_; }

const SourceLocation& Diagnostic::location() const { return primary_location_; }

const std::optional<SourceRange>& Diagnostic::range() const { return primary_range_; }

const std::vector<HighlightRange>& Diagnostic::highlights() const { return highlights_; }

const std::vector<std::pair<std::string, std::optional<SourceLocation>>>& Diagnostic::notes() const { return notes_; }

const std::vector<FixItHint>& Diagnostic::fixits() const { return fixits_; }

const std::vector<std::pair<SourceLocation, std::string>>& Diagnostic::related_locations() const
{
    return related_locations_;
}

const std::optional<std::string>& Diagnostic::help_text() const { return help_text_; }

const std::optional<std::string>& Diagnostic::documentation_url() const { return documentation_url_; }

std::string DiagnosticRenderer::render(const Diagnostic& diag) const
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

std::string DiagnosticRenderer::colorize(const std::string& text, Color color) const
{
    if (options_.color_mode != ColorMode::ANSI)
    {
        return text;
    }

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

std::string DiagnosticRenderer::severity_string(Severity sev) const
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

void DiagnosticRenderer::render_header(std::ostringstream& out, const Diagnostic& diag) const
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

void DiagnosticRenderer::render_source_snippet(std::ostringstream& out, const Diagnostic& diag) const
{
    const auto& loc = diag.location();
    std::string line = source_manager_.get_line(loc);

    if (line.empty())
    {
        return;
    }

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

void DiagnosticRenderer::render_fixits(std::ostringstream& out, const Diagnostic& diag) const
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

void DiagnosticRenderer::render_notes(std::ostringstream& out, const Diagnostic& diag) const
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

void DiagnosticRenderer::render_related_location(
  std::ostringstream& out, const SourceLocation& loc, const std::string& msg) const
{
    out << colorize(loc.filename, Color::Bold) << ":" << loc.line << ":" << loc.column << ": "
        << colorize("note: ", Color::Cyan) << msg << "\n";
}

void DiagnosticRenderer::render_help(std::ostringstream& out, const std::string& help) const
{
    out << colorize("  help: ", Color::Green) << help << "\n";
}

// Emit a diagnostic
void DiagnosticsEngine::emit(Diagnostic diag)
{
    // Apply filtering
    if (should_suppress(diag))
    {
        return;
    }

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
Diagnostic DiagnosticsEngine::error(Category cat, std::string msg, SourceLocation loc)
{
    return Diagnostic(Severity::Error, cat, std::move(msg), std::move(loc));
}

Diagnostic DiagnosticsEngine::warning(Category cat, std::string msg, SourceLocation loc)
{
    return Diagnostic(Severity::Warning, cat, std::move(msg), std::move(loc));
}

Diagnostic DiagnosticsEngine::note(Category cat, std::string msg, SourceLocation loc)
{
    return Diagnostic(Severity::Note, cat, std::move(msg), std::move(loc));
}

// Configuration
void DiagnosticsEngine::set_options(Options opts) { options_ = std::move(opts); }

const typename DiagnosticsEngine::Options& DiagnosticsEngine::get_options() const { return options_; }

void DiagnosticsEngine::set_custom_handler(std::function<void(const Diagnostic&, const std::string&)> handler)
{
    custom_handler_ = std::move(handler);
}

// Statistics
const typename DiagnosticsEngine::Statistics& DiagnosticsEngine::statistics() const { return stats_; }

void DiagnosticsEngine::reset_statistics()
{
    stats_ = Statistics{};
    error_limit_reached_ = false;
}

// Access diagnostics
const std::vector<Diagnostic>& DiagnosticsEngine::diagnostics() const { return diagnostics_; }

void DiagnosticsEngine::clear_diagnostics() { diagnostics_.clear(); }

// Batch operations
void DiagnosticsEngine::emit_all(std::vector<Diagnostic> diags)
{
    for (auto& d : diags)
    {
        emit(std::move(d));
    }
}

bool DiagnosticsEngine::should_suppress(const Diagnostic& diag) const
{
    if (options_.suppress_notes && diag.severity() == Severity::Note)
    {
        return true;
    }

    if (options_.suppress_warnings && diag.severity() == Severity::Warning)
    {
        return true;
    }

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

void DiagnosticsEngine::update_statistics(const Diagnostic& diag)
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

}