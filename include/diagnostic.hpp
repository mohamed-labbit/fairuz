#ifndef DIAGNOSTIC_HPP
#define DIAGNOSTIC_HPP

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace mylang::diagnostic {

enum class Severity : uint8_t {
    NOTE,
    FATAL,
    ERROR,
    WARNING
}; // enum Severity

namespace errc {

namespace lexer {

enum class Code : uint16_t {
    FILE_NOT_OPEN = 0x0001,
    INVALID_OCTAL_DIGIT = 0x0002,
    INVALID_BINARY_DIGIT = 0x0003,
    INVALID_BASE_LITERAL = 0x0004,
    INCONSISTENT_INDENTATION = 0x0005,
    TOO_MANY_INDENT_LEVELS = 0x0006,
    MIXED_INDENTATION = 0x0007,
    INVALID_UNINDENT = 0x0008,
    INVALID_ESCAPE_SEQUENCE = 0x0009,
    INVALID_CHARACTER = 0x000A,
    INVALID_OPERATOR = 0x000B,
    INVALID_NUMBER_LITERAL = 0x000C,
}; // enum Code

} // namespace lexer

namespace parser {

enum class Code : uint16_t {
    EXPECTED_INDENT = 0x0101,
    EXPECTED_DEDENT = 0x0102,
    EXPECTED_LPAREN = 0x0103,
    EXPECTED_RPAREN_PARAMS = 0x0104,
    EXPECTED_RPAREN_ARGS = 0x0105,
    EXPECTED_RPAREN_EXPR = 0x0106,
    EXPECTED_RBRACKET = 0x0107,
    EXPECTED_COLON_IF = 0x0108,
    EXPECTED_COLON_WHILE = 0x0109,
    EXPECTED_COLON_FN = 0x010A,
    EXPECTED_FN_KEYWORD = 0x010B,
    EXPECTED_FN_NAME = 0x010C,
    EXPECTED_PARAM_NAME = 0x010D,
    EXPECTED_RETURN = 0x010E,
    EXPECTED_IF_KEYWORD = 0x010F,
    EXPECTED_WHILE_KEYWORD = 0x0110,
    INVALID_ASSIGN_TARGET = 0x0111,
    UNEXPECTED_TOKEN = 0x0112,
    UNEXPECTED_EOF = 0x0113,
    INVALID_OPERATOR_SEQ = 0x0114,
}; // enum Code

} // namespace parser

namespace sema {

enum class Code : uint16_t {
    UNDEFINED_VARIABLE = 0x0200,
    UNDEFINED_FUNCTION = 0x0201,
    NOT_CALLABLE = 0x0202,
    REDECLARATION = 0x0203,
    TYPE_MISMATCH = 0x0204,
    INVALID_STRING_OP = 0x0205,
    DIVISION_BY_ZERO_CONST = 0x0206,
    MISSING_RETURN = 0x0207,
    UNUSED_VARIABLE = 0x0208,
    LOOP_VAR_SHADOW = 0x0209,
    CONSTANT_CONDITION = 0x020A,
    INFINITE_LOOP = 0x020B,
    UNUSED_EXPR_RESULT = 0x020C,
}; // enum Code

} // namespace sema

namespace compiler {

enum class Code : uint16_t {
    NULL_AST_ROOT = 0x0300,
    INVALID_STATEMENT_NODE = 0x0301,
    INVALID_EXPRESSION_NODE = 0x0302,
    NULL_FUNCTION_NAME = 0x0303,
    INVALID_FUNCTION_PARAMETER = 0x0304,
    FOR_NOT_IMPLEMENTED = 0x0305,
    UNKNOWN_LITERAL_TYPE = 0x0306,
    UNKNOWN_UNARY_OPERATOR = 0x0307,
    UNKNOWN_BINARY_OPERATOR = 0x0308,
    SHIFT_AMOUNT_NOT_CONSTANT = 0x0309,
    SHIFT_AMOUNT_OUT_OF_RANGE = 0x030A,
    INVALID_ASSIGNMENT_TARGET = 0x030B,
    TOO_MANY_REGISTERS = 0x030C,
    JUMP_OFFSET_OVERFLOW = 0x030D,
    LOOP_JUMP_OFFSET_OVERFLOW = 0x030E,
}; // enum Code

} // namespace compiler

namespace runtime {

enum class Code : uint16_t {
    STACK_OVERFLOW = 0x0400,
    STACK_UNDERFLOW = 0x0401,
    DIVISION_BY_ZERO = 0x0402,
    MODULO_BY_ZERO = 0x0403,
    TYPE_ERROR_ARITH = 0x0404,
    TYPE_ERROR_COMPARE = 0x0405,
    TYPE_ERROR_CALL = 0x0406,
    WRONG_ARG_COUNT = 0x0407,
    UNDEFINED_GLOBAL = 0x0408,
    UNDEFINED_LOCAL = 0x0409,
    INDEX_OUT_OF_BOUNDS = 0x040A,
    INDEX_TYPE_ERROR = 0x040B,
    UPVALUE_ESCAPE = 0x040C,
    INVALID_OPCODE = 0x040D,
    FRAME_OVERFLOW = 0x040E,
    NEGATIVE_EXPONENT = 0x040F,
    NON_FUNCTION_CALL = 0x0410,
    NATIVE_ARG_COUNT = 0x0411,
    NATIVE_TYPE_ERROR = 0x0412,
}; // enum Code

} // namespace runtime

namespace stdlib {

enum class Code : uint16_t {
    APPEND_ARG_COUNT = 0x0500,
    APPEND_TYPE_ERROR = 0x0501,
    POP_ARG_COUNT = 0x0502,
    POP_TYPE_ERROR = 0x0503,
    SLICE_ARG_COUNT = 0x0504,
    STR_ARG_COUNT = 0x0505,
    BOOL_ARG_COUNT = 0x0506,
    SUBSTR_ARG_COUNT = 0x0507,
    FLOOR_ARG_COUNT = 0x0508,
    FLOOR_TYPE_ERROR = 0x0509,
    CEIL_ARG_COUNT = 0x050A,
    CEIL_TYPE_ERROR = 0x050B,
    ROUND_ARG_COUNT = 0x050C,
    ROUND_TYPE_ERROR = 0x050D,
    ABS_ARG_COUNT = 0x050E,
    ABS_TYPE_ERROR = 0x050F,
    ABS_OUT_OF_RANGE = 0x0510,
    MIN_ARG_COUNT = 0x0511,
    MAX_ARG_COUNT = 0x0512,
    POW_ARG_COUNT = 0x0513,
    POW_TYPE_ERROR = 0x0514,
    SQRT_ARG_COUNT = 0x0515,
    SQRT_TYPE_ERROR = 0x0516,
}; // enum Code

} // namespace stdlib

namespace container {

enum class Code : uint16_t {
    ARRAY_EMPTY_BACK = 0x0600,
    ARRAY_EMPTY_FRONT = 0x0601,
    ARRAY_CAPACITY_EXCEEDED = 0x0602,
    ARRAY_OUT_OF_BOUNDS = 0x0603,
    STRING_SLICE_START_OOB = 0x0604,
    STRING_SLICE_END_BEFORE_START = 0x0605,
}; // enum Code

} // namespace container

namespace general {

enum class Code : uint16_t {
    ALLOC_FAILED = 0x0700,
    ARENA_EXHAUSTED = 0x0701,
    INTERNAL_ERROR = 0x0702,
    UNKNOWN = 0x0703,
    ALLOCATOR_CONTEXT_NOT_INITIALIZED = 0x0704,
    MMAP_FAILED = 0x0705,
    NANBOX_ADDRESS_UNSAFE = 0x0706,
}; // enum Code

} // namespace general

} // namespace errc

static constexpr char const* errorMessageFor(uint16_t code)
{
    switch (code) {
    // lexer
    case 0x0001:
        return "Source file could not be opened";
    case 0x0002:
        return "Invalid digit in octal literal";
    case 0x0003:
        return "Invalid digit in binary literal";
    case 0x0004:
        return "Invalid base-prefixed numeric literal";
    case 0x0005:
        return "Inconsistent indentation";
    case 0x0006:
        return "Too many indentation levels";
    case 0x0007:
        return "Mixed tabs and spaces in indentation";
    case 0x0008:
        return "Unindent does not match an outer indentation level";
    case 0x0009:
        return "Invalid escape sequence in string literal";
    case 0x000A:
        return "Invalid character";
    case 0x000B:
        return "Invalid operator token";
    case 0x000C:
        return "Invalid numeric literal";
    // parser
    case 0x0101:
        return "Expected indented block";
    case 0x0102:
        return "Expected dedent after block";
    case 0x0103:
        return "Expected '(' before parameters";
    case 0x0104:
        return "Expected ')' after parameters";
    case 0x0105:
        return "Expected ')' after arguments";
    case 0x0106:
        return "Expected ')' after expression";
    case 0x0107:
        return "Expected ']' after list elements";
    case 0x0108:
        return "Expected ':' after if condition";
    case 0x0109:
        return "Expected ':' after while condition";
    case 0x010A:
        return "Expected ':' after function parameters";
    case 0x010B:
        return "Expected 'fn' keyword";
    case 0x010C:
        return "Expected function name after 'fn'";
    case 0x010D:
        return "Expected parameter name";
    case 0x010E:
        return "Expected 'return' keyword";
    case 0x010F:
        return "Expected 'if' keyword";
    case 0x0110:
        return "Expected 'while' keyword";
    case 0x0111:
        return "Invalid assignment target";
    case 0x0112:
        return "Unexpected token";
    case 0x0113:
        return "Unexpected end of input";
    case 0x0114:
        return "Invalid operator sequence";
    // sema
    case 0x0200:
        return "Undefined variable";
    case 0x0201:
        return "Undefined function";
    case 0x0202:
        return "Expression is not callable";
    case 0x0203:
        return "Redeclaration of identifier";
    case 0x0204:
        return "Type mismatch in binary expression";
    case 0x0205:
        return "Only '+' is valid for string operands";
    case 0x0206:
        return "Division by zero (constant expression)";
    case 0x0207:
        return "Function may not return a value";
    case 0x0208:
        return "Unused variable";
    case 0x0209:
        return "Loop variable shadows outer variable";
    case 0x020A:
        return "Condition is always constant";
    case 0x020B:
        return "Infinite loop detected (condition is always true)";
    case 0x020C:
        return "Expression result is not used";
    // compiler
    case 0x0300:
        return "Compiler received a null AST root";
    case 0x0301:
        return "Invalid statement node";
    case 0x0302:
        return "Invalid expression node";
    case 0x0303:
        return "Function name is missing";
    case 0x0304:
        return "Function parameter must be a name";
    case 0x0305:
        return "For loops are not implemented in the compiler";
    case 0x0306:
        return "Unknown literal type";
    case 0x0307:
        return "Unknown unary operator";
    case 0x0308:
        return "Unknown binary operator";
    case 0x0309:
        return "Shift amount must be a constant integer";
    case 0x030A:
        return "Shift amount is out of range";
    case 0x030B:
        return "Invalid assignment target in compiler";
    case 0x030C:
        return "Too many registers allocated for function";
    case 0x030D:
        return "Jump offset overflow";
    case 0x030E:
        return "Loop jump offset overflow";
    // runtime
    case 0x0400:
        return "Stack overflow";
    case 0x0401:
        return "Stack underflow";
    case 0x0402:
        return "Division by zero";
    case 0x0403:
        return "Modulo by zero";
    case 0x0404:
        return "Arithmetic on non-numeric value";
    case 0x0405:
        return "Comparison between incompatible types";
    case 0x0406:
        return "Attempted to call a non-callable value";
    case 0x0407:
        return "Wrong number of arguments";
    case 0x0408:
        return "Undefined global variable";
    case 0x0409:
        return "Undefined local variable";
    case 0x040A:
        return "List index out of bounds";
    case 0x040B:
        return "List index must be an integer";
    case 0x040C:
        return "Upvalue accessed after enclosing scope exited";
    case 0x040D:
        return "Invalid opcode in dispatch loop";
    case 0x040E:
        return "Call frame limit exceeded";
    case 0x040F:
        return "Integer exponentiation with negative exponent";
    case 0x0410:
        return "Attempted to call a non-function value";
    case 0x0411:
        return "Native call received the wrong number of arguments";
    case 0x0412:
        return "Native call received arguments of the wrong type";
    // stdlib
    case 0x0500:
        return "append() expects at least two arguments";
    case 0x0501:
        return "append() expects a list as the first argument";
    case 0x0502:
        return "pop() expects exactly one argument";
    case 0x0503:
        return "pop() expects a list argument";
    case 0x0504:
        return "slice() expects at least two arguments";
    case 0x0505:
        return "str() expects zero or one argument";
    case 0x0506:
        return "bool() expects exactly one argument";
    case 0x0507:
        return "substr() expects exactly three arguments";
    case 0x0508:
        return "floor() expects exactly one argument";
    case 0x0509:
        return "floor() expects a numeric argument";
    case 0x050A:
        return "ceil() expects exactly one argument";
    case 0x050B:
        return "ceil() expects a numeric argument";
    case 0x050C:
        return "round() expects exactly one argument";
    case 0x050D:
        return "round() expects a numeric argument";
    case 0x050E:
        return "abs() expects exactly one argument";
    case 0x050F:
        return "abs() expects a numeric argument";
    case 0x0510:
        return "abs() argument is out of range";
    case 0x0511:
        return "min() expects at least one argument";
    case 0x0512:
        return "max() expects at least one argument";
    case 0x0513:
        return "pow() expects exactly two arguments";
    case 0x0514:
        return "pow() expects numeric arguments";
    case 0x0515:
        return "sqrt() expects exactly one argument";
    case 0x0516:
        return "sqrt() expects a numeric argument";
    // containers
    case 0x0600:
        return "Array::back() called on an empty array";
    case 0x0601:
        return "Array::front() called on an empty array";
    case 0x0602:
        return "Requested array capacity exceeds the maximum";
    case 0x0603:
        return "Array index is out of bounds";
    case 0x0604:
        return "String slice start index is out of range";
    case 0x0605:
        return "String slice end must not precede start";
    // general
    case 0x0700:
        return "Memory allocation failed";
    case 0x0701:
        return "Arena allocator exhausted";
    case 0x0702:
        return "Internal compiler error";
    case 0x0703:
        return "Unknown error";
    case 0x0704:
        return "AllocatorContext is not initialized";
    case 0x0705:
        return "mmap failed";
    case 0x0706:
        return "mmap returned an address unsafe for NaN-boxing";
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
    }; // struct Diagnostic

    // Maximum number of errors before the engine stops accumulating and
    // the parser should stop trying to recover. FATAL diagnostics bypass
    // this limit and always get recorded.
    static constexpr uint32_t LIMIT = 20;

    DiagnosticEngine() = default;

    // --- existing API, unchanged ---

    constexpr void emit(std::string const& msg, Severity const sv = Severity::ERROR) { emitError(msg, sv); }

    [[noreturn]] constexpr void panic(std::string const& msg)
    {
        if (hasErrors())
            prettyPrint();
        _panic(msg);
    }

    // Accumulates a diagnostic. Does NOT emit to stderr.
    // If the engine is saturated (error count >= LIMIT), non-FATAL diagnostics
    // are silently dropped — the existing diagnostic list is still valid and
    // will be emitted in full when dump() is called.
    void report(Severity const sev, uint32_t const line, uint16_t const col, uint16_t err_code, std::string const& code = "");

    void addSuggestion(std::string const& suggestion);
    void addNote(int32_t line, std::string const& note);

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
}; // class DiagnosticEngine

// --- module-level singletons and forwarding functions, unchanged ---

inline DiagnosticEngine engine;

template<typename CodeEnum>
static constexpr uint16_t codeValue(CodeEnum code)
{
    static_assert(std::is_enum_v<CodeEnum>, "diagnostic code must be an enum");
    return static_cast<uint16_t>(code);
}

static void emit(std::string const& msg, Severity const sv = Severity::ERROR) { engine.emit(msg, sv); }

template<typename CodeEnum>
static void emit(CodeEnum code, Severity const sv = Severity::ERROR)
{
    engine.emit(errorMessageFor(codeValue(code)), sv);
}

template<typename CodeEnum>
static void emit(CodeEnum code, std::string const& detail, Severity const sv = Severity::ERROR)
{
    std::string message = errorMessageFor(codeValue(code));
    if (!detail.empty())
        message += ": " + detail;
    engine.emit(message, sv);
}

static void panic(std::string const& msg) { engine.panic(msg); }

template<typename CodeEnum>
[[noreturn]] static void panic(CodeEnum code)
{
    engine.panic(errorMessageFor(codeValue(code)));
}

template<typename CodeEnum>
[[noreturn]] static void panic(CodeEnum code, std::string const& detail)
{
    std::string message = errorMessageFor(codeValue(code));
    if (!detail.empty())
        message += ": " + detail;
    engine.panic(message);
}

static void report(Severity const sev, uint32_t const line, uint16_t const col, uint16_t err_code, std::string const& code = "")
{
    engine.report(sev, line, col, err_code, code);
}

template<typename CodeEnum>
static void report(Severity const sev, uint32_t const line, uint16_t const col, CodeEnum code, std::string const& snippet = "")
{
    engine.report(sev, line, col, codeValue(code), snippet);
}

static void internalError(errc::general::Code err_code) { emit(err_code); }

static void runtimeError(errc::runtime::Code err_code) { emit(err_code); }

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

} // namespace mylang::diagnostic

#endif // DIAGNOSTIC_HPP
