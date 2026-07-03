/// parser.cc — refactored
///
/// ============================================================================
/// REQUIRED CHANGES TO parser.hpp (keep this list, delete once done):
///
///   Fa_SymbolTable:
///     + Fa_SymbolTable* get_parent() const;          ← new accessor
///     + Symbol const*   lookup(Fa_StringRef const&) const;  ← const overload
///       (optional but enables const-correct infer_type one day)
///
///   Fa_SemanticAnalyzer:
///     - bool is_local(Fa_StringRef, Fa_SymbolTable*);   ← remove entirely
///
///   Fa_Parser:
///     ~ bool check(TokType) const;   ← add const qualifier
///     Remove declarations (keep as delegating stubs in .cc until cleaned up):
///       parse_logical_expr, parse_logical_expr_precedence,
///       parse_comparison_expr, parse_binary_expr, parse_binary_expr_precedence
///     They now all delegate to the unified parse_binary_expr_precedence.
///     Alternatively, rename parse_binary_expr_precedence in the header to make
///     the unification visible; a new private method name is not needed because
///     we reuse the existing signature.
/// ============================================================================

#include "parser.hpp"
#include "loop_header.hpp"
#include "macros.hpp"
#include "util.hpp"

#include <iostream>

namespace fairuz::parser {

// ─────────────────────────────────────────────────────────────────────────────
// Macros
// ─────────────────────────────────────────────────────────────────────────────

// Consume a token, early-return the error code if the token doesn't match.
#define Fa_VERIFY_TOKEN(expected, errc) \
    do {                                \
        if (UNLIKELY(!match(expected))) \
            return report_error(errc);  \
    } while (0)

// Propagate an error from an Fa_ErrorOr expression without unwrapping.
#define Fa_VERIFY_NODE(n)              \
    do {                               \
        if (UNLIKELY((n).has_error())) \
            return (n).error();        \
    } while (0)

// Token-pasting helpers for unique temporary names (standard C++, no GNU extension).
#define FA_CONCAT_(a, b) a##b
#define FA_CONCAT(a, b) FA_CONCAT_(a, b)

// Fa_TRY — replaces the GNU ({...}) statement-expression macros.
//
// Declares `var` as the unwrapped value of `expr` (which must return
// Fa_ErrorOr<T>).  Early-returns the error from the *enclosing function* if
// any.  Each expansion creates a uniquely named temporary via __LINE__, so
// multiple Fa_TRY calls in the same scope are safe as long as they appear on
// different source lines (which they always should).
#define Fa_TRY(var, expr)                                   \
    auto FA_CONCAT(fa_try_, __LINE__) = (expr);             \
    if (UNLIKELY(FA_CONCAT(fa_try_, __LINE__).has_error())) \
        return FA_CONCAT(fa_try_, __LINE__).error();        \
    auto var = std::move(FA_CONCAT(fa_try_, __LINE__)).value()

// ─────────────────────────────────────────────────────────────────────────────
// Type aliases
// ─────────────────────────────────────────────────────────────────────────────

using TokType = tok::Fa_TokenType;
using StmtPtr = AST::Fa_Stmt*;
using ExprPtr = AST::Fa_Expr*;
using TokenPtr = tok::Fa_Token const*;
using Fa_Type = Fa_SymbolTable::DataType;
using ParserCode = diagnostic::errc::parser::Code;
using SemaCode = diagnostic::errc::sema::Code;

// Shared between the parser (parse_class_method) and the semantic analyser
// (analyze_stmt CLASS_DEF).  Move to ast_constants.hpp if the two components
// are ever split into separate translation units.
static constexpr char kClassInstanceName[] = "__class$instance";

// ─────────────────────────────────────────────────────────────────────────────
// File-local helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

// Maps an augmented-assignment operator token to the corresponding binary op.
// Replaces the fragile pointer-arithmetic enum indexing used previously.
AST::Fa_BinaryOp augmented_assign_to_binary_op(TokType t)
{
    switch (t) {
    case TokType::OP_PLUSEQ: return AST::Fa_BinaryOp::OP_ADD;
    case TokType::OP_MINUSEQ: return AST::Fa_BinaryOp::OP_SUB;
    case TokType::OP_STAREQ: return AST::Fa_BinaryOp::OP_MUL;
    case TokType::OP_SLASHEQ: return AST::Fa_BinaryOp::OP_DIV;
    case TokType::OP_PERCENTEQ: return AST::Fa_BinaryOp::OP_MOD;
    default: return AST::Fa_BinaryOp::INVALID;
    }
}

bool is_augmented_assign_tok(TokenPtr t)
{
    return t->is(TokType::OP_PLUSEQ) || t->is(TokType::OP_MINUSEQ)
        || t->is(TokType::OP_STAREQ) || t->is(TokType::OP_SLASHEQ)
        || t->is(TokType::OP_PERCENTEQ);
}

// Conservative definite-return check used to warn about missing returns.
bool stmt_definitely_returns(AST::Fa_Stmt const* stmt)
{
    if (stmt == nullptr)
        return false;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::RETURN:
        return true;

    case AST::Fa_Stmt::Kind::BLOCK: {
        for (AST::Fa_Stmt const* child : AS_CONST_BLOCK(stmt)->get_statements())
            if (stmt_definitely_returns(child))
                return true;
        return false;
    }

    case AST::Fa_Stmt::Kind::IF: {
        auto* s = AS_CONST_IF(stmt);
        return stmt_definitely_returns(s->get_then())
            && stmt_definitely_returns(s->get_else());
    }

    default:
        return false;
    }
}

// ─── RAII scope guard ────────────────────────────────────────────────────────
//
// Usage A – create a fresh child scope automatically:
//   ScopeGuard guard{m_current_scope};
//
// Usage B – adopt an existing child scope (e.g. class method scopes that are
//   children of class_scope, not of m_current_scope):
//   ScopeGuard guard{m_current_scope, class_scope->create_child()};
//
// In both cases the destructor restores m_current_scope to its original value,
// even if analysis of the body returns early.

struct ScopeGuard {
    Fa_SymbolTable*& ref;
    Fa_SymbolTable* saved;

    // Creates a new child scope.
    explicit ScopeGuard(Fa_SymbolTable*& r)
        : ref(r)
        , saved(r)
    {
        ref = r->create_child();
    }

    // Adopts an existing scope as the new current scope.
    ScopeGuard(Fa_SymbolTable*& r, Fa_SymbolTable* child)
        : ref(r)
        , saved(r)
    {
        ref = child;
    }

    ~ScopeGuard() { ref = saved; }

    ScopeGuard(ScopeGuard const&) = delete;
    ScopeGuard& operator=(ScopeGuard const&) = delete;
};

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SymbolTable
// ─────────────────────────────────────────────────────────────────────────────

Fa_SymbolTable::Fa_SymbolTable(Fa_SymbolTable* p, i32 level)
    : m_parent(p)
    , m_scope_level(level)
{
}

void Fa_SymbolTable::define(Fa_StringRef const& name, Symbol symbol)
{
    if (lookup_local(name))
        return;
    symbol.name = name;
    m_symbols[name] = symbol;
}

// Non-const: walks the parent chain, returns a mutable pointer.
typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup(Fa_StringRef const& name)
{
    if (m_symbols.contains(name))
        return &m_symbols[name];
    return m_parent ? m_parent->lookup(name) : nullptr;
}

