/// ast.cc

#include "../include/ast.hpp"

namespace fairuz::AST {

typename Fa_ASTNode::NodeType Fa_ASTNode::getNodeType() const { return NodeType_; }

u32 Fa_ASTNode::getLine() const { return Line_; }

u16 Fa_ASTNode::getColumn() const { return Column_; }

void Fa_ASTNode::setLine(u32 const line) { Line_ = line; }

void Fa_ASTNode::setColumn(u16 const col) { Column_ = col; }

bool Fa_BinaryExpr::equals(Fa_Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto bin = static_cast<Fa_BinaryExpr const*>(other);
    return Operator_ == bin->getOperator() && Left_->equals(bin->getLeft()) && Right_->equals(bin->getRight());
}

Fa_BinaryExpr* Fa_BinaryExpr::clone() const { return Fa_makeBinary(Left_->clone(), Right_->clone(), Operator_); }

Fa_Expr* Fa_BinaryExpr::getLeft() const { return Left_; }

Fa_Expr* Fa_BinaryExpr::getRight() const { return Right_; }

Fa_BinaryOp Fa_BinaryExpr::getOperator() const { return Operator_; }

void Fa_BinaryExpr::setLeft(Fa_Expr* l) { Left_ = l; }

void Fa_BinaryExpr::setRight(Fa_Expr* r) { Right_ = r; }

void Fa_BinaryExpr::setOperator(Fa_BinaryOp op) { Operator_ = op; }

bool Fa_UnaryExpr::equals(Fa_Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto un = static_cast<Fa_UnaryExpr const*>(other);
    return Operator_ == un->getOperator() && Operand_->equals(un->getOperand());
}

Fa_UnaryExpr* Fa_UnaryExpr::clone() const { return Fa_makeUnary(Operand_->clone(), Operator_); }

Fa_Expr* Fa_UnaryExpr::getOperand() const { return Operand_; }

Fa_UnaryOp Fa_UnaryExpr::getOperator() const { return Operator_; }

typename Fa_LiteralExpr::Type Fa_LiteralExpr::getType() const { return Type_; }

i64 Fa_LiteralExpr::getInt() const
{
    assert(isInteger());
    return IntValue_;
}

f64 Fa_LiteralExpr::getFloat() const
{
    assert(isDecimal());
    return FloatValue_;
}

bool Fa_LiteralExpr::getBool() const
{
    assert(isBoolean());
    return BoolValue_;
}

Fa_StringRef Fa_LiteralExpr::getStr() const
{
    assert(isString());
    return StrValue_;
}

bool Fa_LiteralExpr::isInteger() const
{
    return Type_ == Type::INTEGER;
}

bool Fa_LiteralExpr::isDecimal() const { return Type_ == Type::FLOAT; }

bool Fa_LiteralExpr::isBoolean() const { return Type_ == Type::BOOLEAN; }

bool Fa_LiteralExpr::isString() const { return Type_ == Type::STRING; }

bool Fa_LiteralExpr::isNumeric() const { return isInteger() || isDecimal(); }

bool Fa_LiteralExpr::isNil() const { return Type_ == Type::NIL; }

bool Fa_LiteralExpr::equals(Fa_Expr const* other) const
{
    if (!other || other->getKind() != Kind_)
        return false;

    auto lit = static_cast<Fa_LiteralExpr const*>(other);
    if (!lit || Type_ != lit->Type_)
        return false;

    switch (Type_) {
    case Type::INTEGER:
        return IntValue_ == lit->IntValue_;
    case Type::FLOAT:
        return FloatValue_ == lit->FloatValue_;
    case Type::BOOLEAN:
        return BoolValue_ == lit->BoolValue_;
    case Type::STRING:
        return StrValue_ == lit->StrValue_;
    case Type::NIL:
        return true; // two nil objects are always equal
    }
    return false;
}

Fa_LiteralExpr* Fa_LiteralExpr::clone() const
{
    switch (Type_) {
    case Type::INTEGER:
        return Fa_makeLiteralInt(IntValue_);
    case Type::FLOAT:
        return Fa_makeLiteralFloat(FloatValue_);
    case Type::BOOLEAN:
        return Fa_makeLiteralBool(BoolValue_);
    case Type::STRING:
        return Fa_makeLiteralString(StrValue_);
    case Type::NIL:
        return Fa_makeLiteralNil();
    default:
        return nullptr; // should never happen
    }
}

f64 Fa_LiteralExpr::toNumber() const
{
    if (isInteger())
        return static_cast<f64>(IntValue_);
    if (isDecimal())
        return FloatValue_;

    return 0.0;
}

bool Fa_NameExpr::equals(Fa_Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto name = static_cast<Fa_NameExpr const*>(other);
    return Value_ == name->getValue();
}

Fa_NameExpr* Fa_NameExpr::clone() const { return Fa_makeName(Value_); }

Fa_StringRef Fa_NameExpr::getValue() const { return Value_; }

Fa_Expr* Fa_ListExpr::operator[](size_t const i) { return Elements_[i]; }

Fa_Expr const* Fa_ListExpr::operator[](size_t const i) const { return Elements_[i]; }

bool Fa_ListExpr::equals(Fa_Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto list = static_cast<Fa_ListExpr const*>(other);

    if (Elements_.size() != list->Elements_.size())
        return false;

    for (size_t i = 0; i < Elements_.size(); ++i)
        if (!Elements_[i]->equals(list->Elements_[i]))
            return false;

    return true;
}

Fa_ListExpr* Fa_ListExpr::clone() const { return Fa_makeList(Elements_); }

Fa_Array<Fa_Expr*> const& Fa_ListExpr::getElements() const { return Elements_; }

Fa_Array<Fa_Expr*>& Fa_ListExpr::getElements() { return Elements_; }

bool Fa_ListExpr::isEmpty() const { return Elements_.empty(); }

size_t Fa_ListExpr::size() const { return Elements_.size(); }

bool Fa_CallExpr::equals(Fa_Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto call = static_cast<Fa_CallExpr const*>(other);
    return Callee_->equals(call->getCallee()) && Args_->equals(call->getArgsAsListExpr()) && CallLocation_ == call->getCallLocation();
}

Fa_CallExpr* Fa_CallExpr::clone() const { return Fa_makeCall(Callee_->clone(), Args_->clone()); }

Fa_Expr* Fa_CallExpr::getCallee() const { return Callee_; }

Fa_Array<Fa_Expr*> const& Fa_CallExpr::getArgs() const
{
    static Fa_Array<Fa_Expr*> const empty;
    return Args_ ? Args_->getElements() : empty;
}

Fa_Array<Fa_Expr*>& Fa_CallExpr::getArgs()
{
    assert(Args_ && "Attempting to get mutable args when Args_ is null");
    return Args_->getElements();
}

Fa_ListExpr* Fa_CallExpr::getArgsAsListExpr() { return Args_; }

Fa_ListExpr const* Fa_CallExpr::getArgsAsListExpr() const { return Args_; }

typename Fa_CallExpr::CallLocation Fa_CallExpr::getCallLocation() const { return CallLocation_; }

bool Fa_CallExpr::hasArguments() const { return Args_ && !Args_->isEmpty(); }

bool Fa_AssignmentExpr::equals(Fa_Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto bin = static_cast<Fa_AssignmentExpr const*>(other);
    return Target_->equals(bin->getTarget()) && Value_->equals(bin->getValue());
}

Fa_AssignmentExpr* Fa_AssignmentExpr::clone() const
{
    return Fa_makeAssignmentExpr(Target_->clone(), Value_->clone());
}

Fa_Expr* Fa_AssignmentExpr::getTarget() const { return Target_; }

Fa_Expr* Fa_AssignmentExpr::getValue() const { return Value_; }

void Fa_AssignmentExpr::setTarget(Fa_Expr* t) { Target_ = t; }

void Fa_AssignmentExpr::setValue(Fa_Expr* v) { Value_ = v; }

bool Fa_AssignmentExpr::isDeclaration() const { return isDecl_; }

bool Fa_BlockStmt::equals(Fa_Stmt const* other) const
{
    if (!other || other->getKind() != Kind::BLOCK)
        return false;

    auto block = static_cast<Fa_BlockStmt const*>(other);

    if (Statements_.size() != block->Statements_.size())
        return false;

    for (size_t i = 0; i < Statements_.size(); ++i) {
        if (!Statements_[i]->equals(block->Statements_[i]))
            return false;
    }
    return true;
}

bool Fa_IndexExpr::equals(Fa_Expr const* other) const
{
    if (!other || other->getKind() != Kind::INDEX)
        return false;

    auto index = static_cast<Fa_IndexExpr const*>(other);
    return Object_->equals(index->getObject()) && Index_->equals(index->getIndex());
}

Fa_IndexExpr* Fa_IndexExpr::clone() const { return Fa_makeIndex(Object_->clone(), Index_->clone()); }

Fa_Expr* Fa_IndexExpr::getObject() const { return Object_; }

Fa_Expr* Fa_IndexExpr::getIndex() const { return Index_; }

Fa_BlockStmt* Fa_BlockStmt::clone() const { return Fa_makeBlock(Statements_); }

Fa_Array<Fa_Stmt*> const& Fa_BlockStmt::getStatements() const { return Statements_; }

void Fa_BlockStmt::setStatements(Fa_Array<Fa_Stmt*>& stmts) { Statements_ = stmts; }

bool Fa_BlockStmt::isEmpty() const { return Statements_.empty(); }

// Fa_Expr stmt

bool Fa_ExprStmt::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<Fa_ExprStmt const*>(other);
    return Expr_->equals(block->getExpr());
}

