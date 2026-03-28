#ifndef ERROR_OR
#define ERROR_OR

#include "lexer.hpp"

#include <string>

namespace fairuz {

class [[nodiscard]] Fa_Error {
public:
    template<typename CodeEnum>
    explicit Fa_Error(CodeEnum code)
        : m_code(static_cast<u16>(code))
    {
    }

    template<typename CodeEnum>
    Fa_Error()
        : m_code(CodeEnum::UNKNOWN)
    {
    }

    Fa_Error(Fa_Error const&) = default;
    Fa_Error& operator=(Fa_Error const&) = default;
    Fa_Error(Fa_Error&&) = default;
    Fa_Error& operator=(Fa_Error&&) = default;

    bool operator==(Fa_Error const& other) const { return m_code == other.m_code; }

    Fa_StringRef getErrorMessage() const { return diagnostic::errorMessageFor(m_code); }
    u16 getCode() const { return m_code; }

private:
    u16 m_code;
}; // class Fa_Error

template<typename T, typename E = Fa_Error>
class [[nodiscard]] Fa_ErrorOr {
public:
    Fa_ErrorOr(T val)
        : is_value_(true)
    {
        ::new (static_cast<void*>(&storage_)) T(static_cast<T&&>(val));
    }

    Fa_ErrorOr(E err)
        : is_value_(false)
    {
        ::new (static_cast<void*>(&storage_)) E(static_cast<E&&>(err));
    }

    static Fa_ErrorOr fromValue(T v) { return Fa_ErrorOr(static_cast<T&&>(v)); }
    static Fa_ErrorOr fromError(E e) { return Fa_ErrorOr(static_cast<E&&>(e)); }

    Fa_ErrorOr(Fa_ErrorOr const& other)
        : is_value_(other.is_value_)
    {
        if (is_value_)
            ::new (static_cast<void*>(&storage_)) T(other.get_value());
        else
            ::new (static_cast<void*>(&storage_)) E(other.get_error());
    }

    Fa_ErrorOr& operator=(Fa_ErrorOr const& other)
    {
        if (this == &other)
            return *this;
        destroy_active();
        is_value_ = other.is_value_;
        if (is_value_)
            ::new (static_cast<void*>(&storage_)) T(other.get_value());
        else
            ::new (static_cast<void*>(&storage_)) E(other.get_error());
        return *this;
    }

    Fa_ErrorOr(Fa_ErrorOr&& other) noexcept
        : is_value_(other.is_value_)
    {
        if (is_value_)
            ::new (static_cast<void*>(&storage_)) T(static_cast<T&&>(other.get_value()));
        else
            ::new (static_cast<void*>(&storage_)) E(static_cast<E&&>(other.get_error()));
    }

    Fa_ErrorOr& operator=(Fa_ErrorOr&& other) noexcept
    {
        if (this == &other)
            return *this;
        destroy_active();
        is_value_ = other.is_value_;
        if (is_value_)
            ::new (static_cast<void*>(&storage_)) T(static_cast<T&&>(other.get_value()));
        else
            ::new (static_cast<void*>(&storage_)) E(static_cast<E&&>(other.get_error()));
        return *this;
    }

    ~Fa_ErrorOr() { destroy_active(); }

    bool hasValue() const noexcept { return is_value_; }
    bool hasError() const noexcept { return !is_value_; }

    T value() const
    {
        // In a debug build you want an assert here rather than silent UB.
        assert(is_value_ && "called value() on an Fa_ErrorOr holding an error");
        return get_value();
    }

    E error() const
    {
        assert(!is_value_ && "called error() on an Fa_ErrorOr holding a value");
        return get_error();
    }

    void setValue(T const& v)
    {
        destroy_active();
        ::new (static_cast<void*>(&storage_)) T(v);
        is_value_ = true;
    }

    void setError(E const& e)
    {
        destroy_active();
        ::new (static_cast<void*>(&storage_)) E(e);
        is_value_ = false;
    }

private:
    alignas(T) alignas(E) std::byte storage_[sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E)];
    bool is_value_;

    T& get_value() { return *reinterpret_cast<T*>(&storage_); }
    T const& get_value() const { return *reinterpret_cast<T const*>(&storage_); }

    E& get_error() { return *reinterpret_cast<E*>(&storage_); }
    E const& get_error() const { return *reinterpret_cast<E const*>(&storage_); }

    void destroy_active() noexcept
    {
        if (is_value_)
            get_value().~T();
        else
            get_error().~E();
    }
}; // class Fa_ErrorOr

static Fa_Error _reportError(u16 err_code, Fa_SourceLocation loc, lex::Fa_Lexer* lex)
{
    std::string snippet;
    if (lex) {
        Fa_StringRef line = lex->getLineAt(loc.line);
        if (!line.empty())
            snippet.assign(line.data(), line.len());
    }
    diagnostic::report(diagnostic::Severity::ERROR, loc.line, loc.column, err_code, snippet);
    return Fa_Error(err_code);
}
static Fa_Error reportError(diagnostic::errc::parser::Code err_code, Fa_SourceLocation loc, lex::Fa_Lexer* lex = nullptr)
{
    return _reportError(static_cast<u16>(err_code), loc, lex);
}
static Fa_Error reportError(diagnostic::errc::sema::Code err_code, Fa_SourceLocation loc, lex::Fa_Lexer* lex = nullptr)
{
    return _reportError(static_cast<u16>(err_code), loc, lex);
}
static Fa_Error reportError(diagnostic::errc::runtime::Code err_code, Fa_SourceLocation loc, lex::Fa_Lexer* lex = nullptr)
{
    return _reportError(static_cast<u16>(err_code), loc, lex);
}
static Fa_Error reportError(diagnostic::errc::general::Code err_code, Fa_SourceLocation loc, lex::Fa_Lexer* lex = nullptr)
{
    return _reportError(static_cast<u16>(err_code), loc, lex);
}

} // namespace fairuz

#endif // ERROR_OR
