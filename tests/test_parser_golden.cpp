#include "../include/ast.hpp"
#include "../include/parser.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>

using namespace fairuz;
using namespace fairuz::lex;
using namespace fairuz::parser;

namespace {

std::filesystem::path golden_root()
{
    return std::filesystem::path(__FILE__).parent_path() / "golden" / "parser";
}

struct GoldenCase {
    std::string kind;
    std::string source;
    std::string expected;
};

std::string trim(std::string s)
{
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t start = 0;
    while (start < s.size() && (s[start] == '\n' || s[start] == '\r' || s[start] == ' ' || s[start] == '\t'))
        start += 1;
    return s.substr(start);
}

GoldenCase load_case(std::filesystem::path const& path)
{
    std::ifstream in(path);
    EXPECT_TRUE(in.good()) << path;

    enum class Section {
        None,
        Source,
        Expected
    };

    GoldenCase gc;
    Section section = Section::None;
    std::string m_line;
    std::ostringstream source;
    std::ostringstream expected;

    while (std::getline(in, m_line)) {
        if (m_line.rfind("KIND:", 0) == 0) {
            gc.kind = trim(m_line.substr(5));
            continue;
        }
        if (m_line == "SOURCE:") {
            section = Section::Source;
            continue;
        }
        if (m_line == "EXPECTED:") {
            section = Section::Expected;
            continue;
        }
        if (section == Section::Source)
            source << m_line << '\n';
        else if (section == Section::Expected)
            expected << m_line << '\n';
    }

    gc.source = trim(source.str());
    gc.expected = trim(expected.str());
    return gc;
}

std::string bin_op_name(AST::Fa_BinaryOp op)
{
    switch (op) {
    case AST::Fa_BinaryOp::OP_ADD:
        return "add";
    case AST::Fa_BinaryOp::OP_SUB:
        return "sub";
    case AST::Fa_BinaryOp::OP_MUL:
        return "mul";
    case AST::Fa_BinaryOp::OP_DIV:
        return "div";
    case AST::Fa_BinaryOp::OP_MOD:
        return "mod";
    case AST::Fa_BinaryOp::OP_POW:
        return "pow";
    case AST::Fa_BinaryOp::OP_EQ:
        return "eq";
    case AST::Fa_BinaryOp::OP_NEQ:
        return "neq";
    case AST::Fa_BinaryOp::OP_LT:
        return "lt";
    case AST::Fa_BinaryOp::OP_GT:
        return "gt";
    case AST::Fa_BinaryOp::OP_LTE:
        return "lte";
    case AST::Fa_BinaryOp::OP_GTE:
        return "gte";
    case AST::Fa_BinaryOp::OP_AND:
        return "and";
    case AST::Fa_BinaryOp::OP_OR:
        return "or";
    case AST::Fa_BinaryOp::OP_BITAND:
        return "bitand";
    case AST::Fa_BinaryOp::OP_BITOR:
        return "bitor";
    case AST::Fa_BinaryOp::OP_BITXOR:
        return "bitxor";
    case AST::Fa_BinaryOp::OP_LSHIFT:
        return "lshift";
    case AST::Fa_BinaryOp::OP_RSHIFT:
        return "rshift";
    default:
        return "invalid";
    }
}

std::string unary_op_name(AST::Fa_UnaryOp op)
{
    switch (op) {
    case AST::Fa_UnaryOp::OP_PLUS:
        return "plus";
    case AST::Fa_UnaryOp::OP_NEG:
        return "neg";
    case AST::Fa_UnaryOp::OP_BITNOT:
        return "bitnot";
    case AST::Fa_UnaryOp::OP_NOT:
        return "not";
    default:
        return "invalid";
    }
}

std::string sig_expr(AST::Fa_Expr const* m_expr);
std::string sig_stmt(AST::Fa_Stmt const* stmt);

std::string sig_list(AST::Fa_ListExpr const* list)
{
    std::ostringstream out;
    out << "[";
    for (u32 i = 0; i < list->get_elements().size(); i += 1) {
        if (i)
            out << ",";
        out << sig_expr(list->get_elements()[i]);
    }
    out << "]";
    return out.str();
}

std::string sig_expr(AST::Fa_Expr const* m_expr)
{
    if (!m_expr)
        return "null";

    switch (m_expr->get_kind()) {
    case AST::Fa_Expr::Kind::LITERAL: {
        auto const* lit = static_cast<AST::Fa_LiteralExpr const*>(m_expr);
        if (lit->is_nil())
            return "nil";
        if (lit->is_bool())
            return std::string("bool(") + (lit->get_bool() ? "true" : "false") + ")";
        if (lit->is_integer())
            return "int(" + std::to_string(lit->get_int()) + ")";
        if (lit->is_float())
            return "float(" + std::to_string(lit->get_float()) + ")";
        if (lit->is_string())
            return "str(" + std::string(lit->get_str().data(), lit->get_str().len()) + ")";
        return "lit(?)";
    }
    case AST::Fa_Expr::Kind::NAME: {
        auto const* m_name = static_cast<AST::Fa_NameExpr const*>(m_expr);
        return "name(" + std::string(m_name->get_value().data(), m_name->get_value().len()) + ")";
    }
    case AST::Fa_Expr::Kind::UNARY: {
        auto const* un = static_cast<AST::Fa_UnaryExpr const*>(m_expr);
        return "unary(" + unary_op_name(un->get_operator()) + "," + sig_expr(un->get_operand()) + ")";
    }
    case AST::Fa_Expr::Kind::BINARY: {
        auto const* bin = static_cast<AST::Fa_BinaryExpr const*>(m_expr);
        return "bin(" + bin_op_name(bin->get_operator()) + "," + sig_expr(bin->get_left()) + "," + sig_expr(bin->get_right()) + ")";
    }
    case AST::Fa_Expr::Kind::LIST:
        return "list" + sig_list(static_cast<AST::Fa_ListExpr const*>(m_expr));
    case AST::Fa_Expr::Kind::CALL: {
        auto const* call = static_cast<AST::Fa_CallExpr const*>(m_expr);
        return "call(" + sig_expr(call->get_callee()) + "," + sig_list(call->get_args_as_list_expr()) + ")";
    }
    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto const* a = static_cast<AST::Fa_AssignmentExpr const*>(m_expr);
        return "assign(" + sig_expr(a->get_target()) + "," + sig_expr(a->get_value()) + ",decl=" + std::string(a->is_declaration() ? "true" : "false") + ")";
    }
    case AST::Fa_Expr::Kind::INDEX: {
        auto const* idx = static_cast<AST::Fa_IndexExpr const*>(m_expr);
        return "index(" + sig_expr(idx->get_object()) + "," + sig_expr(idx->get_index()) + ")";
    }
    default:
        return "expr(?)";
    }
}

