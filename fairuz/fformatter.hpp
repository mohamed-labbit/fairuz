#ifndef FA_FORMAT_HPP
#define FA_FORMAT_HPP

#include "fAST.hpp"
#include "farray.hpp"
#include "fstring.hpp"

namespace fairuz {

class Fa_Formatter {
private:
    u32 m_indent_sz { 4 };
    u32 m_indent_level { 0 };
    u32 m_line_width { 100 };
    bool m_line_start { true };

    u32 m_current_line { 0 };
    u32 m_current_col { 0 };

    Fa_StringRef m_formatted;

    void write(Fa_StringRef const& text);
    void write(char ch);
    void write_newline();

    void format_body(AST::Fa_Stmt const* stmt);
    void format_if_statement(AST::Fa_IfStmt const* stmt);
    void format_statement(AST::Fa_Stmt const* stmt);
    void format_expression(AST::Fa_Expr const* expr, int parent_precedence = -1, bool parenthesize_on_equal = false);
    void format_comma_separated(Fa_Array<AST::Fa_Expr*> const& exprs);
    void format_assignment_target(AST::Fa_Expr const* expr);

    [[nodiscard]] int precedence(AST::Fa_Expr const* expr) const;
    [[nodiscard]] bool is_class_member_target(AST::Fa_Expr const* expr) const;

public:
    [[nodiscard]] Fa_StringRef format(Fa_Array<AST::Fa_Stmt*> const& stmts);
};

} // namespace fairuz

#endif // FA_FORMAT_HPP
