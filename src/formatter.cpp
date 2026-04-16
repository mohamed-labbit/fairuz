#include "../include/formatter.hpp"

#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace fairuz {

namespace {

constexpr char kClassInstanceName[] = "__class$instance";

enum Precedence : int {
    kPrecAssignment = 1,
    kPrecLogicalOr = 2,
    kPrecLogicalAnd = 3,
    kPrecBitOr = 4,
    kPrecBitXor = 5,
    kPrecBitAnd = 6,
    kPrecComparison = 7,
    kPrecShift = 8,
    kPrecAdditive = 9,
    kPrecMultiplicative = 10,
    kPrecPower = 11,
    kPrecUnary = 12,
    kPrecPostfix = 13,
    kPrecAtom = 14,
};

Fa_StringRef binary_op_string(AST::Fa_BinaryOp op)
{
    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD:
        return "+";
    case AST::Fa_BinaryOp::OP_SUB:
        return "-";
    case AST::Fa_BinaryOp::OP_MUL:
        return "*";
    case AST::Fa_BinaryOp::OP_DIV:
        return "/";
    case AST::Fa_BinaryOp::OP_MOD:
        return "%";
    case AST::Fa_BinaryOp::OP_POW:
        return "**";
    case AST::Fa_BinaryOp::OP_EQ:
        return "=";
    case AST::Fa_BinaryOp::OP_NEQ:
        return "!=";
    case AST::Fa_BinaryOp::OP_LT:
        return "<";
    case AST::Fa_BinaryOp::OP_GT:
        return ">";
    case AST::Fa_BinaryOp::OP_LTE:
        return "<=";
    case AST::Fa_BinaryOp::OP_GTE:
        return ">=";
    case AST::Fa_BinaryOp::OP_BITAND:
        return "&";
    case AST::Fa_BinaryOp::OP_BITOR:
        return "|";
    case AST::Fa_BinaryOp::OP_BITXOR:
        return "^";
    case AST::Fa_BinaryOp::OP_LSHIFT:
        return "<<";
    case AST::Fa_BinaryOp::OP_RSHIFT:
        return ">>";
    case AST::Fa_BinaryOp::OP_AND:
        return "و";
    case AST::Fa_BinaryOp::OP_OR:
        return "او";
    default:
        return "";
    }
}

Fa_StringRef unary_op_string(AST::Fa_UnaryOp op)
{
    switch (op) {
    case AST::Fa_UnaryOp::OP_PLUS:
        return "+";
    case AST::Fa_UnaryOp::OP_NEG:
        return "-";
    case AST::Fa_UnaryOp::OP_BITNOT:
        return "~";
    case AST::Fa_UnaryOp::OP_NOT:
        return "ليس";
    default:
        return "";
    }
}

std::string format_float_literal(double value)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;

    std::string text = out.str();
    if (text.find('.') == std::string::npos
        && text.find('e') == std::string::npos
        && text.find('E') == std::string::npos) {
        text += ".0";
        return text;
    }

    if (text.find('e') == std::string::npos && text.find('E') == std::string::npos) {
        while (text.size() > 2 && text.back() == '0' && text[text.size() - 2] != '.')
            text.pop_back();
    }

    return text;
}

std::string escape_string_literal(Fa_StringRef value)
{
    std::string out;
    out.reserve(value.len() + 2);
    out.push_back('"');

    for (size_t i = 0; i < value.len(); i += 1) {
        char ch = value[i];
        switch (ch) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out.push_back(ch);
            break;
        }
    }

    out.push_back('"');
    return out;
}

} // namespace

void Fa_Formatter::write(Fa_StringRef const& text)
{
    if (text.empty())
        return;

    if (m_line_start) {
        if (m_indent_level > 0)
            m_formatted += Fa_StringRef(static_cast<size_t>(m_indent_level * m_indent_sz), ' ');
        m_line_start = false;
    }

    m_formatted += text;
}

void Fa_Formatter::write(char ch)
{
    if (m_line_start) {
        if (m_indent_level > 0)
            m_formatted += Fa_StringRef(static_cast<size_t>(m_indent_level * m_indent_sz), ' ');
        m_line_start = false;
    }

    m_formatted += ch;
}

void Fa_Formatter::write_newline()
{
    m_formatted += '\n';
    m_line_start = true;
}

