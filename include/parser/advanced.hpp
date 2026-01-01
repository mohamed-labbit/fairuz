#pragma once


#include "../lex/lexer.hpp"
#include "../lex/token.hpp"
#include "ast/ast.hpp"
#include "parser.hpp"

#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace mylang {
namespace parser {

// ============================================================================
// INCREMENTAL PARSING - Parse only changed regions
// ============================================================================
class IncrementalParser
{
 private:
  struct ParseNode
  {
    std::size_t startPos, endPos;
    std::unique_ptr<ast::ASTNode> ast;
    std::size_t hash;
  };

  std::vector<ParseNode> cache_;
  string_type lastSource_;

  std::size_t hashRegion(const string_type& source, std::size_t start, std::size_t end);

 public:
  // Only reparse changed regions
  std::vector<ast::StmtPtr> parseIncremental(const string_type& newSource, const std::vector<std::size_t>& changedLines);
};

// ============================================================================
// PARALLEL PARSING - Parse multiple functions concurrently
// ============================================================================
class ParallelParser
{
 public:
  static std::vector<ast::StmtPtr> parseParallel(const std::vector<mylang::lex::tok::Token>& tokens, std::int32_t threadCount = 4);

 private:
  static std::vector<std::vector<mylang::lex::tok::Token>> splitIntoChunks(const std::vector<mylang::lex::tok::Token>& tokens);
};

// ============================================================================
// ADVANCED TYPE SYSTEM with Generics & Inference
// ============================================================================
class TypeSystem
{
 public:
  enum class BaseType { Int, Float, String, Bool, None, Any, List, Dict, Tuple, Set, Function, Class };

  struct Type
  {
    BaseType base;
    std::vector<std::shared_ptr<Type>> typeParams;                  // For generics: List[Int]
    std::unordered_map<string_type, std::shared_ptr<Type>> fields;  // For classes

    // Function signature
    std::vector<std::shared_ptr<Type>> paramTypes;
    std::shared_ptr<Type> returnType;

    bool operator==(const Type& other) const;
    string_type toString() const;
  };

  // Hindley-Milner type inference
  class TypeInference
  {
   private:
    std::int32_t freshVarCounter = 0;
    std::unordered_map<string_type, std::shared_ptr<Type>> substitutions;

    std::shared_ptr<Type> freshTypeVar();
    void unify(std::shared_ptr<Type> t1, std::shared_ptr<Type> t2);

   public:
    std::shared_ptr<Type> inferExpr(const ast::Expr* expr);
  };
};

// ============================================================================
// CODE GENERATION - Generate optimized bytecode or C++
// ============================================================================
class CodeGenerator
{
 public:
  enum class Target { Bytecode, CPP, LLVM };

  struct Bytecode
  {
    enum class Op { LOAD_CONST, LOAD_VAR, STORE_VAR, ADD, SUB, MUL, DIV, JUMP, JUMP_IF_FALSE, CALL, RETURN, POP, DUP };

    Op opcode;
    std::int32_t arg;
  };

  // Generate Python bytecode-like instructions
  std::vector<Bytecode> generateBytecode(const std::vector<ast::StmtPtr>& ast);

  // Generate C++ code
  std::string generateCPP(const std::vector<ast::StmtPtr>& ast);

 private:
  void generateStmt(const ast::Stmt* stmt, std::vector<Bytecode>& code);

  void generateExpr(const ast::Expr* expr, std::vector<Bytecode>& code);

  string_type generateCPPStmt(const ast::Stmt* stmt);

  string_type generateCPPExpr(const ast::Expr* expr);
};

// ============================================================================
// ADVANCED DIAGNOSTICS - IDE-quality error reporting
// ============================================================================
class DiagnosticEngine
{
 public:
  enum class Severity { Note, Warning, Error, Fatal };

  struct Diagnostic
  {
    Severity severity;
    std::int32_t line, column;
    std::int32_t length;
    string_type message;
    string_type code;  // Error code like E0001
    std::vector<string_type> suggestions;
    std::vector<std::pair<std::int32_t, string_type>> notes;  // Additional context
  };

 private:
  std::vector<Diagnostic> diagnostics_;
  string_type sourceCode_;

 public:
  void setSource(const string_type& source);

  void report(Severity sev, std::int32_t line, std::int32_t col, std::int32_t len, const string_type& msg, const string_type& code = u"");

  void addSuggestion(const string_type& suggestion);

  void addNote(std::int32_t line, const string_type& note);

  // Generate LSP-compatible diagnostic
  std::string toJSON() const;

  // Beautiful terminal output with colors
  void prettyPrint() const;

 private:
  std::vector<std::string> splitLines(const std::string& text) const;
};

// ============================================================================
// LANGUAGE SERVER PROTOCOL support
// ============================================================================
class LanguageServer
{
 public:
  struct Position
  {
    std::int32_t line, character;
  };

  struct Range
  {
    Position start, end;
  };

  struct CompletionItem
  {
    string_type label;
    string_type detail;
    string_type documentation;
    std::int32_t kind;  // Variable, Function, Class, etc.
  };

  struct Hover
  {
    string_type contents;
    Range range;
  };

  // Auto-completion at cursor position
  std::vector<CompletionItem> getCompletions(const string_type& source, Position pos);

  // Hover information
  Hover getHover(const string_type& source, Position pos);

  // Go to definition
  Position getDefinition(const string_type& source, Position pos);

  // Find all references
  std::vector<Range> getReferences(const string_type& source, Position pos);

  // Rename symbol
  std::unordered_map<string_type, string_type> rename(const string_type& source, Position pos, const string_type& newName);
};

// ============================================================================
// PERFORMANCE PROFILER for parser itself
// ============================================================================
class ParserProfiler
{
 private:
  struct Timing
  {
    string_type phase;
    double milliseconds;
  };

  std::vector<Timing> timings;

 public:
  void recordPhase(const string_type& phase, double ms);

  void printReport() const;
};

}
}