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
        ++start;
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
    std::string line;
    std::ostringstream source;
    std::ostringstream expected;

    while (std::getline(in, line)) {
        if (line.rfind("KIND:", 0) == 0) {
            gc.kind = trim(line.substr(5));
            continue;
        }
        if (line == "SOURCE:") {
            section = Section::Source;
            continue;
        }
        if (line == "EXPECTED:") {
            section = Section::Expected;
            continue;
        }
        if (section == Section::Source)
            source << line << '\n';
        else if (section == Section::Expected)
            expected << line << '\n';
    }

    gc.source = trim(source.str());
    gc.expected = trim(expected.str());
    return gc;
}

std::string binOpName(AST::Fa_BinaryOp op)
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

std::string unaryOpName(AST::Fa_UnaryOp op)
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

std::string sigExpr(AST::Fa_Expr const* expr);
std::string sigStmt(AST::Fa_Stmt const* stmt);

std::string sigList(AST::Fa_ListExpr const* list)
{
    std::ostringstream out;
    out << "[";
    for (u32 i = 0; i < list->getElements().size(); ++i) {
        if (i)
            out << ",";
        out << sigExpr(list->getElements()[i]);
    }
    out << "]";
    return out.str();
}

std::string sigExpr(AST::Fa_Expr const* expr)
{
    if (!expr)
        return "null";

    switch (expr->getKind()) {
    case AST::Fa_Expr::Kind::LITERAL: {
        auto const* lit = static_cast<AST::Fa_LiteralExpr const*>(expr);
        if (lit->isNil())
            return "nil";
        if (lit->isBoolean())
            return std::string("bool(") + (lit->getBool() ? "true" : "false") + ")";
        if (lit->isInteger())
            return "int(" + std::to_string(lit->getInt()) + ")";
        if (lit->isDecimal())
            return "float(" + std::to_string(lit->getFloat()) + ")";
        if (lit->isString())
            return "str(" + std::string(lit->getStr().data(), lit->getStr().len()) + ")";
        return "lit(?)";
    }
    case AST::Fa_Expr::Kind::NAME: {
        auto const* name = static_cast<AST::Fa_NameExpr const*>(expr);
        return "name(" + std::string(name->getValue().data(), name->getValue().len()) + ")";
    }
    case AST::Fa_Expr::Kind::UNARY: {
        auto const* un = static_cast<AST::Fa_UnaryExpr const*>(expr);
        return "unary(" + unaryOpName(un->getOperator()) + "," + sigExpr(un->getOperand()) + ")";
    }
    case AST::Fa_Expr::Kind::BINARY: {
        auto const* bin = static_cast<AST::Fa_BinaryExpr const*>(expr);
        return "bin(" + binOpName(bin->getOperator()) + "," + sigExpr(bin->getLeft()) + "," + sigExpr(bin->getRight()) + ")";
    }
    case AST::Fa_Expr::Kind::LIST:
        return "list" + sigList(static_cast<AST::Fa_ListExpr const*>(expr));
    case AST::Fa_Expr::Kind::CALL: {
        auto const* call = static_cast<AST::Fa_CallExpr const*>(expr);
        return "call(" + sigExpr(call->getCallee()) + "," + sigList(call->getArgsAsListExpr()) + ")";
    }
    case AST::Fa_Expr::Kind::ASSIGNMENT: {
        auto const* a = static_cast<AST::Fa_AssignmentExpr const*>(expr);
        return "assign(" + sigExpr(a->getTarget()) + "," + sigExpr(a->getValue()) + ",decl=" + std::string(a->isDeclaration() ? "true" : "false") + ")";
    }
    case AST::Fa_Expr::Kind::INDEX: {
        auto const* idx = static_cast<AST::Fa_IndexExpr const*>(expr);
        return "index(" + sigExpr(idx->getObject()) + "," + sigExpr(idx->getIndex()) + ")";
    }
    default:
        return "expr(?)";
    }
}

std::string sigStmt(AST::Fa_Stmt const* stmt)
{
    if (!stmt)
        return "null";

    switch (stmt->getKind()) {
    case AST::Fa_Stmt::Kind::BLOCK: {
        auto const* block = static_cast<AST::Fa_BlockStmt const*>(stmt);
        std::ostringstream out;
        out << "block[";
        for (u32 i = 0; i < block->getStatements().size(); ++i) {
            if (i)
                out << ",";
            out << sigStmt(block->getStatements()[i]);
        }
        out << "]";
        return out.str();
    }
    case AST::Fa_Stmt::Kind::EXPR:
        return "expr(" + sigExpr(static_cast<AST::Fa_ExprStmt const*>(stmt)->getExpr()) + ")";
    case AST::Fa_Stmt::Kind::ASSIGNMENT: {
        auto const* a = static_cast<AST::Fa_AssignmentStmt const*>(stmt);
        return "assignstmt(" + sigExpr(a->getTarget()) + "," + sigExpr(a->getValue()) + ",decl=" + std::string(a->isDeclaration() ? "true" : "false") + ")";
    }
    case AST::Fa_Stmt::Kind::IF: {
        auto const* i = static_cast<AST::Fa_IfStmt const*>(stmt);
        return "if(" + sigExpr(i->getCondition()) + "," + sigStmt(i->getThen()) + "," + sigStmt(i->getElse()) + ")";
    }
    case AST::Fa_Stmt::Kind::WHILE: {
        auto const* w = static_cast<AST::Fa_WhileStmt const*>(stmt);
        return "while(" + sigExpr(w->getCondition()) + "," + sigStmt(w->getBody()) + ")";
    }
    case AST::Fa_Stmt::Kind::FUNC: {
        auto const* fn = static_cast<AST::Fa_FunctionDef const*>(stmt);
        std::ostringstream out;
        out << "func(" << std::string(fn->getName()->getValue().data(), fn->getName()->getValue().len()) << ",[";
        for (u32 i = 0; i < fn->getParameters().size(); ++i) {
            if (i)
                out << ",";
            out << sigExpr(fn->getParameters()[i]);
        }
        out << "]," << sigStmt(fn->getBody()) << ")";
        return out.str();
    }
    case AST::Fa_Stmt::Kind::RETURN: {
        auto const* ret = static_cast<AST::Fa_ReturnStmt const*>(stmt);
        return std::string("return(") + (ret->hasValue() ? sigExpr(ret->getValue()) : "nil") + ")";
    }
    default:
        return "stmt(?)";
    }
}

std::string sigProgram(Fa_Array<AST::Fa_Stmt*> const& stmts)
{
    std::ostringstream out;
    out << "program[";
    for (u32 i = 0; i < stmts.size(); ++i) {
        if (i)
            out << ",";
        out << sigStmt(stmts[i]);
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
            auto expr = parser.parse();
            ASSERT_TRUE(expr.hasValue()) << entry.path();
            EXPECT_EQ(sigExpr(expr.value()), gc.expected);
        } else if (gc.kind == "program") {
            auto program = parser.parseProgram();
            EXPECT_EQ(sigProgram(program), gc.expected);
            EXPECT_FALSE(diagnostic::hasErrors()) << diagnostic::engine.toJSON();
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
            auto expr = parser.parse();
            EXPECT_TRUE(expr.hasError());
        } else if (gc.kind == "program") {
            parser.parseProgram();
            EXPECT_TRUE(diagnostic::hasErrors());
        } else {
            FAIL() << "unknown kind: " << gc.kind;
        }

        std::string json = diagnostic::engine.toJSON();
        EXPECT_NE(json.find(gc.expected), std::string::npos) << json;
    }
}
