/// ast.cc

#include "ast.hpp"

namespace fairuz::AST {

typename Fa_ASTNode::NodeType Fa_ASTNode::get_node_type() const { return node_type; }

bool Fa_BinaryExpr::equals(Fa_Expr const* other) const
{
    if (other->get_kind() != m_kind)
        return false;

    auto bin = static_cast<Fa_BinaryExpr const*>(other);
    return m_operator == bin->get_operator() && m_left->equals(bin->get_left()) && m_right->equals(bin->get_right());
}

Fa_BinaryExpr* Fa_BinaryExpr::clone() const { return Fa_makeBinary(m_left->clone(), m_right->clone(), m_operator, m_loc); }

Fa_Expr* Fa_BinaryExpr::get_left() const { return m_left; }

Fa_Expr* Fa_BinaryExpr::get_right() const { return m_right; }

Fa_BinaryOp Fa_BinaryExpr::get_operator() const { return m_operator; }

void Fa_BinaryExpr::set_left(Fa_Expr* l) { m_left = l; }

void Fa_BinaryExpr::set_right(Fa_Expr* r) { m_right = r; }

void Fa_BinaryExpr::set_operator(Fa_BinaryOp op) { m_operator = op; }

bool Fa_UnaryExpr::equals(Fa_Expr const* other) const
{
    if (other->get_kind() != m_kind)
        return false;

    auto un = static_cast<Fa_UnaryExpr const*>(other);
    return m_operator == un->get_operator() && m_operand->equals(un->get_operand());
}

Fa_UnaryExpr* Fa_UnaryExpr::clone() const { return Fa_makeUnary(m_operand->clone(), m_operator, m_loc); }

Fa_Expr* Fa_UnaryExpr::get_operand() const { return m_operand; }

Fa_UnaryOp Fa_UnaryExpr::get_operator() const { return m_operator; }

typename Fa_LiteralExpr::Type Fa_LiteralExpr::get_type() const { return m_type; }

i64 Fa_LiteralExpr::get_int() const
{
    assert(is_integer());
    return int_value;
}

f64 Fa_LiteralExpr::get_float() const
{
    assert(is_float());
    return float_value;
}

bool Fa_LiteralExpr::get_bool() const
{
    assert(is_bool());
    return bool_value;
}

Fa_StringRef Fa_LiteralExpr::get_str() const
{
    assert(is_string());
    return str_value;
}

bool Fa_LiteralExpr::is_integer() const { return m_type == Type::INTEGER; }

bool Fa_LiteralExpr::is_float() const { return m_type == Type::FLOAT; }

bool Fa_LiteralExpr::is_bool() const { return m_type == Type::BOOLEAN; }

bool Fa_LiteralExpr::is_string() const { return m_type == Type::STRING; }

bool Fa_LiteralExpr::is_numeric() const { return is_integer() || is_float(); }

bool Fa_LiteralExpr::is_nil() const { return m_type == Type::NIL; }

bool Fa_LiteralExpr::equals(Fa_Expr const* other) const
{
    if (other == nullptr || other->get_kind() != m_kind)
        return false;

    auto lit = static_cast<Fa_LiteralExpr const*>(other);
    if (lit == nullptr || m_type != lit->m_type)
        return false;

    switch (m_type) {
    case Type::INTEGER:
        return int_value == lit->int_value;
    case Type::FLOAT:
        return float_value == lit->float_value;
    case Type::BOOLEAN:
        return bool_value == lit->bool_value;
    case Type::STRING:
        return str_value == lit->str_value;
    case Type::NIL:
        return true; // two nil objects are always equal
    }

    return false;
}

Fa_LiteralExpr* Fa_LiteralExpr::clone() const
{
    switch (m_type) {
    case Type::INTEGER:
        return Fa_makeLiteralInt(int_value, m_loc);
    case Type::FLOAT:
        return Fa_makeLiteralFloat(float_value, m_loc);
    case Type::BOOLEAN:
        return Fa_makeLiteralBool(bool_value, m_loc);
    case Type::STRING:
        return Fa_makeLiteralString(str_value, m_loc);
    case Type::NIL:
        return Fa_makeLiteralNil(m_loc);
    default:
        return nullptr; // should never happen
    }
}

f64 Fa_LiteralExpr::is_number() const
{
    if (is_integer())
        return static_cast<f64>(int_value);
    if (is_float())
        return float_value;

    return 0.0;
}

bool Fa_NameExpr::equals(Fa_Expr const* other) const
{
    if (other->get_kind() != m_kind)
        return false;

    auto name = static_cast<Fa_NameExpr const*>(other);
    return m_value == name->get_value();
}

Fa_NameExpr* Fa_NameExpr::clone() const { return Fa_makeName(m_value, m_loc); }

Fa_StringRef Fa_NameExpr::get_value() const { return m_value; }

Fa_Expr* Fa_ListExpr::operator[](size_t const i) { return m_elements[i]; }

Fa_Expr const* Fa_ListExpr::operator[](size_t const i) const { return m_elements[i]; }

bool Fa_ListExpr::equals(Fa_Expr const* other) const
{
    if (other->get_kind() != m_kind)
        return false;

    auto list = static_cast<Fa_ListExpr const*>(other);

    if (m_elements.size() != list->m_elements.size())
        return false;

    for (size_t i = 0; i < m_elements.size(); i += 1) {
        if (!m_elements[i]->equals(list->m_elements[i]))
            return false;
    }

    return true;
}

Fa_ListExpr* Fa_ListExpr::clone() const { return Fa_makeList(m_elements, m_loc); }

Fa_Array<Fa_Expr*> const& Fa_ListExpr::get_elements() const { return m_elements; }

Fa_Array<Fa_Expr*>& Fa_ListExpr::get_elements() { return m_elements; }

bool Fa_ListExpr::is_empty() const { return m_elements.empty(); }

size_t Fa_ListExpr::size() const { return m_elements.size(); }

bool Fa_CallExpr::equals(Fa_Expr const* other) const
{
    if (other->get_kind() != m_kind)
        return false;

    auto call = static_cast<Fa_CallExpr const*>(other);
    return m_callee->equals(call->get_callee()) && m_args->equals(call->get_args_as_list_expr()) && m_call_location == call->get_call_location();
}

Fa_CallExpr* Fa_CallExpr::clone() const { return Fa_makeCall(m_callee->clone(), m_args->clone(), m_loc); }

Fa_Expr* Fa_CallExpr::get_callee() const { return m_callee; }

Fa_Array<Fa_Expr*> const& Fa_CallExpr::get_args() const
{
    static Fa_Array<Fa_Expr*> const empty;
    return m_args ? m_args->get_elements() : empty;
}

Fa_Array<Fa_Expr*>& Fa_CallExpr::get_args()
{
    assert(m_args && "Attempting to get mutable args when Args_ is null");
    return m_args->get_elements();
}

Fa_ListExpr* Fa_CallExpr::get_args_as_list_expr() { return m_args; }

Fa_ListExpr const* Fa_CallExpr::get_args_as_list_expr() const { return m_args; }

bool Fa_DictExpr::equals(Fa_Expr const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    // a dictionary normally doesn't care about order so this is logically not correct
    // but we gonna stub an implementation that requires order until someone fixes this
    auto dict = static_cast<Fa_DictExpr const*>(other)->get_content();
    u32 const s1 = content.size();
    u32 const s2 = dict.size();

    if (s1 != s2)
        return false;

    for (u32 i = 0; i < s1; i += 1) {
        if (!content[i].first->equals(dict[i].first) || !content[i].second->equals(dict[i].second))
            return false;
    }

    return true;
}

Fa_Expr* Fa_DictExpr::clone() const { return Fa_makeDict(content, m_loc); }
Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> Fa_DictExpr::get_content() const { return content; }
void Fa_DictExpr::set_content(Fa_Array<std::pair<Fa_Expr*, Fa_Expr*>> c) { content = c; }

typename Fa_CallExpr::CallLocation Fa_CallExpr::get_call_location() const { return m_call_location; }

bool Fa_CallExpr::has_arguments() const { return m_args != nullptr && !m_args->is_empty(); }

bool Fa_AssignmentExpr::equals(Fa_Expr const* other) const
{
    if (other->get_kind() != m_kind)
        return false;

    auto bin = static_cast<Fa_AssignmentExpr const*>(other);
    return m_target->equals(bin->get_target()) && m_value->equals(bin->get_value());
}

Fa_AssignmentExpr* Fa_AssignmentExpr::clone() const
{
    return Fa_makeAssignmentExpr(m_target->clone(), m_value->clone(), m_loc, m_is_decl);
}

Fa_Expr* Fa_AssignmentExpr::get_target() const { return m_target; }

Fa_Expr* Fa_AssignmentExpr::get_value() const { return m_value; }

void Fa_AssignmentExpr::set_target(Fa_Expr* t) { m_target = t; }

void Fa_AssignmentExpr::set_value(Fa_Expr* v) { m_value = v; }

void Fa_AssignmentExpr::set_decl() { m_is_decl = true; }

bool Fa_AssignmentExpr::is_declaration() const { return m_is_decl; }

bool Fa_BlockStmt::equals(Fa_Stmt const* other) const
{
    if (other == nullptr || other->get_kind() != Kind::BLOCK)
        return false;

    auto block = static_cast<Fa_BlockStmt const*>(other);

    if (m_statements.size() != block->m_statements.size())
        return false;

    for (size_t i = 0; i < m_statements.size(); i += 1) {
        if (!m_statements[i]->equals(block->m_statements[i]))
            return false;
    }

    return true;
}

bool Fa_IndexExpr::equals(Fa_Expr const* other) const
{
    if (other == nullptr || other->get_kind() != Kind::INDEX)
        return false;

    auto idx = static_cast<Fa_IndexExpr const*>(other);
    return m_object->equals(idx->get_object()) && m_index->equals(idx->get_index());
}

Fa_IndexExpr* Fa_IndexExpr::clone() const { return Fa_makeIndex(m_object->clone(), m_index->clone(), m_loc); }

Fa_Expr* Fa_IndexExpr::get_object() const { return m_object; }

Fa_Expr* Fa_IndexExpr::get_index() const { return m_index; }

Fa_BlockStmt* Fa_BlockStmt::clone() const { return Fa_makeBlock(m_statements, m_loc); }

Fa_Array<Fa_Stmt*> const& Fa_BlockStmt::get_statements() const { return m_statements; }

void Fa_BlockStmt::set_statements(Fa_Array<Fa_Stmt*>& stmts) { m_statements = stmts; }

bool Fa_BlockStmt::is_empty() const { return m_statements.empty(); }

// Fa_Expr stmt

bool Fa_ExprStmt::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto block = static_cast<Fa_ExprStmt const*>(other);
    return m_expr->equals(block->get_expr());
}