Fa_StringRef Fa_Formatter::format(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    m_formatted.clear();
    m_indent_level = 0;
    m_line_start = true;

    for (u32 i = 0; i < stmts.size(); i += 1) {
        if (stmts[i] == nullptr)
            continue;
        format_statement(stmts[i]);
        write_newline();
    }

    return m_formatted;
}

bool Fa_Formatter::is_class_member_target(AST::Fa_Expr const* expr) const
{
    if (expr == nullptr || expr->get_kind() != AST::Fa_Expr::Kind::INDEX)
        return false;

    auto const* index_expr = static_cast<AST::Fa_IndexExpr const*>(expr);
    if (index_expr->get_object() == nullptr || index_expr->get_index() == nullptr)
        return false;
    if (index_expr->get_object()->get_kind() != AST::Fa_Expr::Kind::NAME)
        return false;
    if (index_expr->get_index()->get_kind() != AST::Fa_Expr::Kind::LITERAL)
        return false;

    auto const* object_expr = static_cast<AST::Fa_NameExpr const*>(index_expr->get_object());
    auto const* member_expr = static_cast<AST::Fa_LiteralExpr const*>(index_expr->get_index());

    return object_expr->get_value() == kClassInstanceName && member_expr->is_string();
}

int Fa_Formatter::precedence(AST::Fa_Expr const* expr) const
{
    if (expr == nullptr)
        return kPrecAtom;

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::ASSIGNMENT:
        return kPrecAssignment;
    case AST::Fa_Expr::Kind::UNARY:
        return kPrecUnary;
    case AST::Fa_Expr::Kind::CALL:
    case AST::Fa_Expr::Kind::INDEX:
        return kPrecPostfix;
    case AST::Fa_Expr::Kind::LITERAL:
    case AST::Fa_Expr::Kind::NAME:
    case AST::Fa_Expr::Kind::LIST:
    case AST::Fa_Expr::Kind::DICT:
        return kPrecAtom;
    case AST::Fa_Expr::Kind::BINARY: {
        auto const* bin_expr = static_cast<AST::Fa_BinaryExpr const*>(expr);
        switch (bin_expr->get_operator()) {
        case AST::Fa_BinaryOp::OP_OR:
            return kPrecLogicalOr;
        case AST::Fa_BinaryOp::OP_AND:
            return kPrecLogicalAnd;
        case AST::Fa_BinaryOp::OP_BITOR:
            return kPrecBitOr;
        case AST::Fa_BinaryOp::OP_BITXOR:
            return kPrecBitXor;
        case AST::Fa_BinaryOp::OP_BITAND:
            return kPrecBitAnd;
        case AST::Fa_BinaryOp::OP_EQ:
        case AST::Fa_BinaryOp::OP_NEQ:
        case AST::Fa_BinaryOp::OP_LT:
        case AST::Fa_BinaryOp::OP_GT:
        case AST::Fa_BinaryOp::OP_LTE:
        case AST::Fa_BinaryOp::OP_GTE:
            return kPrecComparison;
        case AST::Fa_BinaryOp::OP_LSHIFT:
        case AST::Fa_BinaryOp::OP_RSHIFT:
            return kPrecShift;
        case AST::Fa_BinaryOp::OP_ADD:
        case AST::Fa_BinaryOp::OP_SUB:
            return kPrecAdditive;
        case AST::Fa_BinaryOp::OP_MUL:
        case AST::Fa_BinaryOp::OP_DIV:
        case AST::Fa_BinaryOp::OP_MOD:
            return kPrecMultiplicative;
        case AST::Fa_BinaryOp::OP_POW:
            return kPrecPower;
        default:
            return kPrecAtom;
        }
    }
    default:
        return kPrecAtom;
    }
}

void Fa_Formatter::format_comma_separated(Fa_Array<AST::Fa_Expr*> const& exprs)
{
    for (u32 i = 0; i < exprs.size(); i += 1) {
        if (i != 0)
            write("، ");
        format_expression(exprs[i]);
    }
}

void Fa_Formatter::format_assignment_target(AST::Fa_Expr const* expr)
{
    if (is_class_member_target(expr)) {
        auto const* index_expr = static_cast<AST::Fa_IndexExpr const*>(expr);
        auto const* member_expr = static_cast<AST::Fa_LiteralExpr const*>(index_expr->get_index());
        write(".");
        write(member_expr->get_str());
        return;
    }

    format_expression(expr, kPrecAssignment, false);
}