// Const overload for use from const contexts (e.g. infer_type if made const).
// NOTE: Add declaration to parser.hpp.
typename Fa_SymbolTable::Symbol const* Fa_SymbolTable::lookup(Fa_StringRef const& name) const
{
    if (m_symbols.contains(name))
        return m_symbols.find_ptr(name);
    return m_parent ? m_parent->lookup(name) : nullptr;
}

typename Fa_SymbolTable::Symbol* Fa_SymbolTable::lookup_local(Fa_StringRef const& name)
{
    return m_symbols.find_ptr(name);
}

// is_defined is now a direct wrapper around lookup — no separate recursive
// walk needed since lookup already traverses the parent chain.
bool Fa_SymbolTable::is_defined(Fa_StringRef const& name) const
{
    return lookup(name) != nullptr;
}

void Fa_SymbolTable::mark_used(Fa_StringRef const& name, i32 line)
{
    if (Symbol* sym = lookup(name)) {
        sym->is_used = true;
        sym->usage_lines.push(line);
    }
}

// Accessor — avoids direct m_parent access from Fa_SemanticAnalyzer.
// NOTE: Add declaration to parser.hpp.
Fa_SymbolTable* Fa_SymbolTable::get_parent() const { return m_parent; }

Fa_SymbolTable* Fa_SymbolTable::create_child()
{
    Fa_SymbolTable* child = get_allocator().allocate_object<Fa_SymbolTable>(this, m_scope_level + 1);
    m_children.push(child);
    return child;
}

Fa_Array<typename Fa_SymbolTable::Symbol*> Fa_SymbolTable::get_unused_symbols()
{
    Fa_Array<Symbol*> unused;
    for (auto [_, sym] : m_symbols) {
        if (!sym.is_used && sym.symbol_type == SymbolType::VARIABLE)
            unused.push(&sym);
    }
    return unused;
}

Fa_HashTable<Fa_StringRef, typename Fa_SymbolTable::Symbol,
    Fa_StringRefHash, Fa_StringRefEqual> const&
Fa_SymbolTable::get_symbols() const { return m_symbols; }

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SemanticAnalyzer — constructor
// ─────────────────────────────────────────────────────────────────────────────

