
#if 0
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
        futures.push_back(std::async(std::launch::async, [chunk]() {
            mylang::parser::Parser parser(chunk);
            auto stmts = parser.parse();
            return stmts.empty() ? nullptr : std::move(stmts[0]);
        }));

    std::vector<ast::StmtPtr> result;
    for (auto& future : futures)
        if (auto stmt = future.get())
            result.push_back(std::move(stmt));
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
        chunks.push_back(current);
    return chunks;
}

// Generate Python bytecode-like instructions
std::vector<typename CodeGenerator::Bytecode> CodeGenerator::generateBytecode(const std::vector<ast::StmtPtr>& ast)
{
    std::vector<Bytecode> code;
    for (const auto& stmt : ast)
        generateStmt(stmt.get(), code);
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
        ss << utf8::utf16to8(generateCPPStmt(stmt.get()));
    return ss.str();
}

void CodeGenerator::generateStmt(const ast::Stmt* stmt, std::vector<Bytecode>& code)
{
    if (!stmt)
        return;

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
        return;

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
            code.push_back({Bytecode::Op::ADD, 0});
        else if (bin->op == u"-")
            code.push_back({Bytecode::Op::SUB, 0});
        else if (bin->op == u"*")
            code.push_back({Bytecode::Op::MUL, 0});
        else if (bin->op == u"/")
            code.push_back({Bytecode::Op::DIV, 0});
        break;
    }
    default :
        break;
    }
}

string_type CodeGenerator::generateCPPStmt(const ast::Stmt* stmt)
{
    if (!stmt)
        return u"";

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
        string_type result = u"if (" + generateCPPExpr(ifStmt->condition.get()) + u") {\n";
        for (const auto& s : ifStmt->thenBlock)
            result += u"  " + generateCPPStmt(s.get());
        result += u"}\n";
        return result;
    }
    case ast::Stmt::Kind::FUNCTION_DEF : {
        auto* func = static_cast<const ast::FunctionDef*>(stmt);
        string_type result = u"auto " + func->name + u"(";
        for (std::size_t i = 0; i < func->params.size(); i++)
        {
            result += u"auto " + func->params[i];
            if (i + 1 < func->params.size())
                result += u", ";
        }
        result += u") {\n";
        for (const auto& s : func->body)
            result += u"  " + generateCPPStmt(s.get());
        result += u"}\n";
        return result;
    }
    default :
        return u"";
    }

    return u"";
}

string_type CodeGenerator::generateCPPExpr(const ast::Expr* expr)
{
    if (!expr)
        return u"";

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
        string_type result = generateCPPExpr(call->callee.get()) + u"(";
        for (std::size_t i = 0; i < call->args.size(); i++)
        {
            result += generateCPPExpr(call->args[i].get());
            if (i + 1 < call->args.size())
                result += u", ";
        }
        result += u")";
        return result;
    }
    default :
        return u"";
    }

    return u"";
}

std::size_t IncrementalParser::hashRegion(const string_type& source, std::size_t start, std::size_t end)
{
    std::hash<string_type> hasher;
    return hasher(source.substr(start, end - start));
}

// Only reparse changed regions
std::vector<ast::StmtPtr> IncrementalParser::parseIncremental(
  const string_type& newSource, const std::vector<std::size_t>& changedLines)
{
    // Identify unchanged regions and reuse cached AST
    std::vector<ast::StmtPtr> result;
    // Implementation would diff source and reparse only changes
    return result;
}

bool TypeSystem::Type::operator==(const Type& other) const
{
    return base == other.base;  // Simplified
}