Fa_ExprStmt* Fa_ExprStmt::clone() const { return Fa_makeExprStmt(Expr_->clone()); }

Fa_Expr* Fa_ExprStmt::getExpr() const { return Expr_; }

void Fa_ExprStmt::setExpr(Fa_Expr* e) { Expr_ = e; }

// assignment stmt

bool Fa_AssignmentStmt::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<Fa_AssignmentStmt const*>(other);
    return Expr_->getValue()->equals(block->getValue()) && Expr_->getTarget()->equals(block->getTarget());
}

Fa_AssignmentStmt* Fa_AssignmentStmt::clone() const
{
    return Fa_makeAssignmentStmt(Expr_->getTarget(), Expr_->getValue(), Expr_->isDeclaration());
}

Fa_Expr* Fa_AssignmentStmt::getValue() const { return Expr_->getValue(); }

Fa_Expr* Fa_AssignmentStmt::getTarget() const { return Expr_->getTarget(); }

void Fa_AssignmentStmt::setValue(Fa_Expr* v) { Expr_->setValue(v); }

void Fa_AssignmentStmt::setTarget(Fa_Expr* t) { Expr_->setTarget(t); }

bool Fa_AssignmentStmt::isDeclaration() const { return Expr_->isDeclaration(); }

bool Fa_IfStmt::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<Fa_IfStmt const*>(other);
    return Condition_->equals(block->getCondition()) && ThenStmt_->equals(block->getThen()) && ElseStmt_->equals(block->getElse());
}

