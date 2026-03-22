#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "error.hpp"
#include "lexer.hpp"

#include <unordered_set>

namespace mylang {
namespace parser {

using namespace ast;

class SymbolTable {
public:
    enum class SymbolType { VARIABLE,
        FUNCTION,
        CLASS,
        MODULE,
        UNKNOWN };

    enum class DataType_t { INTEGER,
        FLOAT,
        STRING,
        BOOLEAN,
        LIST,
        DICT,
        TUPLE,
        NONE,
        FUNCTION,
        ANY,
        UNKNOWN };

    struct Symbol {
        StringRef name;
        SymbolType symbolType;
        DataType_t dataType;
        bool isUsed = false;
        int32_t definitionLine = 0;
        Array<int32_t> usageLines;
        Array<DataType_t> paramTypes;
        DataType_t returnType = DataType_t::UNKNOWN;
        std::unordered_set<DataType_t> possibleTypes;
    };

    SymbolTable* Parent_ = nullptr;

private:
    std::unordered_map<StringRef, Symbol, StringRefHash, StringRefEqual> Symbols_;
    Array<std::unique_ptr<SymbolTable>> Children_;
    unsigned int ScopeLevel_ { 0 };

public:
    explicit SymbolTable(SymbolTable* p = nullptr, int32_t level = 0);

    void define(StringRef const& name, Symbol symbol);
    Symbol* lookup(StringRef const& name);
    Symbol* lookupLocal(StringRef const& name);
    bool isDefined(StringRef const& name) const;
    void markUsed(StringRef const& name, int32_t line);
    SymbolTable* createChild();
    Array<Symbol*> getUnusedSymbols();
    std::unordered_map<StringRef, Symbol, StringRefHash, StringRefEqual> const& getSymbols() const;
};

class SemanticAnalyzer {
public:
    struct Issue {
        enum class Severity { ERROR,
            WARNING,
            INFO };

        Severity severity;
        StringRef message;
        int32_t line;
        StringRef suggestion;
    };

private:
    SymbolTable* CurrentScope_;
    std::unique_ptr<SymbolTable> GlobalScope_;
    Array<Issue> Issues_;

public:
    SemanticAnalyzer();
    SymbolTable::DataType_t inferType(Expr const* expr);
    void reportIssue(Issue::Severity sev, StringRef const& msg, int32_t line, StringRef const& sugg = "");
    void analyzeExpr(Expr const* expr);
    void analyzeStmt(Stmt const* stmt);
    void analyze(Array<Stmt*> const& Statements_);
    Array<Issue> const& getIssues() const;
    SymbolTable const* getGlobalScope() const;
    SymbolTable const* getCurrentScope() const;
    void printReport() const;
};

class ParseError : public std::runtime_error {
public:
    int32_t Line_, Column_;
    StringRef Context_;
    Array<StringRef> Suggestions_;

    ParseError(StringRef const& msg, unsigned int l, unsigned int c, StringRef ctx = "", Array<StringRef> sugg = { })
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

    explicit Parser(Array<tok::Token> seq, std::optional<size_t> s = std::nullopt);

    Array<Stmt*> parseProgram();

    ErrorOr<Stmt*> parseStatement();
    ErrorOr<Stmt*> parseExpressionStmt();
    ErrorOr<Stmt*> parseIfStmt();
    ErrorOr<Stmt*> parseWhileStmt();
    ErrorOr<Stmt*> parseReturnStmt();
    ErrorOr<Stmt*> parseFunctionDef();
    ErrorOr<Expr*> parseParenthesizedExprContent();
    ErrorOr<Expr*> parseCallExpr(Expr* callee);
    ErrorOr<Expr*> parsePrimary();
    ErrorOr<Expr*> parseUnary();
    ErrorOr<Expr*> parseParenthesizedExpr();
    ErrorOr<Expr*> parseExpression();
    ErrorOr<Expr*> parseAssignmentExpr();
    ErrorOr<Expr*> parseListLiteral();
    ErrorOr<Expr*> parseConditionalExpr();
    ErrorOr<Expr*> parseLogicalExpr();
    ErrorOr<Expr*> parseLogicalExprPrecedence(unsigned int min_precedence);
    ErrorOr<Expr*> parseBinaryExprPrecedence(unsigned int min_precedence);
    ErrorOr<Expr*> parseComparisonExpr();
    ErrorOr<Expr*> parseBinaryExpr();
    ErrorOr<Expr*> parseUnaryExpr();
    ErrorOr<Expr*> parsePrimaryExpr();
    ErrorOr<Expr*> parsePostfixExpr();
    ErrorOr<Expr*> parse();
    ErrorOr<Expr*> parseFunctionArguments();
    ErrorOr<Expr*> parseParametersList();
    ErrorOr<Stmt*> parseBlock();
    ErrorOr<Stmt*> parseIndentedBlock();

    bool weDone() const;
    bool check(tok::TokenType type);

    tok::Token const* currentToken();

private:
    lex::Lexer Lexer_;
    SymbolTable SymTable_;
    SemanticAnalyzer Sema_;

    tok::Token const* peek(size_t offset = 1) { return Lexer_.peek(offset); }
    tok::Token const* advance() { return Lexer_.next(); }

    bool match(tok::TokenType const type);

    MY_NODISCARD
    bool consume(tok::TokenType type)
    {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    Error reportError(diagnostic::errc::parser::Code err_code);

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

public:
    std::optional<double> evaluateConstant(Expr const* expr);

    Expr* optimizeConstantFolding(Expr* expr);
    Stmt* eliminateDeadCode(Stmt* stmt);

    class CSEPass {
    private:
        std::unordered_map<StringRef, StringRef, StringRefHash, StringRefEqual> ExprCache_;
        int32_t TempCounter_ = 0;

    public:
        StringRef exprToString(Expr const* expr);
        StringRef getTempVar();

        std::optional<StringRef> findCSE(Expr const* expr);

        void recordExpr(Expr const* expr, StringRef const& var);
    };

    bool isLoopInvariant(Expr const* expr, std::unordered_set<StringRef, StringRefHash, StringRefEqual> const& loopVars);
    Array<Stmt*> optimize(Array<Stmt*> statements, int32_t level = 2);
    OptimizationStats const& getStats() const;
    void printStats() const;
};

} // namespace parser
} // namespace mylang

#endif // PARSER_HPP
