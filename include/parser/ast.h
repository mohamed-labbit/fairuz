#pragma once

#include "lex/lexer.h"
#include "lex/token.h"


namespace mylang {
namespace parser {
namespace ast {


using string_type = std::u16string;

struct ASTNode
{
   private:
    lex::tok::Token token_;

   public:
    virtual ~ASTNode() = default;
};

struct NumberNode: public ASTNode
{
    double value_;

    NumberNode() = default;

    NumberNode(double v) :
        value_(v)
    {
    }
};

struct BinaryOpNode: public ASTNode
{
    string_type op_{};
    ASTNode*    left_{nullptr};
    ASTNode*    right_{nullptr};

    BinaryOpNode() = default;

    BinaryOpNode(const string_type& o, ASTNode* l, ASTNode* r) :
        op_(o),
        left_(l),
        right_(r)
    {
    }
};

struct NonTerminalNode: public ASTNode
{
    string_type value_;

    NonTerminalNode() = default;

    NonTerminalNode(string_type v) :
        value_(v)
    {
    }
};

struct SymbolNode: public ASTNode
{
    wchar_t value_;

    SymbolNode() = default;

    SymbolNode(const wchar_t& v) :
        value_(v)
    {
    }
};

struct SpaceNode: public ASTNode
{
    wchar_t value_;

    SpaceNode() = default;

    SpaceNode(const wchar_t& v) :
        value_(v)
    {
    }
};

struct TerminalNode: public ASTNode
{
    string_type value_;

    TerminalNode() = default;

    TerminalNode(const string_type& v) :
        value_(v)
    {
    }
};

class Parser
{
   private:
    const lex::Lexer& lex_;

   public:
    Parser(const lex::Lexer& l) :
        lex_(l)
    {
    }
};


}
}
}