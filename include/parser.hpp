#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "lexer.hpp"
#include "token.hpp"

#include <optional>
#include <sstream>

namespace mylang {
namespace parser {

class SymbolTable {
public:
    enum class SymbolType {
        VARIABLE,
        FUNCTION,
        CLASS,
        MODULE,
        UNKNOWN
    };

    enum class DataType_t {
        INTEGER,
        FLOAT,
        STRING,
        BOOLEAN,
        LIST,
        DICT,
        TUPLE,
        NONE,
        FUNCTION,
        ANY,
        UNKNOWN
    };

    struct Symbol {
        StringRef name;
        SymbolType symbolType;
        DataType_t dataType;
        bool isUsed = false;
        int32_t definitionLine = 0;
        std::vector<int32_t> usageLines;
        // for functions
        std::vector<DataType_t> paramTypes;
        DataType_t returnType = DataType_t::UNKNOWN;
        // for type inference
        std::unordered_set<DataType_t> possibleTypes;
    };

    SymbolTable* Parent_ = nullptr;

private:
    std::unordered_map<StringRef, Symbol, StringRefHash, StringRefEqual> Symbols_;
    std::vector<std::unique_ptr<SymbolTable>> Children_;
    unsigned int ScopeLevel_ { 0 };

public:
    explicit SymbolTable(SymbolTable* p = nullptr, int32_t level = 0);

    void define(StringRef const& name, Symbol symbol);

    Symbol* lookup(StringRef const& name);

    Symbol* lookupLocal(StringRef const& name);

    bool isDefined(StringRef const& name) const;

    void markUsed(StringRef const& name, int32_t line);

    SymbolTable* createChild();

    std::vector<Symbol*> getUnusedSymbols();

    std::unordered_map<StringRef, Symbol, StringRefHash, StringRefEqual> const& getSymbols() const;
};

// semantic analyzer with type inference and optimization hints
class SemanticAnalyzer {
public:
    struct Issue {
        enum class Severity {
            ERROR,
            WARNING,
            INFO
        };

        Severity severity;
        StringRef message;
        int32_t line;
        StringRef suggestion;
    };

private:
    SymbolTable* CurrentScope_;
    std::unique_ptr<SymbolTable> GlobalScope_;
    std::vector<Issue> Issues_;

public:
    SymbolTable::DataType_t inferType(ast::Expr const* expr);

    void reportIssue(Issue::Severity sev, StringRef const& msg, int32_t line, StringRef const& sugg = "");

    void analyzeExpr(ast::Expr const* expr);
    void analyzeStmt(ast::Stmt const* stmt);

    SemanticAnalyzer();

    void analyze(std::vector<ast::Stmt*> const& Statements_);

    std::vector<Issue> const& getIssues() const;

    SymbolTable const* getGlobalScope() const;

    void printReport() const;
};

class ParseError : public std::runtime_error {
public:
    int32_t Line_, Column_;
    StringRef Context_;
    std::vector<StringRef> Suggestions_;

    ParseError(StringRef const& msg, unsigned int l, unsigned int c, StringRef ctx = "", std::vector<StringRef> sugg = { })
        : Line_(l)
        , Column_(c)
        , Context_(ctx)
        , Suggestions_(sugg)
        , std::runtime_error(msg.data())
    {
    }

    StringRef format() const
    {
        std::stringstream ss;
        ss << "Line " << Line_ << ":" << Column_ << " - " << what() << "\n";

        if (!Context_.empty()) {
            ss << "  | " << Context_ << "\n";
            ss << "  | " << std::string(Column_ - 1, ' ') << "^\n";
        }

        if (!Suggestions_.empty()) {
            ss << "Suggestions:\n";
            for (StringRef const& s : Suggestions_)
                ss << "  - " << s << "\n";
        }

        return StringRef(ss.str().data());
    }
};

class Parser {
public:
    explicit Parser() = default;

