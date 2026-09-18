#ifndef FA_DIAGNOSTIC_HPP
#define FA_DIAGNOSTIC_HPP

#include "fmacros.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace fairuz::lex {

class FileManager; // full definition in flexer.hpp; only a pointer is
                   // needed here, and including flexer.hpp would be
                   // circular (flexer.hpp already includes this file).

} // namespace fairuz::lex

namespace fairuz {
namespace diagnostic {

enum class Severity : u8 {
    NOTE,
    FATAL,
    ERROR,
    WARNING
}; // enum Severity

enum class ErrorCode : u16 {
    /* --- File Manager --- */
    FILE_NOT_FOUND = 0x0001,
    FILE_NOT_OPEN,
    SEEK_OUT_OF_BOUND,
    READ_ERROR,
    INVALID_UTF8,
    INVALID_CHAR_OFFSET,
    PERMISSION_DENIED,
    UNEXPECTED_EOF,
    SYSTEM_ERROR,
    ENCODING_ERROR,
    CACHE_ERROR,
    INVALID_LINE_NUMBER,
    BUFFER_TOO_SMALL,
    /* --- Lexer --- */
    INVALID_OCTAL_DIGIT = 0x0101,
    INVALID_BINARY_DIGIT,
    INVALID_BASE_LITERAL,
    INCONSISTENT_INDENTATION,
    TOO_MANY_INDENT_LEVELS,
    MIXED_INDENTATION,
    INVALID_UNINDENT,
    INVALID_ESCAPE_SEQUENCE,
    INVALID_CHARACTER,
    INVALID_OPERATOR,
    INVALID_NUMBER_LITERAL,
    UNTERMINATED_STRING,
    UNCLOSED_DELIMITER,
    MISMATCHED_DELIMITER,
    /* --- Parser --- */
    EXPECTED_INDENT = 0x0201,
    EXPECTED_DEDENT,
    EXPECTED_LPAREN,
    EXPECTED_RPAREN_PARAMS,
    EXPECTED_RPAREN_ARGS,
    EXPECTED_RPAREN_EXPR,
    EXPECTED_RBRACKET,
    EXPECTED_COLON_IF,
    EXPECTED_COLON_WHILE,
    EXPECTED_COLON_FN,
    EXPECTED_FN_KEYWORD,
    EXPECTED_FN_NAME,
    EXPECTED_PARAM_NAME,
    EXPECTED_RETURN,
    EXPECTED_IF_KEYWORD,
    EXPECTED_WHILE_KEYWORD,
    INVALID_ASSIGN_TARGET,
    UNEXPECTED_TOKEN,
    INVALID_OPERATOR_SEQ,
    EXPECTED_COLON_DICT,
    EXPECTED_RBRACE_EXPR,
    EXPECTED_FOR_TARGET,
    EXPECTED_IN_KEYWORD,
    EXPECTED_COLON_FOR,
    EXPECTED_CLASS_KEYWORD,
    EXPECTED_COLON_CLASS,
    EXPECTED_CLASS_NAME,
    EXPECTED_MEMBER_NAME,
    EXCEEDED_MAX_NESTING_LIMIT,
    EXPECTED_MODULE_NAME,
    EXPECTED_IMPORT_KEYWORD,
    EXPECTED_IMPORT_NAME,
    EXPECTED_ALIAS_NAME,
    EXPECTED_RPAREN_CLASS,
    /* --- Semantics --- */
    UNDEFINED_VARIABLE = 0x0300,
    UNDEFINED_FUNCTION,
    NOT_CALLABLE,
    REDECLARATION,
    TYPE_MISMATCH,
    INVALID_STRING_OP,
    DIVISION_BY_ZERO_CONST,
    MISSING_RETURN,
    UNUSED_VARIABLE,
    LOOP_VAR_SHADOW,
    CONSTANT_CONDITION,
    INFINITE_LOOP,
    UNUSED_EXPR_RESULT,
    /* --- Compiler --- */
    NULL_AST_ROOT = 0x0400,
    INVALID_STATEMENT_NODE,
    INVALID_EXPRESSION_NODE,
    NULL_FUNCTION_NAME,
    INVALID_FUNCTION_PARAMETER,
    FOR_NOT_IMPLEMENTED,
    UNKNOWN_LITERAL_TYPE,
    UNKNOWN_UNARY_OPERATOR,
    UNKNOWN_BINARY_OPERATOR,
    SHIFT_AMOUNT_NOT_CONSTANT,
    SHIFT_AMOUNT_OUT_OF_RANGE,
    TOO_MANY_CONSTANTS,
    TOO_MANY_REGISTERS,
    JUMP_OFFSET_OVERFLOW,
    LOOP_JUMP_OFFSET_OVERFLOW,
    NESTED_FUNCTION_UNSUPPORTED,
    BREAK_OUTSIDE_LOOP,
    CONTINUE_OUTSIDE_LOOP,
    NESTED_CLASS_UNSUPPORTED,
    TOO_MANY_LIST_ELEMENTS,
    TOO_MANY_FUNCTIONS,
    TOO_MANY_INLINE_CACHES,
    /* --- Runtime --- */
    STACK_OVERFLOW = 0x0500,
    STACK_UNDERFLOW,
    DIVISION_BY_ZERO,
    MODULO_BY_ZERO,
    TYPE_ERROR_ARITH,
    TYPE_ERROR_COMPARE,
    TYPE_ERROR_CALL,
    WRONG_ARG_COUNT,
    UNDEFINED_GLOBAL,
    UNDEFINED_LOCAL,
    INDEX_OUT_OF_BOUNDS,
    INDEX_TYPE_ERROR,
    INDEX_ASSIGN_TYPE_ERROR,
    INDEX_OBJECT_TYPE_ERROR,
    INVALID_OPCODE,
    FRAME_OVERFLOW,
    NEGATIVE_EXPONENT,
    NON_FUNCTION_CALL,
    NATIVE_ARG_COUNT,
    NATIVE_TYPE_ERROR,
    UNDEFINED_METHOD,
    UNDEFINED_FIELD,
    NUMERIC_OUT_OF_RANGE,
    MODULE_NOT_FOUND,
    /* --- Builtins --- */
    APPEND_ARG_COUNT = 0x0600,
    APPEND_TYPE_ERROR,
    POP_ARG_COUNT,
    POP_TYPE_ERROR,
    SLICE_ARG_COUNT,
    STR_ARG_COUNT,
    BOOL_ARG_COUNT,
    SUBSTR_ARG_COUNT,
    FLOOR_ARG_COUNT,
    FLOOR_TYPE_ERROR,
    CEIL_ARG_COUNT,
    CEIL_TYPE_ERROR,
    ROUND_ARG_COUNT,
    ROUND_TYPE_ERROR,
    ABS_ARG_COUNT,
    ABS_TYPE_ERROR,
    ABS_OUT_OF_RANGE,
    MIN_ARG_COUNT,
    MAX_ARG_COUNT,
    POW_ARG_COUNT,
    POW_TYPE_ERROR,
    SQRT_ARG_COUNT,
    SQRT_TYPE_ERROR,
    ASSERT_ARG_COUNT,
    ASSERT_FAILED,
    OPEN_ARG_COUNT,
    APPEND_FILE_ARG_COUNT,
    APPEND_FILE_TYPE_ERROR,
    APPEND_FILE_FAILED,
    CLOSE_ARG_COUNT,
    CLOSE_TYPE_ERROR,
    POP_EMPTY_LIST,
    /* --- Containers --- */
    ARRAY_EMPTY_BACK = 0x0700,
    ARRAY_EMPTY_FRONT,
    ARRAY_CAPACITY_EXCEEDED,
    ARRAY_OUT_OF_BOUNDS,
    STRING_SLICE_START_OOB,
    STRING_SLICE_END_BEFORE_START,
    /* --- General --- */
    ALLOC_FAILED = 0x0800,
    ARENA_EXHAUSTED,
    INTERNAL_ERROR,
    UNKNOWN,
    ALLOCATOR_CONTEXT_NOT_INITIALIZED,
    MMAP_FAILED,
    NANBOX_ADDRESS_UNSAFE,
    INVALID_PARAMETER,
}; // enum ErrorCode

static constexpr char const* error_message_for(ErrorCode const code)
{
    switch (code) {
    // file manager
    case ErrorCode::FILE_NOT_FOUND: return "File not found";
    case ErrorCode::FILE_NOT_OPEN: return "File is not open";
    case ErrorCode::SEEK_OUT_OF_BOUND: return "Seek position out of bounds";
    case ErrorCode::READ_ERROR: return "Failed to read from file";
    case ErrorCode::INVALID_UTF8: return "Invalid UTF-8 sequence encountered";
    case ErrorCode::INVALID_CHAR_OFFSET: return "Invalid character offset";
    case ErrorCode::PERMISSION_DENIED: return "Permission denied";
    case ErrorCode::UNEXPECTED_EOF: return "Unexpected end of file";
    case ErrorCode::SYSTEM_ERROR: return "System error occurred";
    case ErrorCode::ENCODING_ERROR: return "Encoding conversion error";
    case ErrorCode::CACHE_ERROR: return "Cache operation failed";
    case ErrorCode::INVALID_LINE_NUMBER: return "Invalid line number";
    case ErrorCode::BUFFER_TOO_SMALL: return "Buffer too small for operation";
    // lexer
    case ErrorCode::INVALID_OCTAL_DIGIT: return "Invalid digit in octal literal";
    case ErrorCode::INVALID_BINARY_DIGIT: return "Invalid digit in binary literal";
    case ErrorCode::INVALID_BASE_LITERAL: return "Invalid base-prefixed numeric literal";
    case ErrorCode::INCONSISTENT_INDENTATION: return "Inconsistent indentation";
    case ErrorCode::TOO_MANY_INDENT_LEVELS: return "Too many indentation levels";
    case ErrorCode::MIXED_INDENTATION: return "Mixed tabs and spaces in indentation";
    case ErrorCode::INVALID_UNINDENT: return "Unindent does not match an outer indentation level";
    case ErrorCode::INVALID_ESCAPE_SEQUENCE: return "Invalid escape sequence in string literal";
    case ErrorCode::INVALID_CHARACTER: return "Invalid character";
    case ErrorCode::INVALID_OPERATOR: return "Invalid operator token";
    case ErrorCode::INVALID_NUMBER_LITERAL: return "Invalid numeric literal";
    case ErrorCode::UNTERMINATED_STRING: return "Unterminated string literal";
    case ErrorCode::UNCLOSED_DELIMITER: return "Opening delimiter was never closed";
    case ErrorCode::MISMATCHED_DELIMITER: return "Unmatched closing delimiter";
    // parser
    case ErrorCode::EXPECTED_INDENT: return "Expected indented block";
    case ErrorCode::EXPECTED_DEDENT: return "Expected dedent after block";
    case ErrorCode::EXPECTED_LPAREN: return "Expected '(' before parameters";
    case ErrorCode::EXPECTED_RPAREN_PARAMS: return "Expected ')' after parameters";
    case ErrorCode::EXPECTED_RPAREN_ARGS: return "Expected ')' after arguments";
    case ErrorCode::EXPECTED_RPAREN_EXPR: return "Expected ')' after expression";
    case ErrorCode::EXPECTED_RBRACKET: return "Expected ']' after list elements";
    case ErrorCode::EXPECTED_COLON_IF: return "Expected ':' after if condition";
    case ErrorCode::EXPECTED_COLON_WHILE: return "Expected ':' after while condition";
    case ErrorCode::EXPECTED_COLON_FN: return "Expected ':' after function parameters";
    case ErrorCode::EXPECTED_FN_KEYWORD: return "Expected 'fn' keyword";
    case ErrorCode::EXPECTED_FN_NAME: return "Expected function name after 'fn'";
    case ErrorCode::EXPECTED_PARAM_NAME: return "Expected parameter name";
    case ErrorCode::EXPECTED_RETURN: return "Expected 'return' keyword";
    case ErrorCode::EXPECTED_IF_KEYWORD: return "Expected 'if' keyword";
    case ErrorCode::EXPECTED_WHILE_KEYWORD: return "Expected 'while' keyword";
    case ErrorCode::INVALID_ASSIGN_TARGET: return "Invalid assignment target";
    case ErrorCode::UNEXPECTED_TOKEN: return "Unexpected token";
    case ErrorCode::INVALID_OPERATOR_SEQ: return "Invalid operator sequence";
    case ErrorCode::EXPECTED_COLON_DICT: return "Expected ':' after dictionary key";
    case ErrorCode::EXPECTED_RBRACE_EXPR: return "Expected '}' after dictionary literal";
    case ErrorCode::EXPECTED_FOR_TARGET: return "Expected loop variable name after 'for'";
    case ErrorCode::EXPECTED_IN_KEYWORD: return "Expected 'in' after loop variable";
    case ErrorCode::EXPECTED_COLON_FOR: return "Expected ':' after for loop header";
    case ErrorCode::EXPECTED_CLASS_KEYWORD: return "Expected class keyword";
    case ErrorCode::EXPECTED_COLON_CLASS: return "Expected ':' after class name";
    case ErrorCode::EXPECTED_CLASS_NAME: return "Expected class name";
    case ErrorCode::EXPECTED_MEMBER_NAME: return "Expected member name";
    case ErrorCode::EXCEEDED_MAX_NESTING_LIMIT: return "Exceeded max nesting limit";
    case ErrorCode::EXPECTED_MODULE_NAME: return "Expected module name";
    case ErrorCode::EXPECTED_IMPORT_KEYWORD: return "Expected 'import' keyword";
    case ErrorCode::EXPECTED_IMPORT_NAME: return "Expected imported name";
    case ErrorCode::EXPECTED_ALIAS_NAME: return "Expected alias name";
    case ErrorCode::EXPECTED_RPAREN_CLASS: return "Expected ')' after parent class";
    // sema
    case ErrorCode::UNDEFINED_VARIABLE: return "Undefined variable";
    case ErrorCode::UNDEFINED_FUNCTION: return "Undefined function";
    case ErrorCode::NOT_CALLABLE: return "Expression is not callable";
    case ErrorCode::REDECLARATION: return "Redeclaration of identifier";
    case ErrorCode::TYPE_MISMATCH: return "Type mismatch in binary expression";
    case ErrorCode::INVALID_STRING_OP: return "Only '+' is valid for string operands";
    case ErrorCode::DIVISION_BY_ZERO_CONST: return "Division by zero (constant expression)";
    case ErrorCode::MISSING_RETURN: return "Not all code paths return a value";
    case ErrorCode::UNUSED_VARIABLE: return "Unused variable";
    case ErrorCode::LOOP_VAR_SHADOW: return "Loop variable shadows outer variable";
    case ErrorCode::CONSTANT_CONDITION: return "Condition is always true or always false";
    case ErrorCode::INFINITE_LOOP: return "Infinite loop detected (condition is always true)";
    case ErrorCode::UNUSED_EXPR_RESULT: return "Expression result is not used";
    // compiler
    case ErrorCode::NULL_AST_ROOT: return "Compiler received a null AST root";
    case ErrorCode::INVALID_STATEMENT_NODE: return "Invalid statement node";
    case ErrorCode::INVALID_EXPRESSION_NODE: return "Invalid expression node";
    case ErrorCode::NULL_FUNCTION_NAME: return "Function name is missing";
    case ErrorCode::INVALID_FUNCTION_PARAMETER: return "Function parameter must be a name";
    case ErrorCode::FOR_NOT_IMPLEMENTED: return "For loops are not implemented in the compiler";
    case ErrorCode::UNKNOWN_LITERAL_TYPE: return "Unknown literal type";
    case ErrorCode::UNKNOWN_UNARY_OPERATOR: return "Unknown unary operator";
    case ErrorCode::UNKNOWN_BINARY_OPERATOR: return "Unknown binary operator";
    case ErrorCode::SHIFT_AMOUNT_NOT_CONSTANT: return "Shift amount must be a constant integer";
    case ErrorCode::SHIFT_AMOUNT_OUT_OF_RANGE: return "Shift amount is out of range";
    case ErrorCode::TOO_MANY_CONSTANTS: return "Too many constants in function (max 65536)";
    case ErrorCode::TOO_MANY_REGISTERS: return "Too many registers allocated for function";
    case ErrorCode::JUMP_OFFSET_OVERFLOW: return "Jump offset overflow";
    case ErrorCode::LOOP_JUMP_OFFSET_OVERFLOW: return "Loop jump offset overflow";
    case ErrorCode::NESTED_FUNCTION_UNSUPPORTED: return "Nested function definitions are not supported";
    case ErrorCode::BREAK_OUTSIDE_LOOP: return "'break' used outside of a loop";
    case ErrorCode::CONTINUE_OUTSIDE_LOOP: return "'continue' used outside of a loop";
    case ErrorCode::NESTED_CLASS_UNSUPPORTED: return "Nested class definition is not supported";
    case ErrorCode::TOO_MANY_LIST_ELEMENTS: return "Too many elements in list object (max 255)";
    case ErrorCode::TOO_MANY_FUNCTIONS: return "Too many nested function chunks (max 65536)";
    case ErrorCode::TOO_MANY_INLINE_CACHES: return "Too many inline-cache slots in function (max 256)";
    // runtime
    case ErrorCode::STACK_OVERFLOW: return "Stack overflow";
    case ErrorCode::STACK_UNDERFLOW: return "Stack underflow";
    case ErrorCode::DIVISION_BY_ZERO: return "Division by zero";
    case ErrorCode::MODULO_BY_ZERO: return "Modulo by zero";
    case ErrorCode::TYPE_ERROR_ARITH: return "Arithmetic on non-numeric value";
    case ErrorCode::TYPE_ERROR_COMPARE: return "Comparison between incompatible types";
    case ErrorCode::TYPE_ERROR_CALL: return "Attempted to call a non-callable value";
    case ErrorCode::WRONG_ARG_COUNT: return "Wrong number of arguments";
    case ErrorCode::UNDEFINED_GLOBAL: return "Undefined global variable";
    case ErrorCode::UNDEFINED_LOCAL: return "Undefined local variable";
    case ErrorCode::INDEX_OUT_OF_BOUNDS: return "Index out of bounds";
    case ErrorCode::INDEX_TYPE_ERROR: return "Index must be an integer";
    case ErrorCode::INDEX_ASSIGN_TYPE_ERROR: return "Assignment target does not support index assignment";
    case ErrorCode::INDEX_OBJECT_TYPE_ERROR: return "Indexing into object which doesn't support index operator";
    case ErrorCode::INVALID_OPCODE: return "Invalid opcode in dispatch loop";
    case ErrorCode::FRAME_OVERFLOW: return "Call frame limit exceeded";
    case ErrorCode::NEGATIVE_EXPONENT: return "Integer exponentiation with negative exponent";
    case ErrorCode::NON_FUNCTION_CALL: return "Attempted to call a non-function value";
    case ErrorCode::NATIVE_ARG_COUNT: return "Native call received the wrong number of arguments";
    case ErrorCode::NATIVE_TYPE_ERROR: return "Native call received arguments of the wrong type";
    case ErrorCode::UNDEFINED_METHOD: return "Call to undefined method";
    case ErrorCode::UNDEFINED_FIELD: return "Undefined field";
    case ErrorCode::NUMERIC_OUT_OF_RANGE: return "Integer result is outside the signed 48-bit range";
    case ErrorCode::MODULE_NOT_FOUND: return "No module named";
    // stdlib
    case ErrorCode::APPEND_ARG_COUNT: return "append() expects at least two arguments";
    case ErrorCode::APPEND_TYPE_ERROR: return "append() expects a list as the first argument";
    case ErrorCode::POP_ARG_COUNT: return "pop() expects exactly one argument";
    case ErrorCode::POP_TYPE_ERROR: return "pop() expects a list argument";
    case ErrorCode::SLICE_ARG_COUNT: return "slice() expects exactly two or three arguments";
    case ErrorCode::STR_ARG_COUNT: return "str() expects zero or one argument";
    case ErrorCode::BOOL_ARG_COUNT: return "bool() expects exactly one argument";
    case ErrorCode::SUBSTR_ARG_COUNT: return "substr() expects exactly three arguments";
    case ErrorCode::FLOOR_ARG_COUNT: return "floor() expects exactly one argument";
    case ErrorCode::FLOOR_TYPE_ERROR: return "floor() expects a numeric argument";
    case ErrorCode::CEIL_ARG_COUNT: return "ceil() expects exactly one argument";
    case ErrorCode::CEIL_TYPE_ERROR: return "ceil() expects a numeric argument";
    case ErrorCode::ROUND_ARG_COUNT: return "round() expects exactly one argument";
    case ErrorCode::ROUND_TYPE_ERROR: return "round() expects a numeric argument";
    case ErrorCode::ABS_ARG_COUNT: return "abs() expects exactly one argument";
    case ErrorCode::ABS_TYPE_ERROR: return "abs() expects a numeric argument";
    case ErrorCode::ABS_OUT_OF_RANGE: return "abs() argument is out of range";
    case ErrorCode::MIN_ARG_COUNT: return "min() expects at least one argument";
    case ErrorCode::MAX_ARG_COUNT: return "max() expects at least one argument";
    case ErrorCode::POW_ARG_COUNT: return "pow() expects exactly two arguments";
    case ErrorCode::POW_TYPE_ERROR: return "pow() expects numeric arguments";
    case ErrorCode::SQRT_ARG_COUNT: return "sqrt() expects exactly one argument";
    case ErrorCode::SQRT_TYPE_ERROR: return "sqrt() expects a numeric argument";
    case ErrorCode::ASSERT_ARG_COUNT: return "assert expects at least one argument";
    case ErrorCode::ASSERT_FAILED: return "assertion failed";
    case ErrorCode::OPEN_ARG_COUNT: return "open() expects at least one argument";
    case ErrorCode::APPEND_FILE_ARG_COUNT: return "append_file() expects at least two arguments";
    case ErrorCode::APPEND_FILE_TYPE_ERROR: return "append_file() expects a file as first argument and a string as second argument";
    case ErrorCode::APPEND_FILE_FAILED: return "append_file() failed to write";
    case ErrorCode::CLOSE_ARG_COUNT: return "close() expects exactly one argument";
    case ErrorCode::CLOSE_TYPE_ERROR: return "close() expects a file value as argument";
    case ErrorCode::POP_EMPTY_LIST: return "pop() on an empty list";
    // containers
    case ErrorCode::ARRAY_EMPTY_BACK: return "Array::back() called on an empty array";
    case ErrorCode::ARRAY_EMPTY_FRONT: return "Array::front() called on an empty array";
    case ErrorCode::ARRAY_CAPACITY_EXCEEDED: return "Requested array capacity exceeds the maximum";
    case ErrorCode::ARRAY_OUT_OF_BOUNDS: return "Array index is out of bounds";
    case ErrorCode::STRING_SLICE_START_OOB: return "String slice start index is out of range";
    case ErrorCode::STRING_SLICE_END_BEFORE_START: return "String slice end must not precede start";
    // general
    case ErrorCode::ALLOC_FAILED: return "Memory allocation failed";
    case ErrorCode::ARENA_EXHAUSTED: return "Arena allocator exhausted";
    case ErrorCode::INTERNAL_ERROR: return "Internal compiler error";
    case ErrorCode::UNKNOWN: return "Unknown error";
    case ErrorCode::ALLOCATOR_CONTEXT_NOT_INITIALIZED: return "AllocatorContext is not initialized";
    case ErrorCode::MMAP_FAILED: return "mmap failed";
    case ErrorCode::NANBOX_ADDRESS_UNSAFE: return "mmap returned an address unsafe for NaN-boxing";
    case ErrorCode::INVALID_PARAMETER: return "Invalid parameters to function";
    default: return "Unknown error";
    }
}

struct DiagnosticAbort final : public std::runtime_error {
    DiagnosticAbort()
        : std::runtime_error("fatal diagnostic")
    {
    }
};

// Immutable source text survives parser/file-manager destruction and imports.
// One snapshot is shared by a compilation unit, its functions and diagnostics.
struct Source {
    std::string path;
    std::string text;
    std::vector<size_t> lines;
    Source(std::string path, std::string text);
    std::string_view line(u32 number) const;
};
using SourcePtr = std::shared_ptr<Source const>;

char const* error_type_for(u16 code);

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
        SrcLoc src_loc;
        ErrorCode err_code { 0 };
        std::string code { "" };
        std::vector<std::string> suggestions;
        std::vector<std::pair<i32, std::string>> notes;
        SourcePtr source;
        struct Frame {
            SourcePtr source;
            SrcLoc location;
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

