#include "../../include/parser/advanced.hpp"


namespace mylang {
namespace parser {

std::vector<ast::StmtPtr> ParallelParser::parseParallel(const std::vector<mylang::lex::tok::Token>& tokens, std::int32_t threadCount)
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
    if (auto stmt = future.get()) result.push_back(std::move(stmt));
  return result;
}

std::vector<std::vector<mylang::lex::tok::Token>> ParallelParser::splitIntoChunks(const std::vector<mylang::lex::tok::Token>& tokens)
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
  if (!current.empty()) chunks.push_back(current);
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
  if (!stmt) return;

  switch (stmt->getKind())
  {
  case ast::Stmt::Kind::ASSIGNMENT : {
    auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
    generateExpr(assign->getValue(), code);
    code.push_back({Bytecode::Op::STORE_VAR, 0});  // Store to variable
    break;
  }
  case ast::Stmt::Kind::EXPR : {
    auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
    generateExpr(exprStmt->getExpr(), code);
    code.push_back({Bytecode::Op::POP, 0});
    break;
  }
  case ast::Stmt::Kind::RETURN : {
    auto* ret = static_cast<const ast::ReturnStmt*>(stmt);
    generateExpr(ret->getValue(), code);
    code.push_back({Bytecode::Op::RETURN, 0});
    break;
  }
  default : break;
  }
}

void CodeGenerator::generateExpr(const ast::Expr* expr, std::vector<Bytecode>& code)
{
  if (!expr) return;

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : code.push_back({Bytecode::Op::LOAD_CONST, 0}); break;
  case ast::Expr::Kind::NAME : code.push_back({Bytecode::Op::LOAD_VAR, 0}); break;
  case ast::Expr::Kind::BINARY : {
    const ast::BinaryExpr* bin = static_cast<const ast::BinaryExpr*>(expr);
    generateExpr(bin->getLeft(), code);
    generateExpr(bin->getRight(), code);
    if (bin->getOperator() == lex::tok::TokenType::OP_PLUS) { code.push_back({Bytecode::Op::ADD, 0}); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_MINUS) { code.push_back({Bytecode::Op::SUB, 0}); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_STAR) { code.push_back({Bytecode::Op::MUL, 0}); }
    else if (bin->getOperator() == lex::tok::TokenType::OP_SLASH) { code.push_back({Bytecode::Op::DIV, 0}); }
    break;
  }
  default : break;
  }
}

string_type CodeGenerator::generateCPPStmt(const ast::Stmt* stmt)
{
  if (!stmt) return u"";

  switch (stmt->getKind())
  {
  case ast::Stmt::Kind::ASSIGNMENT : {
    auto* assign = static_cast<const ast::AssignmentStmt*>(stmt);
    return u"auto " + assign->getTarget()->getValue()->getStr() + u" = " + generateCPPExpr(assign->getValue()) + u";\n";
  }
  case ast::Stmt::Kind::EXPR : {
    auto* exprStmt = static_cast<const ast::ExprStmt*>(stmt);
    return generateCPPExpr(exprStmt->getExpr()) + u";\n";
  }
  case ast::Stmt::Kind::IF : {
    auto* ifStmt = static_cast<const ast::IfStmt*>(stmt);
    string_type result = u"if (" + generateCPPExpr(ifStmt->getCondition()) + u") {\n";
    for (const auto* s : ifStmt->getThenBlock()->getStatements())
      result += u"  " + generateCPPStmt(s);
    result += u"}\n";
    return result;
  }
  case ast::Stmt::Kind::FUNC : {
    auto* func = static_cast<const ast::FunctionDef*>(stmt);
    string_type result = u"auto " + func->getName()->getValue()->getStr() + u"(";
    auto params = func->getParameters();
    for (std::size_t i = 0; i < params.size(); i++)
    {
      result += u"auto " + params[i]->getStr();
      if (i + 1 < params.size()) result += u", ";
    }
    result += u") {\n";
    auto body = func->getBody();
    for (const auto& s : body->getStatements())
      result += u"  " + generateCPPStmt(s);
    result += u"}\n";
    return result;
  }
  default : return u"";
  }

  return u"";
}

string_type CodeGenerator::generateCPPExpr(const ast::Expr* expr)
{
  if (!expr) return u"";

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : {
    auto* lit = static_cast<const ast::LiteralExpr*>(expr);
    return lit->getValue()->getStr();
  }
  case ast::Expr::Kind::NAME : {
    auto* name_expr = static_cast<const ast::NameExpr*>(expr);
    return name_expr->getValue()->getStr();
  }
  case ast::Expr::Kind::BINARY : {
    auto* bin = static_cast<const ast::BinaryExpr*>(expr);
    return u"(" + generateCPPExpr(bin->getLeft()) + u" " + lex::tok::to_string(bin->getOperator()) + u" " + generateCPPExpr(bin->getRight()) + u")";
  }
  case ast::Expr::Kind::CALL : {
    auto* call = static_cast<const ast::CallExpr*>(expr);
    string_type result = generateCPPExpr(call->getCallee()) + u"(";
    for (std::size_t i = 0; i < call->getArgs().size(); i++)
    {
      result += generateCPPExpr(call->getArgs()[i]);
      if (i + 1 < call->getArgs().size()) result += u", ";
    }
    result += u")";
    return result;
  }
  default : return u"";
  }

  return u"";
}