Fa_ExprStmt* Fa_ExprStmt::clone() const { return Fa_makeExprStmt(m_expr->clone(), m_loc); }

Fa_Expr* Fa_ExprStmt::get_expr() const { return m_expr; }

void Fa_ExprStmt::set_expr(Fa_Expr* e) { m_expr = e; }

// assignment stmt

bool Fa_AssignmentStmt::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto block = static_cast<Fa_AssignmentStmt const*>(other);
    return m_expr->get_value()->equals(block->get_value()) && m_expr->get_target()->equals(block->get_target());
}

Fa_AssignmentStmt* Fa_AssignmentStmt::clone() const
{
    return Fa_makeAssignmentStmt(m_expr->get_target(), m_expr->get_value(), m_loc, m_expr->is_declaration());
}

Fa_Expr* Fa_AssignmentStmt::get_value() const { return m_expr->get_value(); }

Fa_Expr* Fa_AssignmentStmt::get_target() const { return m_expr->get_target(); }

void Fa_AssignmentStmt::set_value(Fa_Expr* v) { m_expr->set_value(v); }

void Fa_AssignmentStmt::set_target(Fa_Expr* t) { m_expr->set_target(t); }

void Fa_AssignmentStmt::set_decl() { m_expr->set_decl(); }

bool Fa_AssignmentStmt::is_declaration() const { return m_expr->is_declaration(); }