std::string sig_stmt(AST::Fa_Stmt const* stmt)
{
    if (!stmt)
        return "null";

    switch (stmt->get_kind()) {
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto const* block = static_cast<AST::Fa_BlockStmt const*>(stmt);
        std::ostringstream out;
        out << "block[";
        for (u32 i = 0; i < block->get_statements().size(); i += 1) {
            if (i)
                out << ",";
            out << sig_stmt(block->get_statements()[i]);
        }
        out << "]";
        return out.str();
    }
    case AST::Fa_Stmt::Kind::EXPR:
        return "expr(" + sig_expr(static_cast<AST::Fa_ExprStmt const*>(stmt)->get_expr()) + ")";
    case AST::Fa_Stmt::Kind::ASSIGNMENT: {
        auto const* a = static_cast<AST::Fa_AssignmentStmt const*>(stmt);
        return "assignstmt(" + sig_expr(a->get_target()) + "," + sig_expr(a->get_value()) + ",decl=" + std::string(a->is_declaration() ? "true" : "false") + ")";
    }
    case AST::Fa_Stmt::Kind::IF: {
        auto const* i = static_cast<AST::Fa_IfStmt const*>(stmt);
        return "if(" + sig_expr(i->get_condition()) + "," + sig_stmt(i->get_then()) + "," + sig_stmt(i->get_else()) + ")";
    }
    case AST::Fa_Stmt::Kind::WHILE: {
        auto const* w = static_cast<AST::Fa_WhileStmt const*>(stmt);
        return "while(" + sig_expr(w->get_condition()) + "," + sig_stmt(w->get_body()) + ")";
    }
    case AST::Fa_Stmt::Kind::FUNC: {
        auto const* fn = static_cast<AST::Fa_FunctionDef const*>(stmt);
        std::ostringstream out;
        out << "func(" << std::string(fn->get_name()->get_value().data(), fn->get_name()->get_value().len()) << ",[";
        for (u32 i = 0; i < fn->get_parameters().size(); i += 1) {
            if (i)
                out << ",";
            out << sig_expr(fn->get_parameters()[i]);
        }
        out << "]," << sig_stmt(fn->get_body()) << ")";
        return out.str();
    }
    case AST::Fa_Stmt::Kind::RETURN: {
        auto const* ret = static_cast<AST::Fa_ReturnStmt const*>(stmt);
        return std::string("return(") + (ret->has_value() ? sig_expr(ret->get_value()) : "nil") + ")";
    }
    default:
        return "stmt(?)";
    }
}

