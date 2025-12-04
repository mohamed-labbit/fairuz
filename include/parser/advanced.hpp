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
    std::u16string lastSource_;

    std::size_t hashRegion(const std::u16string& source, std::size_t start, std::size_t end);

   public:
    // Only reparse changed regions
    std::vector<ast::StmtPtr> parseIncremental(
      const std::u16string& newSource, const std::vector<std::size_t>& changedLines);
};

// ============================================================================
// PARALLEL PARSING - Parse multiple functions concurrently
// ============================================================================
class ParallelParser
{
   public:
    static std::vector<ast::StmtPtr> parseParallel(
      const std::vector<mylang::lex::tok::Token>& tokens, std::int32_t threadCount = 4);

   private:
    static std::vector<std::vector<mylang::lex::tok::Token>> splitIntoChunks(
      const std::vector<mylang::lex::tok::Token>& tokens);
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
        std::vector<std::shared_ptr<Type>> typeParams;  // For generics: List[Int]
        std::unordered_map<std::u16string, std::shared_ptr<Type>> fields;  // For classes

        // Function signature
        std::vector<std::shared_ptr<Type>> paramTypes;
        std::shared_ptr<Type> returnType;

        bool operator==(const Type& other) const;
        std::u16string toString() const;
    };

    // Hindley-Milner type inference
    class TypeInference
    {
       private:
        std::int32_t freshVarCounter = 0;
        std::unordered_map<std::u16string, std::shared_ptr<Type>> substitutions;

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
        enum class Op {
            LOAD_CONST,
            LOAD_VAR,
            STORE_VAR,
            ADD,
            SUB,
            MUL,
            DIV,
            JUMP,
            JUMP_IF_FALSE,
            CALL,
            RETURN,
            POP,
            DUP
        };

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

    std::u16string generateCPPStmt(const ast::Stmt* stmt);

    std::u16string generateCPPExpr(const ast::Expr* expr);
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
        std::u16string message;
        std::u16string code;  // Error code like E0001
        std::vector<std::u16string> suggestions;
        std::vector<std::pair<std::int32_t, std::u16string>> notes;  // Additional context
    };

   private:
    std::vector<Diagnostic> diagnostics_;
    std::u16string sourceCode_;

   public:
    void setSource(const std::u16string& source);

    void report(Severity sev,
      std::int32_t line,
      std::int32_t col,
      std::int32_t len,
      const std::u16string& msg,
      const std::u16string& code = u"");

    void addSuggestion(const std::u16string& suggestion);

    void addNote(std::int32_t line, const std::u16string& note);

    // Generate LSP-compatible diagnostics
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
        std::u16string label;
        std::u16string detail;
        std::u16string documentation;
        std::int32_t kind;  // Variable, Function, Class, etc.
    };

    struct Hover
    {
        std::u16string contents;
        Range range;
    };

    // Auto-completion at cursor position
    std::vector<CompletionItem> getCompletions(const std::u16string& source, Position pos);

    // Hover information
    Hover getHover(const std::u16string& source, Position pos);

    // Go to definition
    Position getDefinition(const std::u16string& source, Position pos);
    
    // Find all references
    std::vector<Range> getReferences(const std::u16string& source, Position pos);

    // Rename symbol
    std::unordered_map<std::u16string, std::u16string> rename(
      const std::u16string& source, Position pos, const std::u16string& newName);
};

// ============================================================================
// PERFORMANCE PROFILER for parser itself
// ============================================================================
class ParserProfiler
{
   private:
    struct Timing
    {
        std::u16string phase;
        double milliseconds;
    };

    std::vector<Timing> timings;

   public:
    void recordPhase(const std::u16string& phase, double ms);
    
    void printReport() const;
};

}
}