string_type TypeSystem::Type::toString() const
{
    switch (base)
    {
    case BaseType::Int :
        return u"int";
    case BaseType::Float :
        return u"float";
    case BaseType::String :
        return u"str";
    case BaseType::Bool :
        return u"bool";
    case BaseType::None :
        return u"None";
    case BaseType::List :
        if (!typeParams.empty())
            return u"List[" + typeParams[0]->toString() + u"]";
        return u"List";
    case BaseType::Dict :
        if (typeParams.size() >= 2)
            return u"Dict[" + typeParams[0]->toString() + u", " + typeParams[1]->toString() + u"]";
        return u"Dict";
    default :
        return u"Any";
    }

    return u"Any";
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::freshTypeVar()
{
    auto t = std::make_shared<Type>();
    t->base = BaseType::Any;
    return t;
}

void TypeSystem::TypeInference::unify(std::shared_ptr<TypeSystem::Type> t1, std::shared_ptr<TypeSystem::Type> t2)
{
    if (*t1 == *t2)
        return;

    // Type variable substitution
    if (t1->base == BaseType::Any)
    {
        *t1 = *t2;
    }
    else if (t2->base == BaseType::Any)
    {
        *t2 = *t1;
    }
    else if (t1->base == BaseType::List && t2->base == BaseType::List)
    {
        if (!t1->typeParams.empty() && !t2->typeParams.empty())
            unify(t1->typeParams[0], t2->typeParams[0]);
    }
    else
    {
        throw std::runtime_error(
          "Type mismatch: " + utf8::utf16to8(t1->toString()) + " vs " + utf8::utf16to8(t2->toString()));
    }
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::inferExpr(const ast::Expr* expr)
{
    if (!expr)
        return std::make_shared<Type>();

    switch (expr->kind)
    {
    case ast::Expr::Kind::LITERAL : {
        auto* lit = static_cast<const ast::LiteralExpr*>(expr);
        auto t = std::make_shared<Type>();
        switch (lit->litType)
        {
        case ast::LiteralExpr::Type::NUMBER :
            t->base = lit->value.find('.') != std::string::npos ? BaseType::Float : BaseType::Int;
            break;
        case ast::LiteralExpr::Type::STRING :
            t->base = BaseType::String;
            break;
        case ast::LiteralExpr::Type::BOOLEAN :
            t->base = BaseType::Bool;
            break;
        case ast::LiteralExpr::Type::NONE :
            t->base = BaseType::None;
            break;
        }
        return t;
    }
    case ast::Expr::Kind::BINARY : {
        auto* bin = static_cast<const ast::BinaryExpr*>(expr);
        auto leftType = inferExpr(bin->left.get());
        auto rightType = inferExpr(bin->right.get());
        unify(leftType, rightType);
        // Result type based on operator
        if (bin->op == u"+" || bin->op == u"-" || bin->op == u"*" || bin->op == u"/")
        {
            return leftType;
        }
        else if (bin->op == u"==" || bin->op == u"!=" || bin->op == u"<" || bin->op == u">")
        {
            auto t = std::make_shared<Type>();
            t->base = BaseType::Bool;
            return t;
        }
        return leftType;
    }
    case ast::Expr::Kind::LIST : {
        auto* list = static_cast<const ast::ListExpr*>(expr);
        auto t = std::make_shared<Type>();
        t->base = BaseType::List;
        if (!list->elements.empty())
        {
            auto elemType = inferExpr(list->elements[0].get());
            t->typeParams.push_back(elemType);
        }
        return t;
    }
    default :
        return freshTypeVar();
    }

    return freshTypeVar();
}

void DiagnosticEngine::setSource(const string_type& source) { sourceCode_ = source; }

void DiagnosticEngine::report(Severity sev,
  std::int32_t line,
  std::int32_t col,
  std::int32_t len,
  const string_type& msg,
  const string_type& code)
{
    Diagnostic diag;
    diag.severity = sev;
    diag.line = line;
    diag.column = col;
    diag.length = len;
    diag.message = msg;
    diag.code = code;
    diagnostics_.push_back(diag);
}

void DiagnosticEngine::addSuggestion(const string_type& suggestion)
{
    if (!diagnostics_.empty())
        diagnostics_.back().suggestions.push_back(suggestion);
}

void DiagnosticEngine::addNote(std::int32_t line, const string_type& note)
{
    if (!diagnostics_.empty())
        diagnostics_.back().notes.push_back({line, note});
}

// Generate LSP-compatible diagnostics
std::string DiagnosticEngine::toJSON() const
{
    std::stringstream ss;
    ss << "[\n";
    for (std::size_t i = 0; i < diagnostics_.size(); i++)
    {
        const auto& diag = diagnostics_[i];
        ss << "  {\n";
        ss << "    \"severity\": " << static_cast<std::int32_t>(diag.severity) << ",\n";
        ss << "    \"line\": " << diag.line << ",\n";
        ss << "    \"column\": " << diag.column << ",\n";
        ss << "    \"message\": \"" << utf8::utf16to8(diag.message) << "\",\n";
        ss << "    \"code\": \"" << utf8::utf16to8(diag.code) << "\"\n";
        ss << "  }";
        if (i + 1 < diagnostics_.size())
            ss << ",";
        ss << "\n";
    }
    ss << "]\n";
    return ss.str();
}

// Beautiful terminal output with colors
void DiagnosticEngine::prettyPrint() const
{
    for (const auto& diag : diagnostics_)
    {
        string_type sevStr;
        string_type color;

        switch (diag.severity)
        {
        case Severity::Note :
            sevStr = u"note";
            color = u"\033[36m";  // Cyan
            break;
        case Severity::Warning :
            sevStr = u"warning";
            color = u"\033[33m";  // Yellow
            break;
        case Severity::Error :
            sevStr = u"error";
            color = u"\033[31m";  // Red
            break;
        case Severity::Fatal :
            sevStr = u"fatal";
            color = u"\033[1;31m";  // Bold Red
            break;
        }

        std::cout << utf8::utf16to8(color) << utf8::utf16to8(sevStr) << "\033[0m";
        if (!diag.code.empty())
            std::cout << "[" << utf8::utf16to8(diag.code) << "]";
        std::cout << ": " << utf8::utf16to8(diag.message) << "\n";
        // Show source line
        std::cout << "  --> line " << diag.line << ":" << diag.column << "\n";
        // Extract and show the problematic line
        auto lines = splitLines(utf8::utf16to8(sourceCode_));
        if (diag.line > 0 && diag.line <= lines.size())
        {
            std::cout << "   |\n";
            std::cout << std::setw(3) << diag.line << " | " << lines[diag.line - 1] << "\n";
            std::cout << "   | " << std::string(diag.column - 1, ' ') << utf8::utf16to8(color)
                      << std::string(std::max(1, diag.length), '^') << "\033[0m\n";
        }
        // Show suggestions
        if (!diag.suggestions.empty())
        {
            std::cout << "\n  \033[1mHelp:\033[0m\n";
            for (const auto& sugg : diag.suggestions)
                std::cout << "    • " << utf8::utf16to8(sugg) << "\n";
        }
        // Show notes
        for (const auto& [noteLine, noteMsg] : diag.notes)
        {
            std::cout << "\n  \033[36mnote:\033[0m " << utf8::utf16to8(noteMsg) << "\n";
            std::cout << "  --> line " << noteLine << "\n";
        }
        std::cout << "\n";
    }
}

std::vector<std::string> DiagnosticEngine::splitLines(const std::string& text) const
{
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line))
        lines.push_back(line);
    return lines;
}

