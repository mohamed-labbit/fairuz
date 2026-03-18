#ifndef ERROR_OR
#define ERROR_OR

#include "diagnostic.hpp"
#include "lexer.hpp"
#include "runtime/opcode.hpp"
#include "string.hpp"
#include <cstdint>
#include <new>
#include <type_traits>

namespace mylang {

class [[nodiscard]] Error {
public:
    // StringRef overload — for descriptive errors from the semantic
    // analyzer and other non-parser paths. The caller is responsible
    // for ensuring the string outlives the Error.
    template<typename CodeEnum>
    explicit Error(CodeEnum code)
        : m_code(static_cast<uint16_t>(code))
    {
    }

    template<typename CodeEnum>
    Error()
        : m_code(CodeEnum::UNKNOWN)
    {
    }

    Error(Error const&) = default;
    Error& operator=(Error const&) = default;
    Error(Error&&) = default;
    Error& operator=(Error&&) = default;

    bool operator==(Error const& other) const
    {
        return m_code == other.m_code;
    }

    StringRef getErrorMessage() const { return diagnostic::errorMessageFor(m_code); }
    uint16_t getCode() const { return m_code; }

private:
    uint16_t m_code;
};

template<typename T, typename E = Error>
class [[nodiscard]] ErrorOr {
public:
    // Implicit construction from a value or error so callers can write
    //   return someValue;
    //   return Error("msg");
    // without explicit wrapping.
    ErrorOr(T val)
        : is_value_(true)
    {
        ::new (static_cast<void*>(&storage_)) T(static_cast<T&&>(val));
    }

    ErrorOr(E err)
        : is_value_(false)
    {
        ::new (static_cast<void*>(&storage_)) E(static_cast<E&&>(err));
    }

    static ErrorOr fromValue(T v) { return ErrorOr(static_cast<T&&>(v)); }
    static ErrorOr fromError(E e) { return ErrorOr(static_cast<E&&>(e)); }

    ErrorOr(ErrorOr const& other)
        : is_value_(other.is_value_)
    {
        if (is_value_)
            ::new (static_cast<void*>(&storage_)) T(other.get_value());
        else
            ::new (static_cast<void*>(&storage_)) E(other.get_error());
    }

    ErrorOr& operator=(ErrorOr const& other)
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

    ErrorOr(ErrorOr&& other) noexcept
        : is_value_(other.is_value_)
    {
        if (is_value_)
            ::new (static_cast<void*>(&storage_)) T(static_cast<T&&>(other.get_value()));
        else
            ::new (static_cast<void*>(&storage_)) E(static_cast<E&&>(other.get_error()));
    }

    ErrorOr& operator=(ErrorOr&& other) noexcept
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

    ~ErrorOr() { destroy_active(); }

    bool hasValue() const noexcept { return is_value_; }
    bool hasError() const noexcept { return !is_value_; }

    T value() const
    {
        // In a debug build you want an assert here rather than silent UB.
        assert(is_value_ && "called value() on an ErrorOr holding an error");
        return get_value();
    }

    E error() const
    {
        assert(!is_value_ && "called error() on an ErrorOr holding a value");
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
    // aligned_storage gives us raw bytes with the right size and alignment
    // for whichever of T or E is larger, without constructing either.
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
};

static Error _reportError(uint16_t err_code, SourceLocation loc, lex::Lexer* lex)
{
    diagnostic::report(diagnostic::Severity::ERROR,
        loc.line, loc.column, err_code, lex ? lex->getLineAt(loc.line).data() : "");
    return Error(err_code);
}

static Error reportError(diagnostic::errc::parser::Code err_code, SourceLocation loc, lex::Lexer* lex = nullptr)
{
    return _reportError(static_cast<uint16_t>(err_code), loc, lex);
}

static Error reportError(diagnostic::errc::sema::Code err_code, SourceLocation loc, lex::Lexer* lex = nullptr)
{
    return _reportError(static_cast<uint16_t>(err_code), loc, lex);
}

static Error reportError(diagnostic::errc::runtime::Code err_code, SourceLocation loc, lex::Lexer* lex = nullptr)
{
    return _reportError(static_cast<uint16_t>(err_code), loc, lex);
}

static Error reportError(diagnostic::errc::general::Code err_code, SourceLocation loc, lex::Lexer* lex = nullptr)
{
    return _reportError(static_cast<uint16_t>(err_code), loc, lex);
}

} // namespace mylang

#endif // ERROR_OR