std::size_t IncrementalParser::hashRegion(const string_type& source, std::size_t start, std::size_t end)
{
  std::hash<string_type> hasher;
  return hasher(source.substr(start, end - start));
}

// Only reparse changed regions
std::vector<ast::StmtPtr> IncrementalParser::parseIncremental(const string_type& newSource, const std::vector<std::size_t>& changedLines)
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
  case BaseType::Int : return u"int";
  case BaseType::Float : return u"float";
  case BaseType::String : return u"str";
  case BaseType::Bool : return u"bool";
  case BaseType::None : return u"None";
  case BaseType::List :
    if (!typeParams.empty()) return u"List[" + typeParams[0]->toString() + u"]";
    return u"List";
  case BaseType::Dict :
    if (typeParams.size() >= 2) return u"Dict[" + typeParams[0]->toString() + u", " + typeParams[1]->toString() + u"]";
    return u"Dict";
  default : return u"Any";
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
  if (*t1 == *t2) return;

  // Type variable substitution
  if (t1->base == BaseType::Any) { *t1 = *t2; }
  else if (t2->base == BaseType::Any) { *t2 = *t1; }
  else if (t1->base == BaseType::List && t2->base == BaseType::List)
  {
    if (!t1->typeParams.empty() && !t2->typeParams.empty()) unify(t1->typeParams[0], t2->typeParams[0]);
  }
  else
  {
    diagnostic::engine.panic("Type mismatch: " + utf8::utf16to8(t1->toString()) + " vs " + utf8::utf16to8(t2->toString()));
  }
}

std::shared_ptr<TypeSystem::Type> TypeSystem::TypeInference::inferExpr(const ast::Expr* expr)
{
  if (!expr) return std::make_shared<Type>();

  switch (expr->getKind())
  {
  case ast::Expr::Kind::LITERAL : {
    auto* lit = static_cast<const ast::LiteralExpr*>(expr);
    auto t = std::make_shared<Type>();
    switch (lit->getType())
    {
    case ast::LiteralExpr::Type::NUMBER : t->base = lit->getValue()->getStr().find('.') != std::string::npos ? BaseType::Float : BaseType::Int; break;
    case ast::LiteralExpr::Type::STRING : t->base = BaseType::String; break;
    case ast::LiteralExpr::Type::BOOLEAN : t->base = BaseType::Bool; break;
    case ast::LiteralExpr::Type::NONE : t->base = BaseType::None; break;
    }
    return t;
  }
  case ast::Expr::Kind::BINARY : {
    auto* bin = static_cast<const ast::BinaryExpr*>(expr);
    auto leftType = inferExpr(bin->getLeft());
    auto rightType = inferExpr(bin->getRight());
    unify(leftType, rightType);
    // Result type based on operator
    if (bin->getOperator() == lex::tok::TokenType::OP_PLUS || bin->getOperator() == lex::tok::TokenType::OP_MINUS
        || bin->getOperator() == lex::tok::TokenType::OP_STAR || bin->getOperator() == lex::tok::TokenType::OP_SLASH)
    {
      return leftType;
    }
    else if (bin->getOperator() == lex::tok::TokenType::OP_EQ || bin->getOperator() == lex::tok::TokenType::OP_NEQ
             || bin->getOperator() == lex::tok::TokenType::OP_LT || bin->getOperator() == lex::tok::TokenType::OP_GT)
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
    if (!list->getElements().empty())
    {
      auto elemType = inferExpr(list->getElements()[0]);
      t->typeParams.push_back(elemType);
    }
    return t;
  }
  default : return freshTypeVar();
  }

  return freshTypeVar();
}

/*
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
*/

LanguageServer::Hover LanguageServer::getHover(const string_type& source, LanguageServer::Position pos)
{
  Hover hover;
  hover.contents = u"Variable: x\nType: int";
  return hover;
}

LanguageServer::Position LanguageServer::getDefinition(const string_type& source, Position pos) { return {0, 0}; }

std::vector<LanguageServer::Range> LanguageServer::getReferences(const string_type& source, Position pos) { return {}; }

std::unordered_map<string_type, string_type> LanguageServer::rename(const string_type& source, Position pos, const string_type& newName)
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
    std::cout << std::left << std::setw(20) << utf8::utf16to8(t.phase) << ": " << std::right << std::setw(8) << std::fixed << std::setprecision(2)
              << t.milliseconds << "ms "
              << "(" << std::setw(5) << percent << "%)\n";
  }
  // std::cout << "\n" << std::string(41, "─") << "\n";
  std::cout << std::left << std::setw(20) << "Total" << ": " << std::right << std::setw(8) << total << "ms\n";
}

}
}