    constexpr void emit(std::string const& msg, Severity const sv = Severity::ERROR) { emit_error(msg, sv); }

    [[noreturn]] constexpr void panic(std::string const& msg)
    {
        _panic(msg);
    }

    // Accumulates a diagnostic. Does NOT emit to stderr.
    // If the engine is saturated (error count >= LIMIT), non-FATAL diagnostics
    // are silently dropped — the existing diagnostic list is still valid and
    // will be emitted in full when dump() is called. Returns the id of the
    // diagnostic just recorded, or INVALID_ID if it was dropped (saturated
    // and non-FATAL) — callers that don't need the id can ignore the
    // return value as before; this is a source-compatible change from the
    // previous void-returning signature.
    // DiagnosticId report(Severity const sev, SrcLoc const loc, u16 err_code, std::string const& code = "");
    DiagnosticId report_deferred(Severity const sev, SrcLoc const loc, ErrorCode err_code, std::string const& code = "");

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
    void add_frame(DiagnosticId id, SourcePtr source, SrcLoc loc, std::string function);

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

    void set_source(lex::FileManager const* fm);
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
    void print_snippet(SourcePtr const& source, SrcLoc const& loc) const;
}; // class DiagnosticEngine

// --- module-level singletons and forwarding functions, unchanged ---

inline DiagnosticEngine engine;

static inline void emit(ErrorCode code, Severity const sv = Severity::ERROR)
{
    engine.report_deferred(sv, { }, code);
    engine.emit(error_message_for(code), sv);
}

static inline void fatal_error(ErrorCode code, std::string const& detail = "")
{
    engine.report_deferred(Severity::FATAL, { }, code, detail);
    std::string message = error_message_for(code);
    if (!detail.empty())
        message += ": " + detail;
    engine.emit(message, Severity::FATAL);
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
    Severity const sev, SrcLoc const loc, ErrorCode err_code, std::string const& code = "")
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
static inline void set_source(lex::FileManager const* fm) { engine.set_source(fm); }

class SourceScope {
public:
    explicit SourceScope(SourcePtr source)
        : m_previous(engine.source())
    {
        engine.set_source(std::move(source));
    }
    explicit SourceScope(lex::FileManager const* source)
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

using ErrorCode = fairuz::diagnostic::ErrorCode;

} // namespace fairuz

#endif // FA_DIAGNOSTIC_HPP