bool Fa_IfStmt::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto if_stmt = static_cast<Fa_IfStmt const*>(other);
    bool eq_else = false;

    if (m_else_stmt != nullptr && if_stmt->get_else())
        eq_else = m_else_stmt->equals(if_stmt->get_else());

    return m_condition->equals(if_stmt->get_condition()) && m_then_stmt->equals(if_stmt->get_then()) && eq_else;
}

Fa_IfStmt* Fa_IfStmt::clone() const
{
    return Fa_makeIf(m_condition->clone(), m_then_stmt->clone(), m_loc, LIKELY(m_else_stmt == nullptr) ? nullptr : m_else_stmt->clone());
}

Fa_Expr* Fa_IfStmt::get_condition() const { return m_condition; }

Fa_Stmt* Fa_IfStmt::get_then() const { return m_then_stmt; }

Fa_Stmt* Fa_IfStmt::get_else() const { return m_else_stmt; }

void Fa_IfStmt::set_then(Fa_Stmt* t) { m_then_stmt = t; }

void Fa_IfStmt::set_else(Fa_Stmt* e) { m_else_stmt = e; }

bool Fa_WhileStmt::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto block = static_cast<Fa_WhileStmt const*>(other);
    return m_condition->equals(block->get_condition()) && m_body->equals(block->get_body());
}

Fa_WhileStmt* Fa_WhileStmt::clone() const { return Fa_makeWhile(m_condition->clone(), m_body->clone(), m_loc); }

Fa_Expr* Fa_WhileStmt::get_condition() const { return m_condition; }

Fa_Stmt* Fa_WhileStmt::get_body() { return m_body; }

Fa_Stmt const* Fa_WhileStmt::get_body() const { return m_body; }

void Fa_WhileStmt::set_body(Fa_Stmt* b) { m_body = b; }

