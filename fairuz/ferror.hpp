#ifndef ERROR_OR
#define ERROR_OR

#include "flexer.hpp"
#include "fmacros.hpp"

#include <string>

// ============================================================
// CHANGES FROM THE ORIGINAL:
//
// [FIX 3] Fa_Error's broken templated default constructor
//             template<typename CodeEnum> Fa_Error() : m_code(CodeEnum::UNKNOWN) {}
//         is removed — CodeEnum could never be deduced from a
//         zero-argument call, making this dead/uncompilable-if-touched
//         code. Replaced with a plain default that stores a sentinel
//         u16 value (0xFFFF, matching no real error code in any phase's
//         0x0001-0x07FF range) rather than trying to default-construct
//         into a specific phase's UNKNOWN value that the type has no way
//         to know about.
//
// [FIX 1 follow-through] report_error()/_report_error() now return the
// same Fa_Error as before, but also thread the new DiagnosticId through
// so a caller that wants to attach a suggestion/note to THIS specific
// diagnostic can do so immediately after reporting it, without relying
// on the fragile "attach to whatever was reported last" behavior.
// Fa_Error itself gains an optional stored DiagnosticId for this purpose
// — existing call sites that ignore it are unaffected.
// ============================================================

namespace fairuz {

class [[nodiscard]] Fa_Error {
public:
    template<typename CodeEnum>
    explicit Fa_Error(CodeEnum code)
        : m_code(static_cast<u16>(code))
    {
    }

    // [FIX 3 addendum] Non-templated raw-code constructor. This is what
    // _report_error() now uses instead of casting an arbitrary u16 through
    // an unrelated phase's Code enum (which the original single-argument
    // template technically allowed you to do by force-casting, but which
    // is semantically wrong — a lexer error code has no business being
    // constructed "as if" it were a general::Code). Marked explicit and
    // named distinctly via the tag type below so it can never be called
    // by accident where a real enum overload was intended.
    struct RawCode { u16 value; };
    explicit Fa_Error(RawCode raw)
        : m_code(raw.value)
    {
    }

    // Plain, non-templated default — no phantom CodeEnum deduction.
    // 0xFFFF is reserved as "no specific code" and deliberately falls
    // outside every phase's 0x0001-0x07FF range, so phase_of(0xFFFF)
    // correctly resolves to Phase::UNKNOWN rather than aliasing a real
    // phase's block by accident.
    Fa_Error() = default;

    Fa_Error(Fa_Error const&) = default;
    Fa_Error& operator=(Fa_Error const&) = default;

    Fa_Error(Fa_Error&&) = default;
    Fa_Error& operator=(Fa_Error&&) = default;

    bool operator==(Fa_Error const& other) const { return m_code == other.m_code; }

    Fa_StringRef get_error_message() const { return diagnostic::error_message_for(m_code); }
    u16 get_code() const { return m_code; }

    // [FIX 1 follow-through] New — lets a caller that just received this
    // Fa_Error from report_error() immediately attach a suggestion/note
    // to the exact diagnostic that was reported, e.g.:
    //     Fa_Error err = report_error(ParserCode::EXPECTED_COLON_IF, loc, &lexer);
    //     diagnostic::engine.add_suggestion(err.diag_id(), "did you forget a ':' here?");
    diagnostic::Fa_DiagnosticEngine::DiagnosticId diag_id() const { return m_diag_id; }
    void set_diag_id(diagnostic::Fa_DiagnosticEngine::DiagnosticId id) { m_diag_id = id; }

private:
    u16 m_code { 0xFFFF };
    diagnostic::Fa_DiagnosticEngine::DiagnosticId m_diag_id { diagnostic::Fa_DiagnosticEngine::INVALID_ID };
}; // class Fa_Error

template<typename T, typename E = Fa_Error>
class [[nodiscard]] Fa_ErrorOr {
public:
    Fa_ErrorOr(T val)
        : m_is_value(true)
    {
        ::new (static_cast<void*>(&m_storage)) T(static_cast<T&&>(val));
    }

    Fa_ErrorOr(E err)
        : m_is_value(false)
    {
        ::new (static_cast<void*>(&m_storage)) E(static_cast<E&&>(err));
    }

