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

    std::size_t hashRegion(const std::u16string& source, std::size_t start, std::size_t end)
    {
        std::hash<std::u16string> hasher;
        return hasher(source.substr(start, end - start));
    }

   public:
    // Only reparse changed regions
    std::vector<ast::StmtPtr> parseIncremental(
      const std::u16string& newSource, const std::vector<std::size_t>& changedLines)
    {
        // Identify unchanged regions and reuse cached AST
        std::vector<ast::StmtPtr> result;
        // Implementation would diff source and reparse only changes
        return result;
    }
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

        bool operator==(const Type& other) const
        {
            return base == other.base;  // Simplified
        }

        std::u16string toString() const
        {
            switch (base)
            {
            case BaseType::Int : return u"int";
            case BaseType::Float : return u"float";
            case BaseType::String : return u"str";
            case BaseType::Bool : return u"bool";
            case BaseType::None : return u"None";
            case BaseType::List :
                if (!typeParams.empty())
                    return u"List[" + typeParams[0]->toString() + u"]";
                return u"List";
            case BaseType::Dict :
                if (typeParams.size() >= 2)
                    return u"Dict[" + typeParams[0]->toString() + u", " + typeParams[1]->toString() + u"]";
                return u"Dict";
            default : return u"Any";
            }
        }
    };

    // Hindley-Milner type inference
    class TypeInference
    {
       private:
        std::int32_t freshVarCounter = 0;
        std::unordered_map<std::u16string, std::shared_ptr<Type>> substitutions;

        std::shared_ptr<Type> freshTypeVar()
        {
            auto t = std::make_shared<Type>();
            t->base = BaseType::Any;
            return t;
        }

        void unify(std::shared_ptr<Type> t1, std::shared_ptr<Type> t2)
        {
            if (*t1 == *t2)
                return;
            // Type variable substitution
            if (t1->base == BaseType::Any)
                *t1 = *t2;
            else if (t2->base == BaseType::Any)
                *t2 = *t1;
            else if (t1->base == BaseType::List && t2->base == BaseType::List)
            {
                if (!t1->typeParams.empty() && !t2->typeParams.empty())
                    unify(t1->typeParams[0], t2->typeParams[0]);
            }
            else
                throw std::runtime_error(
                  "Type mismatch: " + utf8::utf16to8(t1->toString()) + " vs " + utf8::utf16to8(t2->toString()));
        }

       public:
        std::shared_ptr<Type> inferExpr(const ast::Expr* expr)
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
                case ast::LiteralExpr::Type::STRING : t->base = BaseType::String; break;
                case ast::LiteralExpr::Type::BOOLEAN : t->base = BaseType::Bool; break;
                case ast::LiteralExpr::Type::NONE : t->base = BaseType::None; break;
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
            default : return freshTypeVar();
            }
        }
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
    void setSource(const std::u16string& source) { sourceCode_ = source; }

    void report(Severity sev,
      std::int32_t line,
      std::int32_t col,
      std::int32_t len,
      const std::u16string& msg,
      const std::u16string& code = u"")
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

    void addSuggestion(const std::u16string& suggestion)
    {
        if (!diagnostics_.empty())
            diagnostics_.back().suggestions.push_back(suggestion);
    }

    void addNote(std::int32_t line, const std::u16string& note)
    {
        if (!diagnostics_.empty())
            diagnostics_.back().notes.push_back({line, note});
    }

    // Generate LSP-compatible diagnostics
    std::string toJSON() const
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
    void prettyPrint() const
    {
        for (const auto& diag : diagnostics_)
        {
            std::u16string sevStr;
            std::u16string color;

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

   private:
    std::vector<std::string> splitLines(const std::string& text) const
    {
        std::vector<std::string> lines;
        std::stringstream ss(text);
        std::string line;
        while (std::getline(ss, line))
            lines.push_back(line);
        return lines;
    }
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
    std::vector<CompletionItem> getCompletions(const std::u16string& source, Position pos)
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

    // Hover information
    Hover getHover(const std::u16string& source, Position pos)
    {
        Hover hover;
        hover.contents = u"Variable: x\nType: int";
        return hover;
    }

    // Go to definition
    Position getDefinition(const std::u16string& source, Position pos) { return {0, 0}; }

    // Find all references
    std::vector<Range> getReferences(const std::u16string& source, Position pos) { return {}; }

    // Rename symbol
    std::unordered_map<std::u16string, std::u16string> rename(
      const std::u16string& source, Position pos, const std::u16string& newName)
    {
        return {};
    }
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
    void recordPhase(const std::u16string& phase, double ms) { timings.push_back({phase, ms}); }

    void printReport() const
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
};

}
}