std::vector<LanguageServer::CompletionItem> LanguageServer::getCompletions(const string_type& source, Position pos)
{
    std::vector<CompletionItem> items;
    // Parse and get symbol table
    mylang::parser::Parser parser(source);
    auto ast = parser.parse();
    // Analyze and extract symbols
    // Add keywords
    for (const auto& kw : {u"اذا", u"طالما", u"لكل", u"عرف", u"return"})
    {
        CompletionItem item;
        item.label = kw;
        item.kind = 14;  // Keyword
        items.push_back(item);
    }
    return items;
}

LanguageServer::Hover LanguageServer::getHover(const string_type& source, LanguageServer::Position pos)
{
    Hover hover;
    hover.contents = u"Variable: x\nType: int";
    return hover;
}

LanguageServer::Position LanguageServer::getDefinition(const string_type& source, Position pos) { return {0, 0}; }

std::vector<LanguageServer::Range> LanguageServer::getReferences(const string_type& source, Position pos)
{
    return {};
}

std::unordered_map<string_type, string_type> LanguageServer::rename(
  const string_type& source, Position pos, const string_type& newName)
{
    return {};
}

void ParserProfiler::recordPhase(const string_type& phase, double ms) { timings.push_back({phase, ms}); }

void ParserProfiler::printReport() const
{
    std::cout << "\n╔═══════════════════════════════════════╗\n";
    std::cout << "║      Parser Performance Report        ║\n";
    std::cout << "╚═══════════════════════════════════════╝\n\n";
    double total = 0;
    for (const auto& t : timings)
        total += t.milliseconds;
    for (const auto& t : timings)
    {
        double percent = (t.milliseconds / total) * 100;
        std::cout << std::left << std::setw(20) << utf8::utf16to8(t.phase) << ": " << std::right << std::setw(8)
                  << std::fixed << std::setprecision(2) << t.milliseconds << "ms "
                  << "(" << std::setw(5) << percent << "%)\n";
    }
    // std::cout << "\n" << std::string(41, "─") << "\n";
    std::cout << std::left << std::setw(20) << "Total" << ": " << std::right << std::setw(8) << total << "ms\n";
}

}
}
#endif