Fa_IfStmt* Fa_IfStmt::clone() const
{
    return Fa_makeIf(Condition_->clone(), ThenStmt_->clone(), LIKELY(ElseStmt_ == nullptr) ? nullptr : ElseStmt_->clone());
}

Fa_Expr* Fa_IfStmt::getCondition() const { return Condition_; }

Fa_Stmt* Fa_IfStmt::getThen() const { return ThenStmt_; }

Fa_Stmt* Fa_IfStmt::getElse() const { return ElseStmt_; }

void Fa_IfStmt::setThen(Fa_Stmt* t) { ThenStmt_ = t; }

void Fa_IfStmt::setElse(Fa_Stmt* e) { ElseStmt_ = e; }

bool Fa_WhileStmt::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<Fa_WhileStmt const*>(other);
    return Condition_->equals(block->getCondition()) && Body_->equals(block->getBody());
}

Fa_WhileStmt* Fa_WhileStmt::clone() const { return Fa_makeWhile(Condition_->clone(), Body_->clone()); }

Fa_Expr* Fa_WhileStmt::getCondition() const { return Condition_; }

Fa_Stmt* Fa_WhileStmt::getBody() { return Body_; }

Fa_Stmt const* Fa_WhileStmt::getBody() const { return Body_; }

void Fa_WhileStmt::setBody(Fa_Stmt* b) { Body_ = b; }

bool Fa_ForStmt::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<Fa_ForStmt const*>(other);
    return Container_->equals(block->getContainer()) && Iter_->equals(block->getIter()) && Body_->equals(block->getBody());
}

Fa_ForStmt* Fa_ForStmt::clone() const
{
    return getAllocator().allocateObject<Fa_ForStmt>(Container_->clone(), Iter_->clone(), Body_->clone());
}

Fa_Expr* Fa_ForStmt::getContainer() const { return Container_; }

Fa_Expr* Fa_ForStmt::getIter() const { return Iter_; }

Fa_Stmt* Fa_ForStmt::getBody() const { return Body_; }

void Fa_ForStmt::setBody(Fa_Stmt* b) { Body_ = b; }

bool Fa_FunctionDef::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<Fa_FunctionDef const*>(other);
    return Name_->equals(block->getName()) && Params_->equals(block->getParameterList()) && Body_->equals(block->getBody());
}

Fa_FunctionDef* Fa_FunctionDef::clone() const
{
    return Fa_makeFunction(Name_->clone(), Params_->clone(), Body_->clone());
}

Fa_NameExpr* Fa_FunctionDef::getName() const { return Name_; }

Fa_Array<Fa_Expr*> const& Fa_FunctionDef::getParameters() const { return Params_->getElements(); }

Fa_ListExpr* Fa_FunctionDef::getParameterList() const { return Params_; }

Fa_Stmt* Fa_FunctionDef::getBody() const { return Body_; }

void Fa_FunctionDef::setBody(Fa_Stmt* b) { Body_ = b; }

bool Fa_FunctionDef::hasParameters() const { return Params_ && !Params_->isEmpty(); }

Fa_ReturnStmt* Fa_ReturnStmt::clone() const { return Fa_makeReturn(Value_->clone()); }

Fa_Expr const* Fa_ReturnStmt::getValue() const { return Value_; }

void Fa_ReturnStmt::setValue(Fa_Expr* v) { Value_ = v; }

bool Fa_ReturnStmt::hasValue() const { return Value_ != nullptr; }

[[nodiscard]] bool Fa_ReturnStmt::equals(Fa_Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto ret_stmt = static_cast<Fa_ReturnStmt const*>(other);
    return Value_->equals(ret_stmt->getValue());
}

} // namespace fairuz::ast
