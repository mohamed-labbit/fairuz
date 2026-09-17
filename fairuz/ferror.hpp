#ifndef FA_ERROR_HPP
#define FA_ERROR_HPP

#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fstring.hpp"

namespace fairuz {

class [[nodiscard]] Error {
public:
    explicit Error(ErrorCode code)
        : m_code(code)
    {
    }

    Error() = default;

    Error(Error const&) = default;
    Error& operator=(Error const&) = default;

    Error(Error&&) = default;
    Error& operator=(Error&&) = default;

    bool operator==(Error const& other) const { return m_code == other.m_code; }

    StringRef get_error_message() const { return diagnostic::error_message_for(m_code); }
    ErrorCode get_code() const { return m_code; }

    diagnostic::DiagnosticEngine::DiagnosticId diag_id() const { return m_diag_id; }
    void set_diag_id(diagnostic::DiagnosticEngine::DiagnosticId id) { m_diag_id = id; }

private:
    ErrorCode m_code { 0xFFFF };
    diagnostic::DiagnosticEngine::DiagnosticId m_diag_id { diagnostic::DiagnosticEngine::INVALID_ID };
}; // class Error

template<typename T, typename E = Error>
class [[nodiscard]] ErrorOr {
public:
    ErrorOr() = default;

    ErrorOr(T val)
        : m_is_value(true)
    {
        ::new (static_cast<void*>(&m_storage)) T(static_cast<T&&>(val));
    }

    ErrorOr(E err)
        : m_is_value(false)
    {
        ::new (static_cast<void*>(&m_storage)) E(static_cast<E&&>(err));
    }

    static ErrorOr from_value(T v) { return ErrorOr(static_cast<T&&>(v)); }
    static ErrorOr from_error(E e) { return ErrorOr(static_cast<E&&>(e)); }

    ErrorOr(ErrorOr const& other)
        : m_is_value(other.m_is_value)
    {
        if (m_is_value)
            ::new (static_cast<void*>(&m_storage)) T(other.get_value());
        else
            ::new (static_cast<void*>(&m_storage)) E(other.get_error());
    }

    ErrorOr& operator=(ErrorOr const& other)
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

    ErrorOr(ErrorOr&& other) noexcept
        : m_is_value(other.m_is_value)
    {
        if (m_is_value)
            ::new (static_cast<void*>(&m_storage)) T(static_cast<T&&>(other.get_value()));
        else
            ::new (static_cast<void*>(&m_storage)) E(static_cast<E&&>(other.get_error()));
    }

    ErrorOr& operator=(ErrorOr&& other) noexcept
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

    ~ErrorOr() { destroy_active(); }

    bool has_value() const noexcept { return m_is_value; }
    bool has_error() const noexcept { return !m_is_value; }

    T value() const
    {
        assert(m_is_value && "called value() on an ErrorOr holding an error");
        return get_value();
    }

    E error() const
    {
        assert(!m_is_value && "called error() on an ErrorOr holding a value");
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
    ErrorOr<_Tp> error_or(_Tp v)
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
}; // class ErrorOr

inline Error report_error(ErrorCode errc, SourceLocation loc, diagnostic::Severity sv = diagnostic::Severity::ERROR)
{
    auto id = diagnostic::report(sv, loc, errc);
    Error err { Error { errc } };
    err.set_diag_id(id);
    return err;
}

} // namespace fairuz

#endif // FA_ERROR_HPP
