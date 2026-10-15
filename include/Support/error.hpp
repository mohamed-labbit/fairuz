//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Provides typed error-or-value propagation.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_SUPPORT_ERROR_HPP
#define FAIRUZ_SUPPORT_ERROR_HPP

#include "Diag/diagnostic.hpp"
#include "Macros.hpp"
#include "Support/string.hpp"

#include <cassert>
#include <expected>
#include <type_traits>
#include <utility>

namespace fairuz {

class [[nodiscard]] Error {
public:
    explicit Error(ErrorCode code, SourceLocation loc)
        : m_code(code)
        , m_loc(loc)
    {
    }

    Error() = default;

    Error(Error const&) = default;
    Error& operator=(Error const&) = default;

    Error(Error&&) = default;
    Error& operator=(Error&&) = default;

    bool operator==(Error const& other) const { return m_code == other.m_code; }

    StringRef get_error_message() const { return error_message_for(m_code); }
    ErrorCode get_code() const { return m_code; }
    SourceLocation get_location() const { return m_loc; }

    diagnostic::DiagnosticEngine::DiagnosticId diag_id() const { return m_diag_id; }
    void set_diag_id(diagnostic::DiagnosticEngine::DiagnosticId id) { m_diag_id = id; }

    std::string str() const { return get_error_message().data(); }

private:
    ErrorCode m_code { 0xFFFF };
    SourceLocation m_loc;
    diagnostic::DiagnosticEngine::DiagnosticId m_diag_id { diagnostic::DiagnosticEngine::INVALID_ID };
}; // class Error

// std::expected owns the active alternative and provides safe state transitions.
// Its assignment constraints require a nothrow-movable alternative when needed
// for rollback. Unsupported payload combinations are rejected at compile time.
template<typename T, typename E = Error>
class [[nodiscard]] ErrorOr {
public:
    ErrorOr() = delete;

    ErrorOr(T value)
        : m_storage(std::in_place, std::move(value))
    {
    }
    ErrorOr(E error)
    requires(!std::is_same_v<T, E>)
        : m_storage(std::unexpect, std::move(error))
    {
    }

    static ErrorOr from_value(T value) { return ErrorOr(std::in_place, std::move(value)); }
    static ErrorOr from_error(E error) { return ErrorOr(std::unexpect, std::move(error)); }

    ErrorOr(ErrorOr const&) = default;
    ErrorOr& operator=(ErrorOr const&) = default;
    ErrorOr(ErrorOr&&) = default;
    ErrorOr& operator=(ErrorOr&&) = default;
    ~ErrorOr() = default;

    bool has_value() const noexcept { return m_storage.has_value(); }
    bool has_error() const noexcept { return !has_value(); }

    T& value() &
    {
        assert(has_value());
        return *m_storage;
    }
    T const& value() const&
    {
        assert(has_value());
        return *m_storage;
    }
    T&& value() &&
    {
        assert(has_value());
        return *std::move(m_storage);
    }
    T const&& value() const&&
    {
        assert(has_value());
        return *std::move(m_storage);
    }

    E& error() &
    {
        assert(has_error());
        return m_storage.error();
    }
    E const& error() const&
    {
        assert(has_error());
        return m_storage.error();
    }
    E&& error() &&
    {
        assert(has_error());
        return std::move(m_storage).error();
    }
    E const&& error() const&&
    {
        assert(has_error());
        return std::move(m_storage).error();
    }

    void set_value(T value) { m_storage = std::move(value); }
    void set_error(E error) { m_storage = std::unexpected<E>(std::move(error)); }

    template<typename U>
    ErrorOr<U, E> error_or(U value) const&
    {
        if (has_error())
            return ErrorOr<U, E>::from_error(error());
        return ErrorOr<U, E>::from_value(std::move(value));
    }

    template<typename U>
    ErrorOr<U, E> error_or(U value) &&
    {
        if (has_error())
            return ErrorOr<U, E>::from_error(std::move(*this).error());
        return ErrorOr<U, E>::from_value(std::move(value));
    }

private:
    ErrorOr(std::in_place_t, T value)
        : m_storage(std::in_place, std::move(value))
    {
    }
    ErrorOr(std::unexpect_t, E error)
        : m_storage(std::unexpect, std::move(error))
    {
    }
    std::expected<T, E> m_storage;
}; // class ErrorOr

inline Error report_error(ErrorCode errc, SourceLocation loc, diagnostic::Severity sv = diagnostic::Severity::ERROR)
{
    auto id = diagnostic::report(sv, loc, errc);
    Error err { Error { errc, loc } };
    err.set_diag_id(id);
    return err;
}

} // namespace fairuz

#endif // FAIRUZ_SUPPORT_ERROR_HPP