void Fa_Formatter::format_expression(AST::Fa_Expr const* expr, int parent_precedence, bool parenthesize_on_equal)
{
    if (expr == nullptr)
        return;

    int const current_precedence = precedence(expr);
    bool const needs_parens = parent_precedence >= 0
        && (current_precedence < parent_precedence
            || (parenthesize_on_equal && current_precedence == parent_precedence));

    if (needs_parens)
        write('(');

    switch (expr->get_kind()) {
    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto const* assign_expr = static_cast<AST::Fa_AssignmentExpr const*>(expr);
        format_assignment_target(assign_expr->get_target());
        write(" := ");
        format_expression(assign_expr->get_value(), kPrecAssignment, false);
        break;
    }
    case AST::Fa_Expr::Kind::BINARY: {
        auto const* bin_expr = static_cast<AST::Fa_BinaryExpr const*>(expr);
        format_expression(bin_expr->get_left(), current_precedence, false);
        write(" ");
        write(binary_op_string(bin_expr->get_operator()));
        write(" ");
        format_expression(bin_expr->get_right(), current_precedence, true);
        break;
    }
    case AST::Fa_Expr::Kind::CALL: {
        auto const* call_expr = static_cast<AST::Fa_CallExpr const*>(expr);
        format_expression(call_expr->get_callee(), current_precedence, false);
        write('(');
        format_comma_separated(call_expr->get_args());
        write(')');
        break;
    }
    case AST::Fa_Expr::Kind::DICT: {
        auto const* dict_expr = static_cast<AST::Fa_DictExpr const*>(expr);
        write('{');
        bool first = true;
        for (auto const& entry : dict_expr->get_content()) {
            if (!first)
                write("، ");
            format_expression(entry.first);
            write(": ");
            format_expression(entry.second);
            first = false;
        }
        write('}');
        break;
    }
    case AST::Fa_Expr::Kind::INDEX: {
        auto const* index_expr = static_cast<AST::Fa_IndexExpr const*>(expr);
        format_expression(index_expr->get_object(), current_precedence, false);
        write('[');
        format_expression(index_expr->get_index());
        write(']');
        break;
    }
    case AST::Fa_Expr::Kind::LIST: {
        auto const* list_expr = static_cast<AST::Fa_ListExpr const*>(expr);
        write('[');
        format_comma_separated(list_expr->get_elements());
        write(']');
        break;
    }
    case AST::Fa_Expr::Kind::LITERAL: {
        auto const* literal_expr = static_cast<AST::Fa_LiteralExpr const*>(expr);
        switch (literal_expr->get_type()) {
        case AST::Fa_LiteralExpr::Type::BOOLEAN:
            write(literal_expr->get_bool() ? "صحيح" : "خطا");
            break;
        case AST::Fa_LiteralExpr::Type::FLOAT:
            write(Fa_StringRef(format_float_literal(literal_expr->get_float()).c_str()));
            break;
        case AST::Fa_LiteralExpr::Type::INTEGER:
            write(Fa_StringRef(std::to_string(literal_expr->get_int()).c_str()));
            break;
        case AST::Fa_LiteralExpr::Type::NIL:
            write("عدم");
            break;
        case AST::Fa_LiteralExpr::Type::STRING:
            write(Fa_StringRef(escape_string_literal(literal_expr->get_str()).c_str()));
            break;
        }
        break;
    }
    case AST::Fa_Expr::Kind::NAME: {
        auto const* name_expr = static_cast<AST::Fa_NameExpr const*>(expr);
        write(name_expr->get_value());
        break;
    }
    case AST::Fa_Expr::Kind::UNARY: {
        auto const* unary_expr = static_cast<AST::Fa_UnaryExpr const*>(expr);
        Fa_StringRef op = unary_op_string(unary_expr->get_operator());
        write(op);
        if (op == "ليس")
            write(" ");
        format_expression(unary_expr->get_operand(), current_precedence, false);
        break;
    }
    case AST::Fa_Expr::Kind::INVALID:
        break;
    }

    if (needs_parens)
        write(')');
}

