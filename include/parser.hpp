#ifndef PARSER_HPP
#define PARSER_HPP

#include "ast.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include "table.hpp"

#include <unordered_set>

namespace fairuz::parser {

class Fa_SymbolTable {
public:
    enum class SymbolType {
        VARIABLE,
        FUNCTION,
        CLASS,
        MODULE,
        UNKNOWN
    }; // enum SymbolType

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
    }; // enum DataType_t

    struct Symbol {
        Fa_StringRef name;
        SymbolType symbolType;
        DataType_t dataType;
        bool isUsed = false;
        i32 definitionLine = 0;
        Fa_Array<i32> usageLines;
        Fa_Array<DataType_t> paramTypes;
        DataType_t returnType = DataType_t::UNKNOWN;
        std::unordered_set<DataType_t> possibleTypes;
    }; // struct Symbol

    Fa_SymbolTable* Parent_ = nullptr;

private:
    Fa_HashTable<Fa_StringRef, Symbol, Fa_StringRefHash, Fa_StringRefEqual> Symbols_;
    Fa_Array<Fa_SymbolTable*> Children_;
    unsigned int ScopeLevel_ { 0 };

public:
    explicit Fa_SymbolTable(Fa_SymbolTable* p = nullptr, i32 level = 0);

    void define(Fa_StringRef const& name, Symbol symbol);
    Symbol* lookup(Fa_StringRef const& name);
    Symbol* lookupLocal(Fa_StringRef const& name);
    bool isDefined(Fa_StringRef const& name) const;
    void markUsed(Fa_StringRef const& name, i32 line);
    Fa_SymbolTable* createChild();
    Fa_Array<Symbol*> getUnusedSymbols();
    Fa_HashTable<Fa_StringRef, Symbol, Fa_StringRefHash, Fa_StringRefEqual> const& getSymbols() const;
}; // class Fa_SymbolTable

class Fa_SemanticAnalyzer {
public:
    struct Issue {
        enum class Severity {
            ERROR,
            WARNING,
            INFO
        }; // enum Severity

        Severity severity;
        Fa_StringRef message;
        i32 line;
        Fa_StringRef suggestion;
    }; // struct Issue

private:
    Fa_SymbolTable* CurrentScope_;
    Fa_SymbolTable* GlobalScope_;
    Fa_Array<Issue> Issues_;

public:
    Fa_SemanticAnalyzer();
    Fa_SymbolTable::DataType_t inferType(AST::Fa_Expr const* expr);
    void reportIssue(Issue::Severity sev, Fa_StringRef msg, i32 line, Fa_StringRef const& sugg = "");
    void analyzeFa_Expr(AST::Fa_Expr const* expr);
    void analyzeStmt(AST::Fa_Stmt const* stmt);
    void analyze(Fa_Array<AST::Fa_Stmt*> const& statements);
    Fa_Array<Issue> const& getIssues() const;
    Fa_SymbolTable const* getGlobalScope() const;
    Fa_SymbolTable const* getCurrentScope() const;
    void printReport() const;
}; // class Fa_SemanticAnalyzer

class Fa_ParseError : public std::runtime_error {
public:
    i32 Line_, Column_;
    Fa_StringRef Context_;
    Fa_Array<Fa_StringRef> Suggestions_;

    Fa_ParseError(Fa_StringRef const& msg, unsigned int l, unsigned int c, Fa_StringRef ctx = "", Fa_Array<Fa_StringRef> sugg = { })
        : Line_(l)
        , Column_(c)
        , Context_(ctx)
        , Suggestions_(sugg)
        , std::runtime_error(msg.data())
    {
    }

    Fa_StringRef format() const
    {
        std::stringstream ss;
        ss << "Line " << Line_ << ":" << Column_ << " - " << what() << "\n";

        if (!Context_.empty()) {
            ss << "  | " << Context_ << "\n";
            ss << "  | " << std::string(Column_ - 1, ' ') << "^\n";
        }

        if (!Suggestions_.empty()) {
            ss << "Suggestions:\n";
            for (Fa_StringRef const& s : Suggestions_)
                ss << "  - " << s << "\n";
        }

        return Fa_StringRef(ss.str().data());
    }
}; // class ParseError

class Fa_Parser {
public:
    explicit Fa_Parser() = default;

    explicit Fa_Parser(lex::Fa_FileManager* fm)
        : Lexer_(fm)
    {
        if (!fm)
            diagnostic::panic(diagnostic::errc::general::Code::INTERNAL_ERROR, "parser received a null Fa_FileManager");

        Lexer_.next();
    }

    explicit Fa_Parser(Fa_Array<tok::Fa_Token> seq, std::optional<size_t> s = std::nullopt);

    Fa_Array<AST::Fa_Stmt*> parseProgram();

    Fa_ErrorOr<AST::Fa_Stmt*> parseStatement();
    Fa_ErrorOr<AST::Fa_Stmt*> parseExpressionStmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parseIfStmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parseWhileStmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parseReturnStmt();
    Fa_ErrorOr<AST::Fa_Stmt*> parseFunctionDef();
    Fa_ErrorOr<AST::Fa_Expr*> parseParenthesizedExprContent();
    Fa_ErrorOr<AST::Fa_Expr*> parseCallExpr(AST::Fa_Expr* callee);
    Fa_ErrorOr<AST::Fa_Expr*> parsePrimary();
    Fa_ErrorOr<AST::Fa_Expr*> parseUnary();
    Fa_ErrorOr<AST::Fa_Expr*> parseParenthesizedExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parseExpression();
    Fa_ErrorOr<AST::Fa_Expr*> parseAssignmentExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parseListLiteral();
    Fa_ErrorOr<AST::Fa_Expr*> parseConditionalExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parseLogicalExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parseLogicalExprPrecedence(unsigned int min_precedence);
    Fa_ErrorOr<AST::Fa_Expr*> parseBinaryExprPrecedence(unsigned int min_precedence);
    Fa_ErrorOr<AST::Fa_Expr*> parseComparisonExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parseBinaryExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parseUnaryExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parsePrimaryExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parsePostfixExpr();
    Fa_ErrorOr<AST::Fa_Expr*> parse();
    Fa_ErrorOr<AST::Fa_Expr*> parseFunctionArguments();
    Fa_ErrorOr<AST::Fa_Expr*> parseParametersList();
    Fa_ErrorOr<AST::Fa_Stmt*> parseBlock();
    Fa_ErrorOr<AST::Fa_Stmt*> parseIndentedBlock();

    bool weDone() const;
    bool check(tok::Fa_TokenType type);

    tok::Fa_Token const* currentToken();

private:
    lex::Fa_Lexer Lexer_;
    Fa_SymbolTable SymTable_;
    Fa_SemanticAnalyzer Sema_;

    tok::Fa_Token const* peek(size_t offset = 1) { return Lexer_.peek(offset); }
    tok::Fa_Token const* advance() { return Lexer_.next(); }

    bool match(tok::Fa_TokenType const type);

    [[nodiscard]]
    bool consume(tok::Fa_TokenType type)
    {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    Fa_Error reportError(diagnostic::errc::parser::Code err_code);

    void skipNewlines()
    {
        while (match(tok::Fa_TokenType::NEWLINE))
            ;
    }

    void enterScope();
    void synchronize();
}; // class Fa_Parser

} // namespace fairuz::parser

#endif // PARSER_HPP
