#ifndef FA_DIAGNOSTIC_HPP
#define FA_DIAGNOSTIC_HPP

#include "ferrc.hpp"
#include "fmacros.hpp"
#include "fsource_manager.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace fairuz {
namespace diagnostic {

enum class Severity : u8 {
    NOTE,
    FATAL,
    ERROR,
    WARNING
}; // enum Severity

// Compatibility for existing qualified diagnostic error-code users.
using ErrorCode = fairuz::ErrorCode;

struct DiagnosticAbort final : public std::runtime_error {
    DiagnosticAbort()
        : std::runtime_error("fatal diagnostic")
    {
    }
};

using Source = SourceManager;
using SourcePtr = fairuz::SourcePtr;

char const* error_type_for(ErrorCode code) noexcept;

class DiagnosticEngine {
public:
    // Stable handle to a single accumulated diagnostic, returned by
    // report() so a caller can immediately attach a suggestion/note to
    // THIS diagnostic (via the id-taking overloads below) rather than
    // relying on "whatever was reported last," which breaks the moment
    // two diagnostics are reported before either gets annotated (e.g. a
    // caller that reports an error and, while building its suggestion
    // string, triggers another report() call first).
    //
    // Implemented as the diagnostic's index into m_diagnostics at the
    // time it was reported. This is only safe because diagnostics are
    // never removed individually — only appended (report()) or bulk-
    // cleared (reset()) — so an id, once handed out, stays valid for the
    // engine's lifetime or until the next reset(). If individual removal
    // is ever added, this needs to become a generation-checked handle
    // instead of a bare index.
    using DiagnosticId = u32;
    static constexpr DiagnosticId INVALID_ID = static_cast<DiagnosticId>(-1);

    struct Diagnostic {
        Severity severity { Severity::ERROR };
        SourceLocation src_loc;
        ErrorCode err_code { 0 };
        std::string code { "" };
        std::vector<std::string> suggestions;
        std::vector<std::pair<i32, std::string>> notes;
        SourcePtr source;
        struct Frame {
            SourcePtr source;
            SourceLocation location;
            std::string function;
        };
        std::vector<Frame> traceback;
    }; // struct Diagnostic

    // Maximum number of errors before the engine stops accumulating and
    // the parser should stop trying to recover. FATAL diagnostics bypass
    // this limit and always get recorded.
    static constexpr u32 LIMIT = 20;

    DiagnosticEngine() = default;

    // --- existing API, unchanged ---

    void emit(std::string const& msg, Severity const sv = Severity::ERROR) { emit_error(msg, sv); }

    [[noreturn]] void panic(std::string const& msg)
    {
        _panic(msg);
    }

    // Accumulates a diagnostic. Does not emit or abort, for any severity.
    // If the engine is saturated (error count >= LIMIT), non-FATAL diagnostics
    // are silently dropped — the existing diagnostic list is still valid and
    // will be emitted in full when dump() is called. Returns the id of the
    // diagnostic just recorded, or INVALID_ID if it was dropped (saturated
    // and non-FATAL) — callers that don't need the id can ignore the
    // return value as before; this is a source-compatible change from the
    // previous void-returning signature.
    // DiagnosticId report(Severity const sev, SourceLocation const loc, u16 err_code, std::string const& code = "");
    DiagnosticId report_deferred(Severity const sev, SourceLocation const loc, ErrorCode err_code, std::string const& code = "");

    // Existing behavior, unchanged: attaches to whichever diagnostic was
    // reported most recently. Convenient for the common case (report,
    // then immediately annotate, with nothing else reporting in between)
    // but fragile if that assumption doesn't hold — prefer the
    // DiagnosticId-taking overloads below when a report() call and its
    // annotation aren't textually adjacent.
    void add_suggestion(std::string const& suggestion);
    void add_note(i32 line, std::string const& note);

    // Id-targeted overloads: attach to a SPECIFIC diagnostic regardless
    // of what's been reported since. No-op (not an error) if id is
    // INVALID_ID or out of range — annotating a dropped/unknown
    // diagnostic is a silent miss, not a crash, matching how a caller
    // holding an INVALID_ID from a saturated report() should be able to
    // keep calling these unconditionally without checking first.
    void add_suggestion(DiagnosticId id, std::string const& suggestion);
    void add_note(DiagnosticId id, i32 line, std::string const& note);
    void add_frame(DiagnosticId id, SourcePtr source, SourceLocation loc, std::string function);