void Fa_Formatter::format_body(AST::Fa_Stmt const* stmt)
{
    if (stmt == nullptr)
        return;

    m_indent_level += 1;

    if (stmt->get_kind() == AST::Fa_Stmt::Kind::BLOCK) {
        auto const* block_stmt = static_cast<AST::Fa_BlockStmt const*>(stmt);
        if (block_stmt->get_statements().empty()) {
            write_newline();
        } else {
            for (AST::Fa_Stmt const* inner : block_stmt->get_statements()) {
                write_newline();
                format_statement(inner);
            }
        }
    } else {
        write_newline();
        format_statement(stmt);
    }

    m_indent_level -= 1;
}

void Fa_Formatter::format_if_statement(AST::Fa_IfStmt const* stmt)
{
    write("اذا ");
    format_expression(stmt->get_condition());
    write(":");
    format_body(stmt->get_then());

    if (stmt->get_else() == nullptr)
        return;

    write_newline();
    write("غيره");

    if (stmt->get_else()->get_kind() == AST::Fa_Stmt::Kind::IF) {
        write(" ");
        format_if_statement(static_cast<AST::Fa_IfStmt const*>(stmt->get_else()));
        return;
    }

    write(":");
    format_body(stmt->get_else());
}

void Fa_Formatter::format_statement(AST::Fa_Stmt const* stmt)
{
    if (stmt == nullptr)
        return;

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::ASSIGNMENT: {
        auto const* assign_stmt = static_cast<AST::Fa_AssignmentStmt const*>(stmt);
        format_expression(assign_stmt->get_expr());
        break;
    }
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto const* block_stmt = static_cast<AST::Fa_BlockStmt const*>(stmt);
        for (u32 i = 0; i < block_stmt->get_statements().size(); i += 1) {
            if (i != 0)
                write_newline();
            format_statement(block_stmt->get_statements()[i]);
        }
        break;
    }
    case AST::Fa_Stmt::Kind::BREAK:
        write("اخرج");
        break;
    case AST::Fa_Stmt::Kind::CLASS_DEF: {
        auto const* class_stmt = static_cast<AST::Fa_ClassDef const*>(stmt);
        write("نوع ");
        format_expression(class_stmt->get_name());
        write(":");

        Fa_Array<AST::Fa_Stmt*> methods = class_stmt->get_methods();
        m_indent_level += 1;
        for (AST::Fa_Stmt const* method : methods) {
            write_newline();
            format_statement(method);
        }
        m_indent_level -= 1;
        break;
    }
    case AST::Fa_Stmt::Kind::CONTINUE:
        write("اكمل");
        break;
    case AST::Fa_Stmt::Kind::EXPR: {
        auto const* expr_stmt = static_cast<AST::Fa_ExprStmt const*>(stmt);
        format_expression(expr_stmt->get_expr());
        break;
    }
    case AST::Fa_Stmt::Kind::FOR: {
        auto const* for_stmt = static_cast<AST::Fa_ForStmt const*>(stmt);
        write("بكل ");
        format_expression(for_stmt->get_target());
        write(" في ");
        format_expression(for_stmt->get_iter());
        write(":");
        format_body(for_stmt->get_body());
        break;
    }
    case AST::Fa_Stmt::Kind::FUNC: {
        auto const* fn_stmt = static_cast<AST::Fa_FunctionDef const*>(stmt);
        write("دالة ");
        format_expression(fn_stmt->get_name());
        write('(');
        format_comma_separated(fn_stmt->get_parameters());
        write("):");
        format_body(fn_stmt->get_body());
        break;
    }
    case AST::Fa_Stmt::Kind::IF:
        format_if_statement(static_cast<AST::Fa_IfStmt const*>(stmt));
        break;
    case AST::Fa_Stmt::Kind::RETURN: {
        auto const* ret_stmt = static_cast<AST::Fa_ReturnStmt const*>(stmt);
        write("ارجع");
        if (ret_stmt->has_value()) {
            write(" ");
            format_expression(ret_stmt->get_value());
        }
        break;
    }
    case AST::Fa_Stmt::Kind::WHILE: {
        auto const* while_stmt = static_cast<AST::Fa_WhileStmt const*>(stmt);
        write("طالما ");
        format_expression(while_stmt->get_condition());
        write(":");
        format_body(while_stmt->get_body());
        break;
    }
    case AST::Fa_Stmt::Kind::INVALID:
        break;
    }
}

} // namespace fairuz