    static Fa_ErrorOr from_value(T v) { return Fa_ErrorOr(static_cast<T&&>(v)); }
    static Fa_ErrorOr from_error(E e) { return Fa_ErrorOr(static_cast<E&&>(e)); }

    Fa_ErrorOr(Fa_ErrorOr const& other)
        : m_is_value(other.m_is_value)
    {
        if (m_is_value)
            ::new (static_cast<void*>(&m_storage)) T(other.get_value());
        else
            ::new (static_cast<void*>(&m_storage)) E(other.get_error());
    }

    Fa_ErrorOr& operator=(Fa_ErrorOr const& other)
    {
        if (this == &other)
            return *this;
        destroy_active();
        m_is_value = other.m_is_value;
        if (m_is_value)
            ::new (static_cast<void*>(&m_storage)) T(other.get_value());
        else
            ::new (static_cast<void*>(&m_storage)) E(other.get_error());
        return *this;
    }

    Fa_ErrorOr(Fa_ErrorOr&& other) noexcept
        : m_is_value(other.m_is_value)
    {
        if (m_is_value)
            ::new (static_cast<void*>(&m_storage)) T(static_cast<T&&>(other.get_value()));
        else
            ::new (static_cast<void*>(&m_storage)) E(static_cast<E&&>(other.get_error()));
    }

    Fa_ErrorOr& operator=(Fa_ErrorOr&& other) noexcept
    {
        if (this == &other)
            return *this;
        destroy_active();
        m_is_value = other.m_is_value;
        if (m_is_value)
            ::new (static_cast<void*>(&m_storage)) T(static_cast<T&&>(other.get_value()));
        else
            ::new (static_cast<void*>(&m_storage)) E(static_cast<E&&>(other.get_error()));
        return *this;
    }

    ~Fa_ErrorOr() { destroy_active(); }

    bool has_value() const noexcept { return m_is_value; }
    bool has_error() const noexcept { return !m_is_value; }

    T value() const
    {
        assert(m_is_value && "called value() on an Fa_ErrorOr holding an error");
        return get_value();
    }

    E error() const
    {
        assert(!m_is_value && "called error() on an Fa_ErrorOr holding a value");
        return get_error();
    }

    void set_value(T const& v)
    {
        destroy_active();
        ::new (static_cast<void*>(&m_storage)) T(v);
        m_is_value = true;
    }

    void set_error(E const& e)
    {
        destroy_active();
        ::new (static_cast<void*>(&m_storage)) E(e);
        m_is_value = false;
    }

private:
    alignas(T) alignas(E) std::byte m_storage[sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E)];
    bool m_is_value;

    T& get_value() { return *reinterpret_cast<T*>(&m_storage); }
    T const& get_value() const { return *reinterpret_cast<T const*>(&m_storage); }

    E& get_error() { return *reinterpret_cast<E*>(&m_storage); }
    E const& get_error() const { return *reinterpret_cast<E const*>(&m_storage); }

    void destroy_active() noexcept
    {
        if (m_is_value)
            get_value().~T();
        else
            get_error().~E();
    }
}; // class Fa_ErrorOr

// [FIX 1 follow-through] report() now returns a DiagnosticId; _report_error
// captures it and stores it on the Fa_Error it returns, so callers get the
// stable handle "for free" without changing every call site's signature.
static Fa_Error _report_error(u16 errc, Fa_SourceLocation loc)
{
    auto id = diagnostic::report(diagnostic::Severity::ERROR, loc, errc);

    // Uses the RawCode constructor — errc here is already a u16
    // recovered from whichever phase-specific enum the caller passed in
    // (parser::Code, sema::Code, etc. — see the typed overloads below),
    // so there's no need to force-cast it through an unrelated enum just
    // to satisfy the templated constructor.
    Fa_Error err { Fa_Error::RawCode { errc } };
    err.set_diag_id(id);
    return err;
}

static Fa_Error report_error(diagnostic::errc::compiler::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static Fa_Error report_error(diagnostic::errc::parser::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static Fa_Error report_error(diagnostic::errc::sema::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static Fa_Error report_error(diagnostic::errc::runtime::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static Fa_Error report_error(diagnostic::errc::general::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static Fa_Error report_error(diagnostic::errc::stdlib::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}

} // namespace fairuz

#endif // ERROR_OR