#ifndef DIAGNOSTIC_HPP
#define DIAGNOSTIC_HPP

#include "macros.hpp"
#include <cstdint>
#include <exception>
#include <iostream>
#include <sstream>
#include <vector>

namespace mylang {
namespace diagnostic {

enum class Severity : uint8_t {
    NOTE,
    FATAL,
    ERROR,
    WARNING
};

namespace errc {

namespace parser {

enum class Code : uint16_t {
    EXPECTED_INDENT = 0x0001,
    EXPECTED_DEDENT = 0x0002,
    EXPECTED_LPAREN = 0x0003,
    EXPECTED_RPAREN_PARAMS = 0x0004,
    EXPECTED_RPAREN_ARGS = 0x0005,
    EXPECTED_RPAREN_EXPR = 0x0006,
    EXPECTED_RBRACKET = 0x0007,
    EXPECTED_COLON_IF = 0x0008,
    EXPECTED_COLON_WHILE = 0x0009,
    EXPECTED_COLON_FN = 0x000A,
    EXPECTED_FN_KEYWORD = 0x000B,
    EXPECTED_FN_NAME = 0x000C,
    EXPECTED_PARAM_NAME = 0x000D,
    EXPECTED_RETURN = 0x000E,
    EXPECTED_IF_KEYWORD = 0x000F,
    EXPECTED_WHILE_KEYWORD = 0x0010,
    INVALID_ASSIGN_TARGET = 0x0011,
    UNEXPECTED_TOKEN = 0x0012,
    UNEXPECTED_EOF = 0x0013,
    INVALID_OPERATOR_SEQ = 0x0014,
};

}

namespace sema {

enum class Code : uint16_t {
    UNDEFINED_VARIABLE = 0x0100,
    UNDEFINED_FUNCTION = 0x0101,
    NOT_CALLABLE = 0x0102,
    REDECLARATION = 0x0103,
    TYPE_MISMATCH = 0x0104,
    INVALID_STRING_OP = 0x0105,
    DIVISION_BY_ZERO_CONST = 0x0106,
    MISSING_RETURN = 0x0107,
    UNUSED_VARIABLE = 0x0108,
    LOOP_VAR_SHADOW = 0x0109,
    CONSTANT_CONDITION = 0x010A,
    INFINITE_LOOP = 0x010B,
    UNUSED_EXPR_RESULT = 0x010C,
};

}

namespace runtime {

enum class Code : uint16_t {
    STACK_OVERFLOW = 0x0200,
    STACK_UNDERFLOW = 0x0201,
    DIVISION_BY_ZERO = 0x0202,
    MODULO_BY_ZERO = 0x0203,
    TYPE_ERROR_ARITH = 0x0204,
    TYPE_ERROR_COMPARE = 0x0205,
    TYPE_ERROR_CALL = 0x0206,
    WRONG_ARG_COUNT = 0x0207,
    UNDEFINED_GLOBAL = 0x0208,
    UNDEFINED_LOCAL = 0x0209,
    INDEX_OUT_OF_BOUNDS = 0x020A,
    INDEX_TYPE_ERROR = 0x020B,
    UPVALUE_ESCAPE = 0x020C,
    INVALID_OPCODE = 0x020D,
    FRAME_OVERFLOW = 0x020E,
    NEGATIVE_EXPONENT = 0x020F,
};

}

namespace general {

enum class Code : uint16_t {
    ALLOC_FAILED = 0x0300,
    ARENA_EXHAUSTED = 0x0301,
    INTERNAL_ERROR = 0x0302,
    UNKNOWN = 0x0303,
};

}

} // namespace mylang::errc

static constexpr char const* errorMessageFor(uint16_t code)
{
    switch (code) {
    // parser
    case 0x0001:
        return "Expected indented block";
    case 0x0002:
        return "Expected dedent after block";
    case 0x0003:
        return "Expected '(' before parameters";
    case 0x0004:
        return "Expected ')' after parameters";
    case 0x0005:
        return "Expected ')' after arguments";
    case 0x0006:
        return "Expected ')' after expression";
    case 0x0007:
        return "Expected ']' after list elements";
    case 0x0008:
        return "Expected ':' after if condition";
    case 0x0009:
        return "Expected ':' after while condition";
    case 0x000A:
        return "Expected ':' after function parameters";
    case 0x000B:
        return "Expected 'fn' keyword";
    case 0x000C:
        return "Expected function name after 'fn'";
    case 0x000D:
        return "Expected parameter name";
    case 0x000E:
        return "Expected 'return' keyword";
    case 0x000F:
        return "Expected 'if' keyword";
    case 0x0010:
        return "Expected 'while' keyword";
    case 0x0011:
        return "Invalid assignment target";
    case 0x0012:
        return "Unexpected token";
    case 0x0013:
        return "Unexpected end of input";
    case 0x0014:
        return "Invalid operator sequence";
    // sema
    case 0x0100:
        return "Undefined variable";
    case 0x0101:
        return "Undefined function";
    case 0x0102:
        return "Expression is not callable";
    case 0x0103:
        return "Redeclaration of identifier";
    case 0x0104:
        return "Type mismatch in binary expression";
    case 0x0105:
        return "Only '+' is valid for string operands";
    case 0x0106:
        return "Division by zero (constant expression)";
    case 0x0107:
        return "Function may not return a value";
    case 0x0108:
        return "Unused variable";
    case 0x0109:
        return "Loop variable shadows outer variable";
    case 0x010A:
        return "Condition is always constant";
    case 0x010B:
        return "Infinite loop detected (condition is always true)";
    case 0x010C:
        return "Expression result is not used";
    // runtime
    case 0x0200:
        return "Stack overflow";
    case 0x0201:
        return "Stack underflow";
    case 0x0202:
        return "Division by zero";
    case 0x0203:
        return "Modulo by zero";
    case 0x0204:
        return "Arithmetic on non-numeric value";
    case 0x0205:
        return "Comparison between incompatible types";
    case 0x0206:
        return "Attempted to call a non-callable value";
    case 0x0207:
        return "Wrong number of arguments";
    case 0x0208:
        return "Undefined global variable";
    case 0x0209:
        return "Undefined local variable";
    case 0x020A:
        return "List index out of bounds";
    case 0x020B:
        return "List index must be an integer";
    case 0x020C:
        return "Upvalue accessed after enclosing scope exited";
    case 0x020D:
        return "Invalid opcode in dispatch loop";
    case 0x020E:
        return "Call frame limit exceeded";
    case 0x020F:
        return "Integer exponentiation with negative exponent";
    // general
    case 0x0300:
        return "Memory allocation failed";
    case 0x0301:
        return "Arena allocator exhausted";
    case 0x0302:
        return "Internal compiler error";
    case 0x0303:
        return "Unknown error";
    default:
        return "Unknown error";
    }
}

class DiagnosticEngine {
public:
    struct Diagnostic {
        Severity severity;
        uint32_t line;
        uint16_t column;
        uint16_t err_code;
        std::string code;
        std::vector<std::string> suggestions;
        std::vector<std::pair<int32_t, std::string>> notes;
    };