    std::string to_json() const;

    // Formats and writes all accumulated diagnostics to stderr. Safe to call
    // multiple times — subsequent calls are no-ops if nothing new was added.
    void pretty_print() const;

    // --- new query API ---

    // True if any ERROR or FATAL diagnostic has been recorded.
    bool has_errors() const noexcept { return m_error_count > 0; }

    // True once ErrorCount_ hits LIMIT. The parser should stop recovery
    // attempts and return early when this is true — continuing will only
    // produce noise.
    bool is_saturated() const noexcept { return m_error_count >= LIMIT; }

    u32 error_count() const noexcept { return m_error_count; }
    u32 get_warning_count() const noexcept { return m_warning_count; }

    // Clears all accumulated diagnostics. Intended for unit tests that run
    // multiple parse passes through the same engine instance.
    void reset() noexcept
    {
        m_diagnostics.clear();
        m_error_count = 0;
        m_warning_count = 0;
        m_source = nullptr;
        m_printed = 0;
        m_printed_limit = false;
    }

    void set_source(FileManager const* fm);
    void set_source(SourcePtr source) { m_source = std::move(source); }
    SourcePtr source() const { return m_source; }
    void set_json_output(bool enabled) { m_json_output = enabled; }

private:
    std::vector<Diagnostic> m_diagnostics;
    u32 m_error_count { 0 };
    u32 m_warning_count { 0 };
    SourcePtr m_source;
    mutable size_t m_printed { 0 };
    mutable bool m_printed_limit { false };
    bool m_json_output { false };

    void emit_error(std::string const& msg, Severity const sv);
    [[noreturn]] void _panic(std::string const& msg) const;
    static std::string sv_to_str(Severity const sv);
    std::vector<std::string> split_lines(std::string const& text) const;
    void print_snippet(SourcePtr const& source, SourceLocation const& loc) const;
}; // class DiagnosticEngine

// --- module-level singletons and forwarding functions, unchanged ---

inline DiagnosticEngine engine;

static inline void emit(ErrorCode code, Severity const sv = Severity::ERROR)
{
    engine.report_deferred(sv, { }, code);
    engine.emit(error_message_for(code), sv);
}

// Aborting is explicit. Reporting or emitting FATAL severity alone does not throw.
[[noreturn]] static inline void fatal_error(ErrorCode code, std::string const& detail = "")
{
    engine.report_deferred(Severity::FATAL, { }, code, detail);
    engine.panic("");
}

static inline void emit(ErrorCode code, std::string const& detail, Severity const sv = Severity::ERROR)
{
    engine.report_deferred(sv, { }, code, detail);
    std::string message = error_message_for(code);
    if (!detail.empty())
        message += ": " + detail;
    engine.emit(message, sv);
}

[[noreturn]] static inline void panic(ErrorCode code, std::string const& detail = "")
{
    engine.report_deferred(Severity::ERROR, { }, code, detail);
    engine.panic("");
}

static inline DiagnosticEngine::DiagnosticId report(
    Severity const sev, SourceLocation const loc, ErrorCode err_code, std::string const& code = "")
{
    return engine.report_deferred(sev, loc, err_code, code);
}

// Emits all accumulated diagnostics. The parser calls this once after
// parseProgram() returns, not after each individual error.
static inline void dump() { engine.pretty_print(); }

// Forwarding wrappers for the new query API so callers don't need to
// reach into engine directly.
static inline bool has_errors() noexcept { return engine.has_errors(); }
static inline bool is_saturated() noexcept { return engine.is_saturated(); }
static inline u32 error_count() noexcept { return engine.error_count(); }
static inline u32 warning_count() noexcept { return engine.get_warning_count(); }
static inline void reset() noexcept { engine.reset(); }
static inline void set_source(FileManager const* fm) { engine.set_source(fm); }

class SourceScope {
public:
    explicit SourceScope(SourcePtr source)
        : m_previous(engine.source())
    {
        engine.set_source(std::move(source));
    }
    explicit SourceScope(FileManager const* source)
        : m_previous(engine.source())
    {
        engine.set_source(source);
    }
    ~SourceScope() { engine.set_source(std::move(m_previous)); }
    SourceScope(SourceScope const&) = delete;
    SourceScope& operator=(SourceScope const&) = delete;

private:
    SourcePtr m_previous;
};

} // namespace diagnostic

} // namespace fairuz

#endif // FA_DIAGNOSTIC_HPP