std::string sig_program(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    std::ostringstream out;
    out << "program[";
    for (u32 i = 0; i < stmts.size(); i += 1) {
        if (i)
            out << ",";
        out << sig_stmt(stmts[i]);
    }
    out << "]";
    return out.str();
}

} // namespace

TEST(ParserGolden, ValidCorpus)
{
    auto const dir = golden_root() / "valid";
    ASSERT_TRUE(std::filesystem::exists(dir));

    diagnostic::reset();

    for (auto const& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;

        SCOPED_TRACE(entry.path().string());

        GoldenCase gc = load_case(entry.path());
        Fa_FileManager fm(entry.path().string());
        fm.buffer() = gc.source.c_str();
        Fa_Parser parser(&fm);

        if (gc.kind == "expr") {
            auto m_expr = parser.parse();
            ASSERT_TRUE(m_expr.has_value()) << entry.path();
            EXPECT_EQ(sig_expr(m_expr.m_value()), gc.expected);
        } else if (gc.kind == "program") {
            auto program = parser.parse_program();
            EXPECT_EQ(sig_program(program), gc.expected);
            EXPECT_FALSE(diagnostic::has_errors()) << diagnostic::engine.to_json();
        } else {
            FAIL() << "unknown kind: " << gc.kind;
        }
    }
}

TEST(ParserGolden, InvalidCorpus)
{
    auto const dir = golden_root() / "invalid";
    ASSERT_TRUE(std::filesystem::exists(dir));

    diagnostic::reset();

    for (auto const& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file())
            continue;
        SCOPED_TRACE(entry.path().string());
        diagnostic::reset();

        GoldenCase gc = load_case(entry.path());
        Fa_FileManager fm(entry.path().string());
        fm.buffer() = gc.source.c_str();
        Fa_Parser parser(&fm);

        if (gc.kind == "expr") {
            auto m_expr = parser.parse();
            EXPECT_TRUE(m_expr.has_error());
        } else if (gc.kind == "program") {
            parser.parse_program();
            EXPECT_TRUE(diagnostic::has_errors());
        } else {
            FAIL() << "unknown kind: " << gc.kind;
        }

        std::string json = diagnostic::engine.to_json();
        EXPECT_NE(json.find(gc.expected), std::string::npos) << json;
    }
}