    // Maximum number of errors before the engine stops accumulating and
    // the parser should stop trying to recover. FATAL diagnostics bypass
    // this limit and always get recorded.
    static constexpr uint32_t LIMIT = 20;

    DiagnosticEngine() = default;

    // --- existing API, unchanged ---

    constexpr void emit(std::string const& msg, Severity const sv = Severity::ERROR)
    {
        emitError(msg, sv);
    }

    [[noreturn]] constexpr void panic(std::string const& msg) { _panic(msg); }

    // Accumulates a diagnostic. Does NOT emit to stderr.
    // If the engine is saturated (error count >= LIMIT), non-FATAL diagnostics
    // are silently dropped — the existing diagnostic list is still valid and
    // will be emitted in full when dump() is called.
    void report(Severity const sev, uint32_t const line, uint16_t const col,
        uint16_t err_code, std::string const& code = "");

    void addSuggestion(std::string const& suggestion);
    void addNote(std::int32_t line, std::string const& note);

    std::string toJSON() const;

    // Formats and writes all accumulated diagnostics to stderr. Safe to call
    // multiple times — subsequent calls are no-ops if nothing new was added.
    void prettyPrint() const;

    // --- new query API ---

    // True if any ERROR or FATAL diagnostic has been recorded.
    bool hasErrors() const noexcept { return ErrorCount_ > 0; }

    // True once ErrorCount_ hits LIMIT. The parser should stop recovery
    // attempts and return early when this is true — continuing will only
    // produce noise.
    bool isSaturated() const noexcept { return ErrorCount_ >= LIMIT; }

    uint32_t errorCount() const noexcept { return ErrorCount_; }
    uint32_t warningCount() const noexcept { return WarningCount_; }

    // Clears all accumulated diagnostics. Intended for unit tests that run
    // multiple parse passes through the same engine instance.
    void reset() noexcept
    {
        Diagnostics_.clear();
        ErrorCount_ = 0;
        WarningCount_ = 0;
    }

private:
    std::vector<Diagnostic> Diagnostics_;
    uint32_t ErrorCount_ = 0;
    uint32_t WarningCount_ = 0;

    void emitError(std::string const& msg, Severity const sv);
    [[noreturn]] void _panic(std::string const& msg) const;
    static std::string svToStr(Severity const sv);
    std::vector<std::string> splitLines(std::string const& text) const;
};

// --- module-level singletons and forwarding functions, unchanged ---

inline DiagnosticEngine engine;

static void emit(std::string const& msg, Severity const sv = Severity::ERROR)
{
    engine.emit(msg, sv);
}

static void panic(std::string const& msg) { engine.panic(msg); }

static void report(Severity const sev, uint32_t const line, uint16_t const col,
    uint16_t err_code, std::string const& code = "")
{
    engine.report(sev, line, col, err_code, code);
}

static void internalError(errc::general::Code err_code)
{
    engine.emit(errorMessageFor(static_cast<uint16_t>(err_code)));
}

static void runtimeError(errc::runtime::Code err_code)
{
    engine.emit(errorMessageFor(static_cast<uint16_t>(err_code)));
}

// Emits all accumulated diagnostics. The parser calls this once after
// parseProgram() returns, not after each individual error.
static void dump() { engine.prettyPrint(); }

// Forwarding wrappers for the new query API so callers don't need to
// reach into engine directly.
static bool hasErrors() noexcept { return engine.hasErrors(); }
static bool isSaturated() noexcept { return engine.isSaturated(); }
static uint32_t errorCount() noexcept { return engine.errorCount(); }
static uint32_t warningCount() noexcept { return engine.warningCount(); }
static void reset() noexcept { engine.reset(); }

} // namespace diagnostic
} // namespace mylang

#endif // DIAGNOSTIC_HPP
