#ifndef FA_ERROR_HPP
#define FA_ERROR_HPP

#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"

namespace fairuz {

class [[nodiscard]] Fa_Error {
public:
    template<typename CodeEnum>
    explicit Fa_Error(CodeEnum code)
        : m_code(static_cast<u16>(code))
    {
    }

    struct RawCode {
        u16 value;
    };
    explicit Fa_Error(RawCode raw)
        : m_code(raw.value)
    {
    }

    Fa_Error() = default;

    Fa_Error(Fa_Error const&) = default;
    Fa_Error& operator=(Fa_Error const&) = default;

    Fa_Error(Fa_Error&&) = default;
    Fa_Error& operator=(Fa_Error&&) = default;

    bool operator==(Fa_Error const& other) const { return m_code == other.m_code; }

    Fa_StringRef get_error_message() const { return diagnostic::error_message_for(m_code); }
    u16 get_code() const { return m_code; }

    diagnostic::Fa_DiagnosticEngine::DiagnosticId diag_id() const { return m_diag_id; }
    void set_diag_id(diagnostic::Fa_DiagnosticEngine::DiagnosticId id) { m_diag_id = id; }

private:
    u16 m_code { 0xFFFF };
    diagnostic::Fa_DiagnosticEngine::DiagnosticId m_diag_id { diagnostic::Fa_DiagnosticEngine::INVALID_ID };
}; // class Fa_Error

template<typename T, typename E = Fa_Error>
class [[nodiscard]] Fa_ErrorOr {
public:
    Fa_ErrorOr() = default;

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

    template<typename _Tp>
    Fa_ErrorOr<_Tp> error_or(_Tp v)
    {
        if (has_error())
            return get_error();
        return v;
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

static Fa_Error _report_error(u16 errc, Fa_SourceLocation loc, diagnostic::Severity sv = diagnostic::Severity::ERROR)
{
    auto id = diagnostic::report(sv, loc, errc);
    Fa_Error err { Fa_Error::RawCode { errc } };
    err.set_diag_id(id);
    return err;
}

static inline Fa_Error report_error(diagnostic::errc::compiler::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static inline Fa_Error report_error(diagnostic::errc::parser::Code errc, Fa_SourceLocation loc, diagnostic::Severity sv = diagnostic::Severity::ERROR)
{
    return _report_error(static_cast<u16>(errc), loc, sv);
}
static inline Fa_Error report_error(diagnostic::errc::sema::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static inline Fa_Error report_error(diagnostic::errc::runtime::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static inline Fa_Error report_error(diagnostic::errc::general::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}
static inline Fa_Error report_error(diagnostic::errc::stdlib::Code errc, Fa_SourceLocation loc)
{
    return _report_error(static_cast<u16>(errc), loc);
}

} // namespace fairuz

#endif // FA_ERROR_HPP