bool Fa_ForStmt::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto block = static_cast<Fa_ForStmt const*>(other);
    return m_container->equals(block->get_container()) && m_iter->equals(block->get_iter()) && m_body->equals(block->get_body());
}

Fa_ForStmt* Fa_ForStmt::clone() const
{
    return get_allocator().allocate_object<Fa_ForStmt>(m_container->clone(), m_iter->clone(), m_body->clone(), m_loc);
}

Fa_Expr* Fa_ForStmt::get_container() const { return m_container; }

Fa_NameExpr* Fa_ForStmt::get_target() const { return static_cast<Fa_NameExpr*>(m_container); }

Fa_Expr* Fa_ForStmt::get_iter() const { return m_iter; }

Fa_Stmt* Fa_ForStmt::get_body() const { return m_body; }

void Fa_ForStmt::set_body(Fa_Stmt* b) { m_body = b; }

bool Fa_FunctionDef::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto block = static_cast<Fa_FunctionDef const*>(other);
    return m_name->equals(block->get_name()) && m_params->equals(block->get_parameter_list()) && m_body->equals(block->get_body());
}

Fa_FunctionDef* Fa_FunctionDef::clone() const
{
    return Fa_makeFunction(m_name->clone(), m_params->clone(), m_body->clone(), m_loc);
}

Fa_NameExpr* Fa_FunctionDef::get_name() const { return m_name; }

Fa_Array<Fa_Expr*> const& Fa_FunctionDef::get_parameters() const { return m_params->get_elements(); }

Fa_ListExpr* Fa_FunctionDef::get_parameter_list() const { return m_params; }

Fa_Stmt* Fa_FunctionDef::get_body() const { return m_body; }

void Fa_FunctionDef::set_body(Fa_Stmt* b) { m_body = b; }

bool Fa_FunctionDef::has_parameters() const { return m_params && !m_params->is_empty(); }

Fa_ReturnStmt* Fa_ReturnStmt::clone() const
{
    return Fa_makeReturn(m_loc, m_value == nullptr ? nullptr : m_value->clone());
}

Fa_Expr* Fa_ReturnStmt::get_value() { return m_value; }

Fa_Expr const* Fa_ReturnStmt::get_value() const { return m_value; }

void Fa_ReturnStmt::set_value(Fa_Expr* v) { m_value = v; }

bool Fa_ReturnStmt::has_value() const { return m_value != nullptr; }

bool Fa_ReturnStmt::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto ret_stmt = static_cast<Fa_ReturnStmt const*>(other);
    if (m_value == nullptr || ret_stmt->get_value() == nullptr)
        return m_value == ret_stmt->get_value();

    return m_value->equals(ret_stmt->get_value());
}

bool Fa_ClassDef::equals(Fa_Stmt const* other) const
{
    if (m_kind != other->get_kind())
        return false;

    auto class_def = static_cast<Fa_ClassDef const*>(other);

    Fa_Array<Fa_Expr*> other_members = class_def->get_members();
    Fa_Array<Fa_Stmt*> other_methods = class_def->get_methods();
    if (other_members.size() != m_members.size() || other_methods.size() != m_methods.size())
        return false;

    for (u32 i = 0, n = other_members.size(); i < n; ++i) {
        if (!other_members[i]->equals(m_members[i]))
            return false;
    }

    for (u32 i = 0, n = other_methods.size(); i < n; ++i) {
        if (!other_methods[i]->equals(m_methods[i]))
            return false;
    }

    return m_name->equals(class_def->get_name());
}

Fa_ClassDef* Fa_ClassDef::clone() const
{
    Fa_Array<Fa_Expr*> member_clones;
    Fa_Array<Fa_Stmt*> method_clones;
    for (Fa_Expr* mem : m_members)
        member_clones.push(mem->clone());
    for (Fa_Stmt* met : m_methods)
        method_clones.push(met->clone());
    return Fa_makeClassDef(m_name->clone(), member_clones, method_clones, m_loc);
}

Fa_Array<Fa_Expr*> Fa_ClassDef::get_members() const { return m_members; }
Fa_Array<Fa_Stmt*> Fa_ClassDef::get_methods() const { return m_methods; }

Fa_Expr* Fa_ClassDef::get_name() const { return m_name; }

bool Fa_BreakStmt::equals(Fa_Stmt const* other) const
{
    return other != nullptr && other->get_kind() == Kind::BREAK;
}

Fa_BreakStmt* Fa_BreakStmt::clone() const { return Fa_makeBreak(m_loc); }

bool Fa_ContinueStmt::equals(Fa_Stmt const* other) const
{
    return other != nullptr && other->get_kind() == Kind::CONTINUE;
}

Fa_ContinueStmt* Fa_ContinueStmt::clone() const { return Fa_makeContinue(m_loc); }

} // namespace fairuz::ast
