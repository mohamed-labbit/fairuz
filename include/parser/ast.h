ww#pragma once

#include "lex/lexer.h"
#include "lex/token.h"


struct ASTNode
{
   private:
    Token token_;

   public:
    virtual ~ASTNode() = default;
};

struct NumberNode: public ASTNode
{
    double value_;

    NumberNode() = default;

    NumberNode(double v) :
        value_(v) {}
};

struct BinaryOpNode: public ASTNode
{
    std::wstring op_{};
    ASTNode*     left_{nullptr};
    ASTNode*     right_{nullptr};

    BinaryOpNode() = default;

    BinaryOpNode(const std::wstring& o, ASTNode* l, ASTNode* r) :
        op_(o),
        left_(l),
        right_(r) {}
};

struct NonTerminalNode: public ASTNode
{
    std::wstring value_;

    NonTerminalNode() = default;

    NonTerminalNode(std::wstring v) :
        value_(v) {}
};

struct SymbolNode: public ASTNode
{
    wchar_t value_;

    SymbolNode() = default;

    SymbolNode(const wchar_t& v) :
        value_(v) {}
};

struct SpaceNode: public ASTNode
{
    wchar_t value_;

    SpaceNode() = default;

    SpaceNode(const wchar_t& v) :
        value_(v) {}
};

struct TerminalNode: public ASTNode
{
    std::wstring value_;

    TerminalNode() = default;

    TerminalNode(const std::wstring& v) :
        value_(v) {}
};

class Parser
{
   private:
    const Lexer& lex_;

   public:
    Parser(const Lexer& l) :
        lex_(l) {}
};