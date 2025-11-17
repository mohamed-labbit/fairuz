#include "../../include/parser/advanced.hpp"


namespace mylang {
namespace parser {

std::vector<ast::StmtPtr> ParallelParser::parseParallel(
  const std::vector<mylang::lex::tok::Token>& tokens, std::int32_t threadCount)
{
    // Split tokens by top-level definitions
    std::vector<std::vector<mylang::lex::tok::Token>> chunks = splitIntoChunks(tokens);
    std::vector<std::future<ast::StmtPtr>> futures;

    for (auto& chunk : chunks)
    {
        futures.push_back(std::async(std::launch::async, [chunk]() {
            mylang::parser::Parser parser(chunk);
            auto stmts = parser.parse();
            return stmts.empty() ? nullptr : std::move(stmts[0]);
        }));
    }

    std::vector<ast::StmtPtr> result;
    for (auto& future : futures)
    {
        if (auto stmt = future.get())
        {
            result.push_back(std::move(stmt));
        }
    }
    return result;
}

std::vector<std::vector<mylang::lex::tok::Token>> ParallelParser::splitIntoChunks(
  const std::vector<mylang::lex::tok::Token>& tokens)
{
    std::vector<std::vector<mylang::lex::tok::Token>> chunks;
    std::vector<mylang::lex::tok::Token> current;

    for (const auto& tok : tokens)
    {
        current.push_back(tok);
        if (tok.type() == lex::tok::TokenType::KW_FN && current.size() > 1)
        {
            chunks.push_back(current);
            current.clear();
            current.push_back(tok);
        }
    }
    if (!current.empty())
    {
        chunks.push_back(current);
    }
    return chunks;
}

// Generate Python bytecode-like instructions
std::vector<typename CodeGenerator::Bytecode> CodeGenerator::generateBytecode(const std::vector<ast::StmtPtr>& ast)
{
    std::vector<Bytecode> code;

    for (const auto& stmt : ast)
    {
        generateStmt(stmt.get(), code);
    }

    return code;
}

// Generate C++ code
std::string CodeGenerator::generateCPP(const std::vector<ast::StmtPtr>& ast)
{
    std::stringstream ss;
    ss << "#include <iostream>\n";
    ss << "#include <vector>\n";
    ss << "#include <string>\n\n";

    for (const auto& stmt : ast)
    {
        ss << utf8::utf16to8(generateCPPStmt(stmt.get()));
    }

    return ss.str();
}

void CodeGenerator::generateStmt(const ast::Stmt* stmt, std::vector<Bytecode>& code)
{
    if (!stmt)
    {
        return;
    }

    switch (stmt->kind)
    {
    case ast::Stmt::Kind::ASSIGNMENT : {
        auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
        generateExpr(assign->value.get(), code);
        code.push_back({Bytecode::Op::STORE_VAR, 0});  // Store to variable
        break;
    }
    case ast::Stmt::Kind::EXPRESSION : {
        auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
        generateExpr(exprStmt->expression.get(), code);
        code.push_back({Bytecode::Op::POP, 0});
        break;
    }
    case ast::Stmt::Kind::RETURN : {
        auto* ret = static_cast<const ast::ReturnStmt*>(stmt);
        generateExpr(ret->value.get(), code);
        code.push_back({Bytecode::Op::RETURN, 0});
        break;
    }
    default :
        break;
    }
}

void CodeGenerator::generateExpr(const ast::Expr* expr, std::vector<Bytecode>& code)
{
    if (!expr)
    {
        return;
    }

    switch (expr->kind)
    {
    case ast::Expr::Kind::LITERAL : {
        code.push_back({Bytecode::Op::LOAD_CONST, 0});
        break;
    }
    case ast::Expr::Kind::NAME : {
        code.push_back({Bytecode::Op::LOAD_VAR, 0});
        break;
    }
    case ast::Expr::Kind::BINARY : {
        auto* bin = static_cast<const ast::BinaryExpr*>(expr);
        generateExpr(bin->left.get(), code);
        generateExpr(bin->right.get(), code);

        if (bin->op == u"+")
        {
            code.push_back({Bytecode::Op::ADD, 0});
        }
        else if (bin->op == u"-")
        {
            code.push_back({Bytecode::Op::SUB, 0});
        }
        else if (bin->op == u"*")
        {
            code.push_back({Bytecode::Op::MUL, 0});
        }
        else if (bin->op == u"/")
        {
            code.push_back({Bytecode::Op::DIV, 0});
        }
        break;
    }
    default :
        break;
    }
}

std::u16string CodeGenerator::generateCPPStmt(const ast::Stmt* stmt)
{
    if (!stmt)
    {
        return u"";
    }

    switch (stmt->kind)
    {
    case ast::Stmt::Kind::ASSIGNMENT : {
        auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
        return u"auto " + assign->target + u" = " + generateCPPExpr(assign->value.get()) + u";\n";
    }
    case ast::Stmt::Kind::EXPRESSION : {
        auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
        return generateCPPExpr(exprStmt->expression.get()) + u";\n";
    }
    case ast::Stmt::Kind::IF : {
        auto* ifStmt = static_cast<const ast::IfStmt*>(stmt);
        std::u16string result = u"if (" + generateCPPExpr(ifStmt->condition.get()) + u") {\n";
        for (const auto& s : ifStmt->thenBlock)
        {
            result += u"  " + generateCPPStmt(s.get());
        }
        result += u"}\n";
        return result;
    }
    case ast::Stmt::Kind::FUNCTION_DEF : {
        auto* func = static_cast<const ast::FunctionDef*>(stmt);
        std::u16string result = u"auto " + func->name + u"(";
        for (std::size_t i = 0; i < func->params.size(); i++)
        {
            result += u"auto " + func->params[i];
            if (i + 1 < func->params.size())
            {
                result += u", ";
            }
        }
        result += u") {\n";
        for (const auto& s : func->body)
        {
            result += u"  " + generateCPPStmt(s.get());
        }
        result += u"}\n";
        return result;
    }
    default :
        return u"";
    }
}

std::u16string CodeGenerator::generateCPPExpr(const ast::Expr* expr)
{
    if (!expr)
    {
        return u"";
    }

    switch (expr->kind)
    {
    case ast::Expr::Kind::LITERAL : {
        auto* lit = static_cast<const ast::LiteralExpr*>(expr);
        return lit->value;
    }
    case ast::Expr::Kind::NAME : {
        auto* name = static_cast<const ast::NameExpr*>(expr);
        return name->name;
    }
    case ast::Expr::Kind::BINARY : {
        auto* bin = static_cast<const ast::BinaryExpr*>(expr);
        return u"(" + generateCPPExpr(bin->left.get()) + u" " + bin->op + u" " + generateCPPExpr(bin->right.get())
          + u")";
    }
    case ast::Expr::Kind::CALL : {
        auto* call = static_cast<const ast::CallExpr*>(expr);
        std::u16string result = generateCPPExpr(call->callee.get()) + u"(";
        for (std::size_t i = 0; i < call->args.size(); i++)
        {
            result += generateCPPExpr(call->args[i].get());
            if (i + 1 < call->args.size())
            {
                result += u", ";
            }
        }
        result += u")";
        return result;
    }
    default :
        return u"";
    }
}

}
}