Fa_SemanticAnalyzer::Fa_SemanticAnalyzer()
{
    m_global_scope = get_allocator().allocate_object<Fa_SymbolTable>();
    m_current_scope = m_global_scope;

    // Table-driven registration — adding a builtin now means updating only
    // this list AND the corresponding native implementation in stdlib.
    // If Arabic/English pairs diverge, use a separate struct.
    static constexpr char const* kBuiltins[] = {
        "print",
        "اكتب",
        "len",
        "حجم",
        "append",
        "اضف",
        "احذف",
        "مقطع",
        "input",
        "نوع",
        "طبيعي",
        "حقيقي",
        "str",
        "bool",
        "قائمة",
        "جدول",
        "dict",
        "اقسم",
        "join",
        "substr",
        "contains",
        "trim",
        "floor",
        "ceil",
        "round",
        "abs",
        "min",
        "max",
        "pow",
        "sqrt",
        "assert",
        "clock",
        "error",
        "time",
    };

    auto register_builtin = [this](Fa_StringRef name) {
        Fa_SymbolTable::Symbol sym;
        sym.name = name;
        sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        sym.data_type = Fa_Type::FUNCTION;
        m_global_scope->define(name, sym);
    };

    for (auto const* name : kBuiltins)
        register_builtin(name);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SemanticAnalyzer — type inference
// ─────────────────────────────────────────────────────────────────────────────

Fa_Type Fa_SemanticAnalyzer::infer_type(ExprPtr expr)
{
    if (expr == nullptr)
        return Fa_Type::UNKNOWN;

    switch (expr->get_kind()) {

    case AST::Fa_Expr::Kind::LITERAL: {
        auto lit = AS_CONST_LITERAL(expr);
        if (lit->is_string())
            return Fa_Type::STRING;
        if (lit->is_float())
            return Fa_Type::FLOAT;
        if (lit->is_integer())
            return Fa_Type::INTEGER;
        if (lit->is_bool())
            return Fa_Type::BOOLEAN;
        return Fa_Type::NONE;
    }

    case AST::Fa_Expr::Kind::NAME: {
        // FIX: explicit return in both branches — avoids the silent fall-through
        // to UNKNOWN that made the "symbol found, return its type" path implicit.
        auto name = AS_CONST_NAME(expr);
        Fa_SymbolTable::Symbol const* sym = m_current_scope->lookup(name->get_value());
        return sym ? sym->data_type : Fa_Type::UNKNOWN;
    }

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = AS_CONST_BINARY(expr);
        Fa_Type ltype = infer_type(bin->get_left());
        Fa_Type rtype = infer_type(bin->get_right());

        if (ltype == Fa_Type::FLOAT || rtype == Fa_Type::FLOAT)
            return Fa_Type::FLOAT;
        if (ltype == Fa_Type::INTEGER && rtype == Fa_Type::INTEGER)
            return Fa_Type::INTEGER;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_ADD
            && ltype == Fa_Type::STRING)
            return Fa_Type::STRING;
        if (bin->get_operator() == AST::Fa_BinaryOp::OP_AND
            || bin->get_operator() == AST::Fa_BinaryOp::OP_OR)
            return Fa_Type::BOOLEAN;
        // Operator overloads — cannot resolve without the class registry.
        if (ltype == Fa_Type::INSTANCE || rtype == Fa_Type::INSTANCE)
            return Fa_Type::UNKNOWN;
        return Fa_Type::UNKNOWN;
    }

    case AST::Fa_Expr::Kind::INDEX: {
        auto idx = AS_CONST_INDEX(expr);
        Fa_Type ctype = infer_type(idx->get_object());

        switch (ctype) {
        case Fa_Type::LIST: {
            auto list = AS_CONST_LIST(idx->get_object());
            return list->is_empty()
                ? Fa_Type::ANY
                : infer_type(list->get_elements()[0]);
        }
        case Fa_Type::STRING:
            return Fa_Type::STRING;
        case Fa_Type::DICT: {
            // Only perform key lookup if the container is a literal dict, not
            // a named variable of dict type (which would require runtime info).
            if (idx->get_object()->get_kind() != AST::Fa_Expr::Kind::DICT)
                return Fa_Type::ANY;
            auto dict = AS_CONST_DICT(idx->get_object());
            if (dict->get_content().empty())
                return Fa_Type::ANY;
            for (auto const& [k, v] : dict->get_content())
                if (k->equals(idx->get_index()))
                    return infer_type(v);
            return Fa_Type::ANY;
        }
        case Fa_Type::INSTANCE:
            return Fa_Type::UNKNOWN; // field type requires the compiler's class registry
        default:
            return Fa_Type::UNKNOWN;
        }
    }

    case AST::Fa_Expr::Kind::GET:
        // Resolving member types requires the compiler's class registry.
        return Fa_Type::UNKNOWN;

    case AST::Fa_Expr::Kind::UNARY:
        return infer_type(AS_CONST_UNARY(expr)->get_operand());

    case AST::Fa_Expr::Kind::ASSIGNMENT:
        return infer_type(AS_CONST_ASSIGNMENT_EXPR(expr)->get_value());

    case AST::Fa_Expr::Kind::LIST: return Fa_Type::LIST;
    case AST::Fa_Expr::Kind::CALL: return Fa_Type::ANY;
    case AST::Fa_Expr::Kind::DICT: return Fa_Type::DICT;

    default:
        return Fa_Type::UNKNOWN;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SemanticAnalyzer — helpers
// ─────────────────────────────────────────────────────────────────────────────

void Fa_SemanticAnalyzer::report_issue(
    Issue::Severity sev, SemaCode code,
    Fa_StringRef msg, Fa_SourceLocation loc, Fa_StringRef const& sugg)
{
    m_issues.push({ sev, static_cast<u16>(code), msg, loc, sugg });
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SemanticAnalyzer — expression analysis
// ─────────────────────────────────────────────────────────────────────────────

void Fa_SemanticAnalyzer::analyze_expr(ExprPtr expr)
{
    if (expr == nullptr)
        return;

    switch (expr->get_kind()) {

    case AST::Fa_Expr::Kind::NAME: {
        auto* name = AS_NAME(expr);
        if (!m_current_scope->is_defined(name->get_value())) {
            report_issue(Issue::Severity::ERROR,
                SemaCode::UNDEFINED_VARIABLE,
                "Undefined variable: " + name->get_value(),
                expr->get_location(),
                "Did you forget to initialize it?");
        } else {
            m_current_scope->mark_used(name->get_value(), expr->get_location().line);
            // FIX: is_local(name, scope) was identical to scope->is_defined(name) —
            // both walk the parent chain via the same recursive lookup.
            // Removed is_local entirely; is_defined is the single source of truth.
            name->set_local();
        }
    } break;

    case AST::Fa_Expr::Kind::BINARY: {
        auto bin = AS_CONST_BINARY(expr);
        analyze_expr(bin->get_left());
        analyze_expr(bin->get_right());

        Fa_Type ltype = infer_type(bin->get_left());
        Fa_Type rtype = infer_type(bin->get_right());

        // Instance operands may have operator overloads — skip static type checks.
        bool either_instance = (ltype == Fa_Type::INSTANCE || rtype == Fa_Type::INSTANCE);

        if (!either_instance
            && ltype != rtype
            && ltype != Fa_Type::UNKNOWN && rtype != Fa_Type::UNKNOWN
            && ltype != Fa_Type::ANY && rtype != Fa_Type::ANY)
            report_issue(Issue::Severity::ERROR, SemaCode::TYPE_MISMATCH,
                "Type mismatch in binary expression",
                expr->get_location(),
                "Left and right operands must have the same type");

        if (!either_instance && (ltype == Fa_Type::STRING || rtype == Fa_Type::STRING)) {
            AST::Fa_BinaryOp op = bin->get_operator();
            if (op != AST::Fa_BinaryOp::OP_ADD && op != AST::Fa_BinaryOp::OP_EQ
                && op != AST::Fa_BinaryOp::OP_NEQ && op != AST::Fa_BinaryOp::OP_LT
                && op != AST::Fa_BinaryOp::OP_LTE && op != AST::Fa_BinaryOp::OP_GT
                && op != AST::Fa_BinaryOp::OP_GTE)
                report_issue(Issue::Severity::ERROR, SemaCode::INVALID_STRING_OP,
                    "Invalid operation on string", expr->get_location());
        }

        if (bin->get_operator() == AST::Fa_BinaryOp::OP_DIV
            && bin->get_right()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            auto lit = AS_CONST_LITERAL(bin->get_right());
            if (lit->is_numeric() && lit->is_number() == 0)
                report_issue(Issue::Severity::ERROR, SemaCode::DIVISION_BY_ZERO_CONST,
                    "Division by zero", expr->get_location(),
                    "This will cause a runtime error");
        }
    } break;

    case AST::Fa_Expr::Kind::UNARY:
        analyze_expr(AS_CONST_UNARY(expr)->get_operand());
        break;

    case AST::Fa_Expr::Kind::CALL: {
        auto* call = AS_CALL(expr);

        // FIX: analyze the callee exactly ONCE here.  The previous code had an
        // `else if (kind == GET) { analyze_expr(callee); }` branch that re-ran
        // analysis on GET callees that had already been handled by this line.
        analyze_expr(call->get_callee());

        for (ExprPtr arg : call->get_args())
            analyze_expr(arg);

        // Callable checks only apply to bare-name calls.  Method-call validity
        // (obj.method()) is the compiler's job at class-registry time; sema
        // has no visibility into class member tables.
        if (call->get_callee()->get_kind() == AST::Fa_Expr::Kind::NAME) {
            auto* name = AS_CONST_NAME(call->get_callee());
            Fa_SymbolTable::Symbol* sym = m_current_scope->lookup(name->get_value());
            if (sym == nullptr)
                report_issue(Issue::Severity::ERROR, SemaCode::UNDEFINED_FUNCTION,
                    "Undefined function: " + name->get_value(), expr->get_location());
            else if (sym->symbol_type != Fa_SymbolTable::SymbolType::FUNCTION
                && sym->symbol_type != Fa_SymbolTable::SymbolType::CLASS)
                report_issue(Issue::Severity::ERROR, SemaCode::NOT_CALLABLE,
                    "'" + name->get_value() + "' is not callable", expr->get_location());
        }
    } break;

    case AST::Fa_Expr::Kind::LIST:
        for (ExprPtr elem : AS_CONST_LIST(expr)->get_elements())
            analyze_expr(elem);
        break;

    case AST::Fa_Expr::Kind::INDEX: {
        auto* idx = AS_CONST_INDEX(expr);
        analyze_expr(idx->get_object());
        analyze_expr(idx->get_index());
    } break;

    case AST::Fa_Expr::Kind::GET:
        // Analyze the object expression; do NOT analyze the member name as a
        // variable reference (it would produce false "undefined variable" errors).
        analyze_expr(AS_CONST_GET_EXPR(expr)->get_object());
        break;

    case AST::Fa_Expr::Kind::DICT:
        for (auto const& [key, val] : AS_CONST_DICT(expr)->get_content()) {
            analyze_expr(key);
            analyze_expr(val);
        }
        break;

    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto* assign = AS_ASSIGNMENT_EXPR(expr);
        analyze_expr(assign->get_value());

        ExprPtr target = assign->get_target();
        if (target == nullptr)
            break;

        if (target->get_kind() == AST::Fa_Expr::Kind::INDEX) {
            analyze_expr(target);
            break;
        }

        if (target->get_kind() == AST::Fa_Expr::Kind::GET) {
            // obj.field = value — validate the receiver only.
            analyze_expr(AS_GET_EXPR(target)->get_object());
            break;
        }

        if (target->get_kind() == AST::Fa_Expr::Kind::NAME) {
            Fa_StringRef tname = AS_NAME(target)->get_value();
            bool const local_exists = m_current_scope->lookup_local(tname) != nullptr;
            bool const visible = m_current_scope->lookup(tname) != nullptr;
            bool const global = m_global_scope->lookup_local(tname) != nullptr;
            bool const can_shadow = m_current_scope != m_global_scope && visible && global;

            if (!visible && !global)
                assign->set_decl();

            if (!local_exists && (!visible || can_shadow)) {
                Fa_SymbolTable::Symbol sym;
                sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
                sym.data_type = infer_type(assign->get_value());
                sym.definition_loc = expr->get_location();
                m_current_scope->define(tname, sym);
            } else {
                m_current_scope->mark_used(tname, expr->get_location().line);
            }
        }
    } break;

    default:
        break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SemanticAnalyzer — statement analysis
// ─────────────────────────────────────────────────────────────────────────────

void Fa_SemanticAnalyzer::analyze_stmt(StmtPtr stmt)
{
    if (stmt == nullptr)
        return;

    switch (stmt->get_kind()) {

    case AST::Fa_Stmt::Kind::BLOCK: {
        // ScopeGuard creates a child scope and restores the parent on exit.
        ScopeGuard guard { m_current_scope };
        for (StmtPtr child : AS_CONST_BLOCK(stmt)->get_statements())
            analyze_stmt(child);
    } break;

    case AST::Fa_Stmt::Kind::ASSIGNMENT:
        analyze_expr(AS_ASSIGNMENT_STMT(stmt)->get_expr());
        break;

    case AST::Fa_Stmt::Kind::EXPR: {
        auto* es = AS_EXPR_STMT(stmt);
        analyze_expr(es->get_expr());
        if (es->get_expr()->get_kind() != AST::Fa_Expr::Kind::CALL
            && es->get_expr()->get_kind() != AST::Fa_Expr::Kind::ASSIGNMENT)
            report_issue(Issue::Severity::INFO, SemaCode::UNUSED_EXPR_RESULT,
                "Expression result not used", stmt->get_location());
    } break;

    case AST::Fa_Stmt::Kind::IF: {
        auto* s = AS_CONST_IF(stmt);
        analyze_expr(s->get_condition());

        if (s->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL)
            report_issue(Issue::Severity::WARNING, SemaCode::CONSTANT_CONDITION,
                "Condition is always constant", stmt->get_location(),
                "Consider removing the if statement");

        if (s->get_then())
            analyze_stmt(s->get_then());
        if (s->get_else())
            analyze_stmt(s->get_else());
    } break;

    case AST::Fa_Stmt::Kind::FOR: {
        auto* for_stmt = AS_FOR(stmt);
        analyze_expr(for_stmt->get_iter());

        // Open the loop scope, define the loop variable, then analyze the body
        // EXACTLY ONCE.  The original code analyzed the body twice (once before
        // building the header and once after), causing doubled warnings, doubled
        // mark_used entries, and leaving a dead `previous_scope` variable.
        ScopeGuard guard { m_current_scope };

        Fa_SymbolTable::Symbol loop_var;
        loop_var.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
        loop_var.data_type = Fa_Type::ANY;
        m_current_scope->define(for_stmt->get_target()->get_value(), loop_var);

        // Shadow check.  get_parent() is preferred; direct m_parent is used
        // here because the parser.hpp header still exposes it.
        // TODO: switch to get_parent() once the accessor is in the header.
        if (m_current_scope->m_parent
            && m_current_scope->m_parent->lookup_local(
                for_stmt->get_target()->get_value()))
            report_issue(Issue::Severity::WARNING, SemaCode::LOOP_VAR_SHADOW,
                "Loop variable shadows outer variable", stmt->get_location());

        // Build the loop header using the enclosing (parent) scope, then
        // analyze the body once.
        Fa_Array<AST::Fa_Stmt const*> preceding = get_preceding_statements();
        LoopHeader header = build_for_header_with_values(
            for_stmt, m_current_scope->m_parent, preceding);
        for_stmt->set_header(header);

        analyze_stmt(for_stmt->get_body());
        // guard destructor restores m_current_scope to the enclosing scope.
    } break;

    case AST::Fa_Stmt::Kind::WHILE: {
        auto* while_stmt = AS_WHILE(stmt);
        analyze_expr(while_stmt->get_condition());

        // The header builder receives the enclosing scope (or the current scope
        // if already at top level) so it can inspect variables visible to the loop.
        Fa_SymbolTable* enclosing = m_current_scope->m_parent
            ? m_current_scope->m_parent
            : m_current_scope;

        Fa_Array<AST::Fa_Stmt const*> preceding = get_preceding_statements();
        LoopHeader header = build_while_header_with_values(
            while_stmt, enclosing, preceding);
        while_stmt->set_header(header);

        if (while_stmt->get_condition()->get_kind() == AST::Fa_Expr::Kind::LITERAL) {
            auto* lit = AS_CONST_LITERAL(while_stmt->get_condition());
            if (lit->is_bool() && lit->get_bool())
                report_issue(Issue::Severity::WARNING, SemaCode::INFINITE_LOOP,
                    "Infinite loop detected", stmt->get_location(),
                    "Add a break condition");
        }

        analyze_stmt(while_stmt->get_body());
    } break;

    case AST::Fa_Stmt::Kind::FUNC: {
        auto* func_def = AS_CONST_FUNCTION_DEF(stmt);

        // Register the function name in the ENCLOSING scope before entering
        // the function's own scope so that recursive calls can resolve it.
        Fa_SymbolTable::Symbol func_sym;
        func_sym.symbol_type = Fa_SymbolTable::SymbolType::FUNCTION;
        func_sym.data_type = Fa_Type::FUNCTION;
        func_sym.definition_loc = stmt->get_location();
        m_current_scope->define(func_def->get_name()->get_value(), func_sym);

        // Open function scope; guard restores enclosing scope on exit.
        ScopeGuard guard { m_current_scope };

        for (AST::Fa_Expr const* const& param : func_def->get_parameters()) {
            Fa_SymbolTable::Symbol param_sym;
            param_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            param_sym.data_type = Fa_Type::ANY;
            m_current_scope->define(AS_CONST_NAME(param)->get_value(), param_sym);
        }

        analyze_stmt(func_def->get_body());

        if (!stmt_definitely_returns(func_def->get_body()))
            report_issue(Issue::Severity::INFO, SemaCode::MISSING_RETURN,
                "Function may not return a value", stmt->get_location());
    } break;

    case AST::Fa_Stmt::Kind::RETURN:
        analyze_expr(AS_RETURN(stmt)->get_value());
        break;

    case AST::Fa_Stmt::Kind::BREAK:
    case AST::Fa_Stmt::Kind::CONTINUE:
        break;

    case AST::Fa_Stmt::Kind::CLASS_DEF: {
        auto* class_def = AS_CONST_CLASS_DEF(stmt);

        // Register the class name in the enclosing scope.
        if (auto const* cn = dynamic_cast<AST::Fa_NameExpr const*>(class_def->get_name())) {
            Fa_SymbolTable::Symbol class_sym;
            class_sym.symbol_type = Fa_SymbolTable::SymbolType::CLASS;
            class_sym.data_type = Fa_Type::ANY;
            class_sym.definition_loc = stmt->get_location();
            m_current_scope->define(cn->get_value(), class_sym);
        }

        // Class-level scope: field names are registered here so external tooling
        // that walks the symbol table can enumerate them.  Per-instance field
        // validation still requires the compiler's class registry; sema has no
        // notion of "this NAME refers to an instance of class X".
        Fa_SymbolTable* class_scope = m_current_scope->create_child();

        for (ExprPtr member : class_def->get_members()) {
            auto const* mn = dynamic_cast<AST::Fa_NameExpr const*>(member);
            if (!mn)
                continue;

            Fa_SymbolTable::Symbol field_sym;
            field_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            field_sym.data_type = Fa_Type::ANY;
            field_sym.definition_loc = stmt->get_location();
            class_scope->define(mn->get_value(), field_sym);
        }

        for (StmtPtr method_stmt : class_def->get_methods()) {
            auto const* method = dynamic_cast<AST::Fa_FunctionDef const*>(method_stmt);
            if (!method)
                continue;

            // Method scope is a child of class_scope, not of m_current_scope.
            // Use the two-argument ScopeGuard variant to adopt this child.
            ScopeGuard guard { m_current_scope, class_scope->create_child() };

            // Tag the implicit instance as INSTANCE so infer_type's GET/INDEX
            // branches engage correctly for field accesses.
            Fa_SymbolTable::Symbol self_sym;
            self_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
            self_sym.data_type = Fa_Type::INSTANCE;
            m_current_scope->define(kClassInstanceName, self_sym);

            for (AST::Fa_Expr const* const& param : method->get_parameters()) {
                Fa_SymbolTable::Symbol param_sym;
                param_sym.symbol_type = Fa_SymbolTable::SymbolType::VARIABLE;
                param_sym.data_type = Fa_Type::ANY;
                m_current_scope->define(AS_CONST_NAME(param)->get_value(), param_sym);
            }

            analyze_stmt(method->get_body());
            // guard destructor restores m_current_scope.
        }
    } break;

    default:
        break;
    }

    m_preceding_stmts.push(stmt);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_SemanticAnalyzer — analysis entry point, reporting, and accessors
// ─────────────────────────────────────────────────────────────────────────────

void Fa_SemanticAnalyzer::analyze(Fa_Array<StmtPtr> const& stmts)
{
    for (StmtPtr stmt : stmts)
        analyze_stmt(stmt);

    // Unused-variable check at global scope only.  Variables declared inside
    // functions or blocks are intentionally not reported here; the global scope
    // is the only one powerhose lifetime spans the entire program.
    for (Fa_SymbolTable::Symbol* sym : m_global_scope->get_unused_symbols())
        report_issue(Issue::Severity::WARNING, SemaCode::UNUSED_VARIABLE,
            "Unused variable: " + sym->name, sym->definition_loc,
            "Consider removing if not needed");

    for (Issue const& issue : m_issues) {
        diagnostic::Severity sev;
        switch (issue.severity) {
        case Issue::Severity::ERROR: sev = diagnostic::Severity::ERROR; break;
        case Issue::Severity::WARNING: sev = diagnostic::Severity::WARNING; break;
        default: sev = diagnostic::Severity::NOTE; break;
        }
        diagnostic::report(sev, issue.loc, issue.code);
        if (!issue.suggestion.empty())
            diagnostic::engine.add_suggestion(
                std::string(issue.suggestion.data(), issue.suggestion.len()));
    }
}

Fa_Array<typename Fa_SemanticAnalyzer::Issue> const&
Fa_SemanticAnalyzer::get_issues() const { return m_issues; }

Fa_SymbolTable const* Fa_SemanticAnalyzer::get_global_scope() const { return m_global_scope; }
Fa_SymbolTable const* Fa_SemanticAnalyzer::get_current_scope() const { return m_current_scope; }

void Fa_SemanticAnalyzer::print_report() const
{
    if (m_issues.empty()) {
        std::cout << "✓ No issues found\n";
        return;
    }

    std::cout << "Found " << m_issues.size() << " issue(s):\n\n";
    for (Issue const& issue : m_issues) {
        Fa_StringRef sev_str;
        switch (issue.severity) {
        case Issue::Severity::ERROR: sev_str = "ERROR"; break;
        case Issue::Severity::WARNING: sev_str = "WARNING"; break;
        default: sev_str = "INFO"; break;
        }
        std::cout << "[" << sev_str << "] Line " << issue.loc.line
                  << ": " << issue.message << "\n";
        if (!issue.suggestion.empty())
            std::cout << "  → " << issue.suggestion << "\n";
        std::cout << "\n";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Token → AST op converters
// ─────────────────────────────────────────────────────────────────────────────
// NOTE: These are logically a property of the token type and would be better
// placed as methods on Fa_Token or in a token_ops.hpp utility header.
// Kept here to avoid header changes in this refactoring pass.

AST::Fa_BinaryOp to_binary_op(TokType const op)
{
    switch (op) {
    case TokType::OP_PLUS: return AST::Fa_BinaryOp::OP_ADD;
    case TokType::OP_MINUS: return AST::Fa_BinaryOp::OP_SUB;
    case TokType::OP_STAR: return AST::Fa_BinaryOp::OP_MUL;
    case TokType::OP_SLASH: return AST::Fa_BinaryOp::OP_DIV;
    case TokType::OP_PERCENT: return AST::Fa_BinaryOp::OP_MOD;
    case TokType::OP_POWER: return AST::Fa_BinaryOp::OP_POW;
    case TokType::OP_EQ: return AST::Fa_BinaryOp::OP_EQ;
    case TokType::OP_NEQ: return AST::Fa_BinaryOp::OP_NEQ;
    case TokType::OP_LT: return AST::Fa_BinaryOp::OP_LT;
    case TokType::OP_GT: return AST::Fa_BinaryOp::OP_GT;
    case TokType::OP_LTE: return AST::Fa_BinaryOp::OP_LTE;
    case TokType::OP_GTE: return AST::Fa_BinaryOp::OP_GTE;
    case TokType::OP_BITAND: return AST::Fa_BinaryOp::OP_BITAND;
    case TokType::OP_BITOR: return AST::Fa_BinaryOp::OP_BITOR;
    case TokType::OP_BITXOR: return AST::Fa_BinaryOp::OP_BITXOR;
    case TokType::OP_LSHIFT: return AST::Fa_BinaryOp::OP_LSHIFT;
    case TokType::OP_RSHIFT: return AST::Fa_BinaryOp::OP_RSHIFT;
    case TokType::OP_AND: return AST::Fa_BinaryOp::OP_AND;
    case TokType::OP_OR: return AST::Fa_BinaryOp::OP_OR;
    default: return AST::Fa_BinaryOp::INVALID;
    }
}

AST::Fa_UnaryOp to_unary_op(TokType const op)
{
    switch (op) {
    case TokType::OP_PLUS: return AST::Fa_UnaryOp::OP_PLUS;
    case TokType::OP_MINUS: return AST::Fa_UnaryOp::OP_NEG;
    case TokType::OP_BITNOT: return AST::Fa_UnaryOp::OP_BITNOT;
    case TokType::OP_NOT: return AST::Fa_UnaryOp::OP_NOT;
    default: return AST::Fa_UnaryOp::INVALID;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_Parser — utilities
// ─────────────────────────────────────────────────────────────────────────────

Fa_Error Fa_Parser::report_error(ParserCode err_code)
{
    auto* tok = current_token();
    Fa_SourceLocation loc = {
        tok->line(), tok->column(),
        static_cast<u16>(tok->lexeme().len())
    };
    return fairuz::report_error(err_code, loc, &m_lexer);
}

bool Fa_Parser::we_done() const { return current_token()->is(TokType::ENDMARKER); }

bool Fa_Parser::check(TokType t) const { return current_token()->is(t); }

TokenPtr Fa_Parser::current_token() const { return m_lexer.current(); }

bool Fa_Parser::match(TokType const type)
{
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

void Fa_Parser::synchronize()
{
    while (!we_done()) {
        if (check(TokType::NEWLINE) || check(TokType::DEDENT)) {
            advance();
            return;
        }
        if (check(TokType::KW_IF) || check(TokType::KW_WHILE)
            || check(TokType::KW_FOR) || check(TokType::KW_RETURN)
            || check(TokType::KW_BREAK) || check(TokType::KW_CONTINUE)
            || check(TokType::KW_FN))
            return;
        advance();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_Parser — top-level
// ─────────────────────────────────────────────────────────────────────────────

Fa_Array<StmtPtr> Fa_Parser::parse_program()
{
    Fa_Array<StmtPtr> stmts;

    while (!we_done()) {
        skip_newlines();
        if (we_done())
            break;

        auto stmt = parse_statement();
        if (stmt.has_value()) {
            stmts.push(stmt.value());
        } else {
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (we_done())
                break;
        }
    }

    m_sema.analyze(stmts);

    if (diagnostic::has_errors())
        diagnostic::dump();

    return stmts;
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_statement()
{
    skip_newlines();

    if (check(TokType::KW_IF))
        return parse_if_stmt();
    if (check(TokType::KW_WHILE))
        return parse_while_stmt();
    if (check(TokType::KW_FOR))
        return parse_for_stmt();
    if (check(TokType::KW_RETURN))
        return parse_return_stmt();
    if (check(TokType::KW_BREAK))
        return parse_break_stmt();
    if (check(TokType::KW_CONTINUE))
        return parse_continue_stmt();
    if (check(TokType::KW_FN))
        return parse_function_def();
    if (check(TokType::KW_CLASS))
        return parse_class_def();

    return parse_expression_stmt();
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_Parser — statement parsers
// ─────────────────────────────────────────────────────────────────────────────

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_return_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_RETURN, ParserCode::EXPECTED_RETURN);

    if (check(TokType::NEWLINE) || we_done())
        return AST::Fa_make_return(start->location());

    Fa_TRY(ret, parse_expression());
    return AST::Fa_make_return(start->location(), ret);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_break_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::Fa_make_break(start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_continue_stmt()
{
    TokenPtr start = current_token();
    advance();
    return AST::Fa_make_continue(start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_while_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_WHILE, ParserCode::EXPECTED_WHILE_KEYWORD);

    Fa_TRY(condition, parse_expression());
    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_WHILE);

    auto while_block = parse_indented_block();
    Fa_VERIFY_NODE(while_block);

    return Fa_make_while(condition, AS_BLOCK(while_block.value()), start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_for_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_FOR, ParserCode::UNEXPECTED_TOKEN);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_FOR_TARGET);

    auto* target = AST::Fa_make_name(current_token()->lexeme(), current_token()->location());
    advance();

    // Accept both the lexer keyword KW_IN and the Arabic identifier "في".
    bool saw_in = match(TokType::KW_IN);
    if (!saw_in && check(TokType::IDENTIFIER) && current_token()->lexeme() == "في") {
        advance();
        saw_in = true;
    }
    if (!saw_in)
        return report_error(ParserCode::EXPECTED_IN_KEYWORD);

    Fa_TRY(iter, parse_expression());
    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FOR);

    auto body = parse_indented_block();
    Fa_VERIFY_NODE(body);

    return AST::Fa_make_for(target, iter, body.value(), start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_if_stmt()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_IF, ParserCode::EXPECTED_IF_KEYWORD);

    Fa_TRY(condition, parse_expression());
    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_IF);

    auto then_block = parse_indented_block();
    Fa_VERIFY_NODE(then_block);

    StmtPtr else_block = nullptr;
    skip_newlines();

    if (match(TokType::KW_ELSE)) {
        skip_newlines();
        if (check(TokType::KW_IF)) {
            // else-if: no colon between `else` and `if`.
            auto nested = parse_if_stmt();
            Fa_VERIFY_NODE(nested);
            else_block = nested.value();
        } else {
            Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_IF);
            auto else_stmt = parse_indented_block();
            Fa_VERIFY_NODE(else_stmt);
            else_block = else_stmt.value();
        }
    }

    return Fa_make_if(condition, AS_BLOCK(then_block.value()), start->location(), else_block);
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_expression_stmt()
{
    Fa_TRY(expr, parse_expression());
    return Fa_make_expr_stmt(expr, expr->get_location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_indented_block()
{
    TokenPtr start = current_token();
    match(TokType::NEWLINE);
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<StmtPtr> stmts;

    if (match(TokType::DEDENT))
        return Fa_make_block(stmts, start->location());

    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        auto stmt = parse_statement();
        if (stmt.has_value()) {
            stmts.push(stmt.value());
        } else {
            synchronize();
            if (check(TokType::DEDENT) || we_done())
                break;
        }
    }

    if (check(TokType::ENDMARKER))
        return Fa_make_block(stmts, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);
    return Fa_make_block(stmts, start->location());
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_Parser — function and class parsers
// ─────────────────────────────────────────────────────────────────────────────

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_function_def()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_FN, ParserCode::EXPECTED_FN_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::EXPECTED_FN_NAME);
    TokenPtr name_tok = current_token();
    advance();

    auto params = parse_parameters_list();
    Fa_VERIFY_NODE(params);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FN);

    auto body = parse_indented_block();
    Fa_VERIFY_NODE(body);

    return Fa_make_function(
        AST::Fa_make_name(name_tok->lexeme(), name_tok->location()),
        AS_LIST(params.value()),
        AS_BLOCK(body.value()),
        start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_class_def()
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_CLASS, ParserCode::EXPECTED_CLASS_KEYWORD);

    if (!check(TokType::IDENTIFIER))
        return report_error(ParserCode::UNEXPECTED_TOKEN); // replace with EXPECTED_CLASS_NAME if added to ParserCode

    TokenPtr name_tok = current_token();
    advance();
    ExprPtr class_name = AST::Fa_make_name(name_tok->lexeme(), name_tok->location());

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_CLASS);
    skip_newlines();
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<ExprPtr> members = Fa_Array<ExprPtr>::with_capacity(4);
    Fa_Array<StmtPtr> methods = Fa_Array<StmtPtr>::with_capacity(4);

    while (!check(TokType::DEDENT) && !we_done()) {
        auto method = parse_class_method(members);
        if (method.has_value()) {
            methods.push(method.value());
        } else {
            if (diagnostic::is_saturated())
                break;
            synchronize();
            if (check(TokType::DEDENT) || we_done())
                break;
        }
    }

    if (check(TokType::ENDMARKER))
        return AST::Fa_make_class_def(class_name, members, methods, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);
    return AST::Fa_make_class_def(class_name, members, methods, start->location());
}

Fa_ErrorOr<StmtPtr> Fa_Parser::parse_class_method(Fa_Array<ExprPtr>& members)
{
    TokenPtr start = current_token();
    Fa_VERIFY_TOKEN(TokType::KW_FN, ParserCode::EXPECTED_FN_KEYWORD);

    TokenPtr name_tok = current_token();
    Fa_VERIFY_TOKEN(TokType::IDENTIFIER, ParserCode::EXPECTED_FN_NAME);

    auto params = parse_parameters_list();
    Fa_VERIFY_NODE(params);

    Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_FN);
    skip_newlines();
    Fa_VERIFY_TOKEN(TokType::INDENT, ParserCode::EXPECTED_INDENT);

    Fa_Array<StmtPtr> stmts;

    while (!check(TokType::DEDENT) && !we_done()) {
        skip_newlines();
        if (check(TokType::DEDENT))
            break;

        if (match(TokType::DOT)) {
            // `.field = expr` member-initializer syntax inside a method body.
            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::INVALID_ASSIGN_TARGET);

            TokenPtr member_tok = current_token();
            Fa_StringRef mname = member_tok->lexeme();
            advance();

            // Desugar `.field` to a GET expression (instance.field), not an
            // INDEX expression with a string key.  The compiler's fas
            // field-access path (compile_get_i / SET_FIELD) specifically looks
            // for Fa_GetExpr with a NAME member; an index form would silently
            // fall back to the slow dict-style path for every field access.
            ExprPtr target = AST::Fa_make_get_expr(
                AST::Fa_make_name(kClassInstanceName, member_tok->location()),
                AST::Fa_make_name(mname, member_tok->location()),
                member_tok->location());

            AST::Fa_AssignmentExpr* member_assign = nullptr;

            if (check(TokType::OP_ASSIGN)) {
                advance();
                Fa_TRY(rhs, parse_assignment_expr());
                member_assign = AST::Fa_make_assignment_expr(
                    target, rhs, member_tok->location(), /*decl=*/false);
            } else if (is_augmented_assign_tok(current_token())) {
                TokenPtr op_tok = current_token();
                advance();
                Fa_TRY(rhs, parse_assignment_expr());
                AST::Fa_BinaryOp op = augmented_assign_to_binary_op(op_tok->type());
                // target->clone() reads the current field value (GET read);
                // `target` itself is the write target.
                auto* bin = AST::Fa_make_binary(
                    target->clone(), rhs, op, target->get_location());
                member_assign = AST::Fa_make_assignment_expr(
                    target, bin, member_tok->location(), /*decl=*/false);
            } else {
                return report_error(ParserCode::INVALID_ASSIGN_TARGET);
            }

            members.push(AST::Fa_make_name(mname, member_tok->location()));
            stmts.push(AST::Fa_make_expr_stmt(member_assign, member_tok->location()));
            continue;
        }

        // Regular statement inside the method body.
        Fa_TRY(s, parse_statement());
        stmts.push(s);
    }

    AST::Fa_BlockStmt* block = AST::Fa_make_block(
        stmts,
        stmts.empty() ? start->location() : stmts[0]->get_location());

    if (check(TokType::ENDMARKER))
        return AST::Fa_make_function(
            AST::Fa_make_name(name_tok->lexeme(), name_tok->location()),
            AS_LIST(params.value()), block, start->location());

    Fa_VERIFY_TOKEN(TokType::DEDENT, ParserCode::EXPECTED_DEDENT);
    return AST::Fa_make_function(
        AST::Fa_make_name(name_tok->lexeme(), name_tok->location()),
        AS_LIST(params.value()), block, start->location());
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_parameters_list()
{
    TokenPtr open = current_token();
    Fa_VERIFY_TOKEN(TokType::LPAREN, ParserCode::EXPECTED_LPAREN);

    Fa_Array<ExprPtr> params = Fa_Array<ExprPtr>::with_capacity(4);

    if (!check(TokType::RPAREN)) {
        do {
            skip_newlines();
            if (check(TokType::RPAREN))
                break;

            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::EXPECTED_PARAM_NAME);

            TokenPtr param_tok = current_token();
            advance();
            params.push(AST::Fa_make_name(param_tok->lexeme(), param_tok->location()));
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RPAREN));
    }

    Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);

    // For an empty parameter list, use the '(' location (not the token after ')').
    Fa_SourceLocation loc = params.empty() ? open->location() : params[0]->get_location();

    return Fa_make_list(params, loc);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fa_Parser — expression parsers
// ─────────────────────────────────────────────────────────────────────────────

Fa_ErrorOr<ExprPtr> Fa_Parser::parse() { return parse_expression(); }

// parse_expression is the public entry point; it delegates to parse_assignment_expr.
Fa_ErrorOr<ExprPtr> Fa_Parser::parse_expression() { return parse_assignment_expr(); }

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_assignment_expr()
{
    // LHS goes through the full expression hierarchy (via parse_conditional_expr
    // → parse_binary_expr_precedence).  The Pratt parser stops at '=' and
    // augmented-assignment tokens, leaving them for this function to handle.
    Fa_TRY(lhs, parse_conditional_expr());

    if (check(TokType::OP_ASSIGN) || is_augmented_assign_tok(current_token())) {
        ExprPtr target = lhs;
        AST::Fa_Expr::Kind kind = target->get_kind();

        if (kind != AST::Fa_Expr::Kind::NAME && kind != AST::Fa_Expr::Kind::INDEX && kind != AST::Fa_Expr::Kind::GET)
            return report_error(ParserCode::INVALID_ASSIGN_TARGET);

        if (is_augmented_assign_tok(current_token())) {
            TokenPtr op_tok = current_token();
            advance();
            Fa_TRY(rhs, parse_assignment_expr());
            AST::Fa_BinaryOp op = augmented_assign_to_binary_op(op_tok->type());
            auto* bin = AST::Fa_make_binary(lhs->clone(), rhs, op, lhs->get_location());
            return Fa_make_assignment_expr(target, bin, target->get_location(), /*decl=*/false);
        }

        advance(); // consume '='
        Fa_TRY(rhs, parse_assignment_expr());
        return Fa_make_assignment_expr(target, rhs, target->get_location(), /*decl=*/false);
    }

    return lhs;
}

// ─────────────────────────────────────────────────────────────────────────────
// Unified Pratt parser
// ─────────────────────────────────────────────────────────────────────────────
//
// This function replaces the previous four-function cascade:
//   parse_logical_expr_precedence → parse_comparison_expr
//                                 → parse_binary_expr_precedence → parse_unary_expr
//
// The old design had two critical defects:
//
//   (a) Left-associativity was broken.  Both Pratt parsers used `return` inside
//       their loops, so `5 - 3 - 1` produced `5 - (3 - 1) = 3` instead of
//       `(5 - 3) - 1 = 1`.  The bug was masked for some cases by the two-level
//       cascade accidentally picking up the remaining operator at the logical
//       level, but the fix was fragile and implicit.
//
//   (b) The separation into "logical" and "arithmetic" levels was meaningless
//       because both parsers accepted all binary operators; the precedence
//       values from get_precedence() already encode the correct hierarchy.
//
// Fix: accumulate with `lhs = make_binary(lhs, rhs, op)` and continue the loop
// (left-associative for all operators except OP_POWER which is right-associative).

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_binary_expr_precedence(unsigned int min_prec)
{
    Fa_TRY(lhs, parse_unary_expr());

    for (;;) {
        TokenPtr cur = current_token();
        // Stop at non-binary-ops and at plain assignment (handled by parse_assignment_expr).
        if (!cur->is_binary_op() || cur->is(TokType::OP_ASSIGN))
            break;

        unsigned int prec = cur->get_precedence();
        if (prec == tok::PREC_NONE || prec < min_prec)
            break;

        TokType op_type = cur->type();
        advance();

        // OP_POWER is right-associative: pass `prec` (not `prec+1`) so the
        // recursive call accepts another power op of the same precedence.
        // All other operators are left-associative: pass `prec+1`.
        unsigned int next_min = (op_type == TokType::OP_POWER) ? prec : prec + 1;
        Fa_TRY(rhs, parse_binary_expr_precedence(next_min));

        // FIX: assign to lhs and CONTINUE the loop — do not return here.
        // Returning inside the loop was the root cause of the left-associativity bug.
        lhs = Fa_make_binary(lhs, rhs, to_binary_op(op_type), lhs->get_location());
    }

    return lhs;
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_unary_expr()
{
    TokenPtr op_tok = current_token();
    if (op_tok->is_unary_op()) {
        TokType op = op_tok->type();
        advance();
        Fa_TRY(operand, parse_unary_expr());
        // FIX: use the operator token's location (op_tok), not the operand's.
        // `!a` should report the location at `!`, not at `a`.
        return Fa_make_unary(operand, to_unary_op(op), op_tok->location());
    }
    return parse_postfix_expr();
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_postfix_expr()
{
    Fa_TRY(base, parse_primary_expr());
    ExprPtr expr = base;

    for (;;) {
        // Function call: expr(args...)
        if (check(TokType::LPAREN)) {
            advance();
            Fa_Array<ExprPtr> args = Fa_Array<ExprPtr>::with_capacity(4);

            if (!check(TokType::RPAREN)) {
                do {
                    skip_newlines();
                    if (check(TokType::RPAREN))
                        break;
                    Fa_TRY(arg, parse_expression());
                    args.push(arg);
                    skip_newlines();
                } while (match(TokType::COMMA) && !check(TokType::RPAREN));
            }

            Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);
            Fa_SourceLocation loc = (!args.empty() && args[0])
                ? args[0]->get_location()
                : Fa_SourceLocation { };
            expr = Fa_make_call(
                expr, Fa_make_list(std::move(args), loc),
                expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        // Subscript: expr[index]
        if (match(TokType::LBRACKET)) {
            Fa_TRY(index, parse_expression());
            Fa_VERIFY_TOKEN(TokType::RBRACKET, ParserCode::EXPECTED_RBRACKET);
            expr = Fa_make_index(
                expr, index,
                expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        // Member access: expr.member
        // FIX: parse only a plain IDENTIFIER after '.', not a full expression.
        // The old code called parse_expression(), so `obj.a + b` was silently
        // parsed as GET(obj, BinaryExpr(a,+,b)) instead of being a syntax error.
        if (match(TokType::DOT)) {
            if (!check(TokType::IDENTIFIER))
                return report_error(ParserCode::UNEXPECTED_TOKEN); // replace with EXPECTED_MEMBER_NAME if added
            TokenPtr member_tok = current_token();
            advance();
            expr = Fa_make_get_expr(
                expr,
                AST::Fa_make_name(member_tok->lexeme(), member_tok->location()),
                expr ? expr->get_location() : Fa_SourceLocation { });
            continue;
        }

        break;
    }

    return expr;
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_primary_expr()
{
    TokenPtr cur = current_token();

    // Numeric literals
    if (cur->is_numeric()) {
        advance();
        TokType tt = cur->type();

        if (tt == TokType::DECIMAL)
            return AST::Fa_make_literal_float(cur->lexeme().to_double(), cur->location());

        // FIX: determine the numeric base via a switch statement, not by
        // indexing into an array with enum arithmetic.  The old code assumed
        // BINARY/OCTAL/INTEGER/HEX were contiguous in the enum; this makes no
        // such assumption.
        int base = 10;
        switch (tt) {
        case TokType::BINARY: base = 2; break;
        case TokType::OCTAL: base = 8; break;
        case TokType::INTEGER: base = 10; break;
        case TokType::HEX: base = 16; break;
        default: break;
        }
        return AST::Fa_make_literal_int(
            util::parse_integer_literal(cur->lexeme(), base),
            cur->location());
    }

    if (match(TokType::STRING))
        return AST::Fa_make_literal_string(cur->lexeme(), cur->location());

    // FIX: use the token type to distinguish true/false, not a lexeme string
    // comparison against the Arabic spelling "صحيح".  The lexer already
    // encodes this distinction in KW_TRUE vs KW_FALSE.
    if (check(TokType::KW_TRUE) || check(TokType::KW_FALSE)) {
        bool val = cur->is(TokType::KW_TRUE);
        advance();
        return AST::Fa_make_literal_bool(val, cur->location());
    }

    if (match(TokType::KW_NIL))
        return AST::Fa_make_literal_nil(cur->location());

    if (match(TokType::IDENTIFIER))
        return AST::Fa_make_name(cur->lexeme(), cur->location());

    if (match(TokType::LPAREN)) {
        if (match(TokType::RPAREN))
            return Fa_make_list(Fa_Array<ExprPtr> { }, cur->location());
        Fa_TRY(inner, parse_expression());
        Fa_VERIFY_TOKEN(TokType::RPAREN, ParserCode::EXPECTED_RPAREN_EXPR);
        return inner;
    }

    if (match(TokType::LBRACKET))
        return parse_list_literal();
    if (match(TokType::LBRACE))
        return parse_dict_literal();

    if (we_done())
        return report_error(ParserCode::UNEXPECTED_EOF);

    skip_newlines();
    return report_error(ParserCode::UNEXPECTED_TOKEN);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_list_literal()
{
    TokenPtr start = current_token();
    Fa_Array<ExprPtr> elements = Fa_Array<ExprPtr>::with_capacity(4);

    if (!check(TokType::RBRACKET)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACKET))
                break;
            Fa_TRY(elem, parse_expression());
            elements.push(elem);
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACKET));
    }

    Fa_VERIFY_TOKEN(TokType::RBRACKET, ParserCode::EXPECTED_RBRACKET);

    Fa_SourceLocation loc = elements.empty()
        ? start->location()
        : elements[0]->get_location();
    return Fa_make_list(std::move(elements), loc);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_dict_literal()
{
    TokenPtr start = current_token();
    Fa_Array<std::pair<ExprPtr, ExprPtr>> content;

    if (!check(TokType::RBRACE)) {
        do {
            skip_newlines();
            if (check(TokType::RBRACE))
                break;
            Fa_TRY(key, parse_expression());
            Fa_VERIFY_TOKEN(TokType::COLON, ParserCode::EXPECTED_COLON_DICT);
            Fa_TRY(val, parse_expression());
            content.push({ key, val });
            skip_newlines();
        } while (match(TokType::COMMA) && !check(TokType::RBRACE));
    }

    Fa_VERIFY_TOKEN(TokType::RBRACE, ParserCode::EXPECTED_RBRACE_EXPR);

    Fa_SourceLocation loc = content.empty()
        ? start->location()
        : content[0].first->get_location();
    return AST::Fa_make_dict(std::move(content), loc);
}

// ─────────────────────────────────────────────────────────────────────────────
// Compatibility stubs
// ─────────────────────────────────────────────────────────────────────────────
//
// These thin wrappers exist only so that existing parser.hpp declarations
// remain valid while the header is being cleaned up.  They add no logic;
// all behaviour now lives in parse_binary_expr_precedence.
//
// TODO: remove these declarations from parser.hpp, then delete the stubs.

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_conditional_expr()
{
    // Entry point for the unified Pratt parser.  Ternary operator will be
    // inserted here once implemented (see the existing TODO in the design).
    return parse_binary_expr_precedence(0);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_logical_expr()
{
    return parse_binary_expr_precedence(0);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_logical_expr_precedence(unsigned int p)
{
    return parse_binary_expr_precedence(p);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_comparison_expr()
{
    return parse_binary_expr_precedence(0);
}

Fa_ErrorOr<ExprPtr> Fa_Parser::parse_binary_expr()
{
    return parse_binary_expr_precedence(0);
}

} // namespace fairuz::parser