    explicit Parser(lex::FileManager* fm)
        : Lexer_(fm)
    {
        if (!fm)
            diagnostic::panic("file_manager is NULL!");

        Lexer_.next();
    }

    explicit Parser(std::vector<tok::Token> seq, std::optional<size_t> s = std::nullopt);

    std::vector<ast::Stmt*> parseProgram();

    ast::Stmt* parseStatement();
    ast::Stmt* parseExpressionStmt();
    ast::Stmt* parseIfStmt();
    ast::Stmt* parseWhileStmt();
    ast::Stmt* parseReturnStmt();
    ast::Stmt* parseFunctionDef();
    ast::Expr* parseParenthesizedExprContent();
    ast::Expr* parseCallExpr(ast::Expr* callee);
    ast::Expr* parsePrimary();
    ast::Expr* parseUnary();
    ast::Expr* parseParenthesizedExpr();
    ast::Expr* parseExpression();
    ast::Expr* parseAssignmentExpr();
    ast::Expr* parseListLiteral();
    ast::Expr* parseConditionalExpr();
    ast::Expr* parseLogicalExpr();
    ast::Expr* parseLogicalExprPrecedence(unsigned int min_precedence);
    ast::Expr* parseBinaryExprPrecedence(unsigned int min_precedence);
    ast::Expr* parseComparisonExpr();
    ast::Expr* parseBinaryExpr();
    ast::Expr* parseUnaryExpr();
    ast::Expr* parsePrimaryExpr();
    ast::Expr* parsePostfixExpr();
    ast::Expr* parse();
    ast::ListExpr* parseFunctionArguments();
    ast::ListExpr* parseParametersList();
    ast::BlockStmt* parseBlock();
    ast::BlockStmt* parseIndentedBlock();

    bool weDone() const;
    bool check(tok::TokenType type);

    tok::Token const* currentToken();

private:
    lex::Lexer Lexer_;
    SymbolTable SymTable_;
    SemanticAnalyzer Sema_;

    bool Expecting_ { false };

    tok::Token const* peek(size_t offset = 1) { return Lexer_.peek(offset); }
    tok::Token const* advance() { return Lexer_.next(); }

    bool match(tok::TokenType const type);

    MY_NODISCARD
    bool consume(tok::TokenType type, StringRef const& msg)
    {
        if (check(type)) {
            advance();
            return true;
        }
        diagnostic::emit(msg.data(), diagnostic::Severity::ERROR);
        return false;
    }

    void skipNewlines()
    {
        while (match(tok::TokenType::NEWLINE))
            ;
    }

    void enterScope();
    void synchronize();
};

class ASTOptimizer {
public:
    struct OptimizationStats {
        size_t ConstantFolds { 0 };
        size_t DeadCodeEliminations { 0 };
        size_t CommonSubexprEliminations { 0 };
        size_t LoopInvariants { 0 };
        size_t StrengthReductions { 0 };
    };

private:
    OptimizationStats Stats_;

    // Constant folding evaluator

public:
    std::optional<double> evaluateConstant(ast::Expr const* expr);

    ast::Expr* optimizeConstantFolding(ast::Expr* expr);
    ast::Stmt* eliminateDeadCode(ast::Stmt* stmt);

    class CSEPass {
    private:
        std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> ExprCache_;
        int32_t TempCounter_ = 0;

    public:
        StringRef exprToString(ast::Expr const* expr);
        StringRef getTempVar();

        std::optional<StringRef> findCSE(ast::Expr const* expr);

        void recordExpr(ast::Expr const* expr, StringRef const& var);
    };

    // Pass 4: Loop Invariant Code Motion
    bool isLoopInvariant(ast::Expr const* expr, std::unordered_set<StringRef, StringRefHash, StringRefEqual> const& loopVars);

    // Main optimization pipeline
    std::vector<ast::Stmt*> optimize(std::vector<ast::Stmt*> statements, int32_t level = 2);

    OptimizationStats const& getStats() const;

    void printStats() const;
};

} // namespace parser
} // namespace mylang

#endif // PARSER_HPP
