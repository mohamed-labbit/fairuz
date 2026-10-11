#pragma once

#include "Macros.hpp"

namespace fairuz {

// Diagnostic domains, not recoverability or VM-health policy. The same code
// can describe an ordinary failure or an invariant violation at different sites.
enum class ErrorCategory : u8 {
    FILE,
    LEXICAL,
    SYNTAX,
    SEMANTIC,
    COMPILE,
    RUNTIME,
    BUILTIN,
    CONTAINER,
    INTERNAL,
    UNKNOWN
};

enum class ErrorCode : u16 {
#define FAIRUZ_ERROR(name, number, category, message) name = number,
#include "errc.def"
#undef FAIRUZ_ERROR
};

inline constexpr ErrorCode all_error_codes[] = {
#define FAIRUZ_ERROR(name, number, category, message) ErrorCode::name,
#include "errc.def"
#undef FAIRUZ_ERROR
};

constexpr char const* error_name_for(ErrorCode code) noexcept
{
    switch (code) {
#define FAIRUZ_ERROR(name, number, category, message) \
    case ErrorCode::name: return #name;
#include "errc.def"
#undef FAIRUZ_ERROR
    default: return "UNKNOWN";
    }
}

constexpr char const* error_message_for(ErrorCode code) noexcept
{
    switch (code) {
#define FAIRUZ_ERROR(name, number, category, message) \
    case ErrorCode::name: return message;
#include "errc.def"
#undef FAIRUZ_ERROR
    default: return "Unknown error";
    }
}

constexpr ErrorCategory error_category_for(ErrorCode code) noexcept
{
    switch (code) {
#define FAIRUZ_ERROR(name, number, category, message) \
    case ErrorCode::name: return ErrorCategory::category;
#include "errc.def"
#undef FAIRUZ_ERROR
    default: return ErrorCategory::UNKNOWN;
    }
}

constexpr char const* error_category_name(ErrorCategory category) noexcept
{
    switch (category) {
    case ErrorCategory::FILE: return "file";
    case ErrorCategory::LEXICAL: return "lexical";
    case ErrorCategory::SYNTAX: return "syntax";
    case ErrorCategory::SEMANTIC: return "semantic";
    case ErrorCategory::COMPILE: return "compile";
    case ErrorCategory::RUNTIME: return "runtime";
    case ErrorCategory::BUILTIN: return "builtin";
    case ErrorCategory::CONTAINER: return "container";
    case ErrorCategory::INTERNAL: return "internal";
    case ErrorCategory::UNKNOWN: return "unknown";
    }
    return "unknown";
}

} // namespace fairuz
