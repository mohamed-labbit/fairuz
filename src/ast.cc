/// ast.cc

#include "../include/ast.hpp"

namespace mylang::ast {

typename ASTNode::NodeType ASTNode::getNodeType() const { return NodeType_; }

uint32_t ASTNode::getLine() const { return Line_; }

uint16_t ASTNode::getColumn() const { return Column_; }

void ASTNode::setLine(uint32_t const line) { Line_ = line; }

void ASTNode::setColumn(uint16_t const col) { Column_ = col; }

bool BinaryExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto bin = static_cast<BinaryExpr const*>(other);
    return Operator_ == bin->getOperator() && Left_->equals(bin->getLeft()) && Right_->equals(bin->getRight());
}

BinaryExpr* BinaryExpr::clone() const { return makeBinary(Left_->clone(), Right_->clone(), Operator_); }

Expr* BinaryExpr::getLeft() const { return Left_; }

Expr* BinaryExpr::getRight() const { return Right_; }

BinaryOp BinaryExpr::getOperator() const { return Operator_; }

void BinaryExpr::setLeft(Expr* l) { Left_ = l; }

void BinaryExpr::setRight(Expr* r) { Right_ = r; }

void BinaryExpr::setOperator(BinaryOp op) { Operator_ = op; }

bool UnaryExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto un = static_cast<UnaryExpr const*>(other);
    return Operator_ == un->getOperator() && Operand_->equals(un->getOperand());
}

UnaryExpr* UnaryExpr::clone() const { return makeUnary(Operand_->clone(), Operator_); }

Expr* UnaryExpr::getOperand() const { return Operand_; }

UnaryOp UnaryExpr::getOperator() const { return Operator_; }

typename LiteralExpr::Type LiteralExpr::getType() const { return Type_; }

int64_t LiteralExpr::getInt() const
{
    assert(isInteger());
    return IntValue_;
}

double LiteralExpr::getFloat() const
{
    assert(isDecimal());
    return FloatValue_;
}

bool LiteralExpr::getBool() const
{
    assert(isBoolean());
    return BoolValue_;
}

StringRef LiteralExpr::getStr() const
{
    assert(isString());
    return StrValue_;
}

bool LiteralExpr::isInteger() const { return Type_ == Type::INTEGER || Type_ == Type::HEX || Type_ == Type::OCTAL || Type_ == Type::BINARY; }

bool LiteralExpr::isDecimal() const { return Type_ == Type::FLOAT; }

bool LiteralExpr::isBoolean() const { return Type_ == Type::BOOLEAN; }

bool LiteralExpr::isString() const { return Type_ == Type::STRING; }

bool LiteralExpr::isNumeric() const { return isInteger() || isDecimal(); }

bool LiteralExpr::isNil() const { return Type_ == Type::NIL; }

bool LiteralExpr::equals(Expr const* other) const
{
    if (!other || other->getKind() != Kind_)
        return false;

    auto lit = static_cast<LiteralExpr const*>(other);
    if (!lit || Type_ != lit->Type_)
        return false;

    switch (Type_) {
    case Type::INTEGER:
    case Type::HEX:
    case Type::OCTAL:
    case Type::BINARY:
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

LiteralExpr* LiteralExpr::clone() const
{
    switch (Type_) {
    case Type::INTEGER:
    case Type::HEX:
    case Type::OCTAL:
    case Type::BINARY:
        return makeLiteralInt(IntValue_);
    case Type::FLOAT:
        return makeLiteralFloat(FloatValue_);
    case Type::BOOLEAN:
        return makeLiteralBool(BoolValue_);
    case Type::STRING:
        return makeLiteralString(StrValue_);
    case Type::NIL:
        return makeLiteralNil();
    default:
        return nullptr; // should never happen
    }
}

double LiteralExpr::toNumber() const
{
    if (isInteger())
        return static_cast<double>(IntValue_);
    if (isDecimal())
        return FloatValue_;

    return 0.0;
}

bool NameExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto name = static_cast<NameExpr const*>(other);
    return Value_ == name->getValue();
}

NameExpr* NameExpr::clone() const { return makeName(Value_); }

StringRef NameExpr::getValue() const { return Value_; }

Expr* ListExpr::operator[](size_t const i) { return Elements_[i]; }

Expr const* ListExpr::operator[](size_t const i) const { return Elements_[i]; }

bool ListExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto list = static_cast<ListExpr const*>(other);

    if (Elements_.size() != list->Elements_.size())
        return false;

    for (size_t i = 0; i < Elements_.size(); ++i)
        if (!Elements_[i]->equals(list->Elements_[i]))
            return false;

    return true;
}

ListExpr* ListExpr::clone() const { return makeList(Elements_); }

Array<Expr*> const& ListExpr::getElements() const { return Elements_; }

Array<Expr*>& ListExpr::getElements() { return Elements_; }

bool ListExpr::isEmpty() const { return Elements_.empty(); }

size_t ListExpr::size() const { return Elements_.size(); }

bool CallExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto call = static_cast<CallExpr const*>(other);
    return Callee_->equals(call->getCallee()) && Args_->equals(call->getArgsAsListExpr()) && CallLocation_ == call->getCallLocation();
}

CallExpr* CallExpr::clone() const { return makeCall(Callee_->clone(), Args_->clone()); }

Expr* CallExpr::getCallee() const { return Callee_; }

Array<Expr*> const& CallExpr::getArgs() const
{
    static Array<Expr*> const empty;
    return Args_ ? Args_->getElements() : empty;
}

Array<Expr*>& CallExpr::getArgs()
{
    assert(Args_ && "Attempting to get mutable args when Args_ is null");
    return Args_->getElements();
}

ListExpr* CallExpr::getArgsAsListExpr() { return Args_; }

ListExpr const* CallExpr::getArgsAsListExpr() const { return Args_; }

typename CallExpr::CallLocation CallExpr::getCallLocation() const { return CallLocation_; }

bool CallExpr::hasArguments() const { return Args_ && !Args_->isEmpty(); }

bool AssignmentExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    auto bin = static_cast<AssignmentExpr const*>(other);
    return Target_->equals(bin->getTarget()) && Value_->equals(bin->getValue());
}

AssignmentExpr* AssignmentExpr::clone() const { return makeAssignmentExpr(Target_->clone(), Value_->clone()); }

Expr* AssignmentExpr::getTarget() const { return Target_; }

Expr* AssignmentExpr::getValue() const { return Value_; }

bool AssignmentExpr::isDeclaration() const { return isDecl_; }

bool BlockStmt::equals(Stmt const* other) const
{
    if (!other || other->getKind() != Kind::BLOCK)
        return false;

    BlockStmt const* block = static_cast<BlockStmt const*>(other);

    if (Statements_.size() != block->Statements_.size())
        return false;

    for (size_t i = 0; i < Statements_.size(); ++i) {
        if (!Statements_[i]->equals(block->Statements_[i]))
            return false;
    }
    return true;
}

bool IndexExpr::equals(Expr const* other) const
{
    if (!other || other->getKind() != Kind::INDEX)
        return false;

    IndexExpr const* index = static_cast<IndexExpr const*>(other);
    return Object_->equals(index->getObject()) && Index_->equals(index->getIndex());
}

IndexExpr* IndexExpr::clone() const { return makeIndex(Object_->clone(), Index_->clone()); }

Expr* IndexExpr::getObject() const { return Object_; }

Expr* IndexExpr::getIndex() const { return Index_; }

BlockStmt* BlockStmt::clone() const { return makeBlock(Statements_); }

Array<Stmt*> const& BlockStmt::getStatements() const { return Statements_; }

void BlockStmt::setStatements(Array<Stmt*>& stmts) { Statements_ = stmts; }

bool BlockStmt::isEmpty() const { return Statements_.empty(); }

// expr stmt

bool ExprStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<ExprStmt const*>(other);
    return Expr_->equals(block->getExpr());
}

ExprStmt* ExprStmt::clone() const { return makeExprStmt(Expr_->clone()); }

Expr* ExprStmt::getExpr() const { return Expr_; }

void ExprStmt::setExpr(Expr* e) { Expr_ = e; }

// assignment stmt

bool AssignmentStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<AssignmentStmt const*>(other);
    return Value_->equals(block->getValue()) && Target_->equals(block->getTarget());
}

AssignmentStmt* AssignmentStmt::clone() const { return makeAssignmentStmt(Target_->clone(), Target_->clone()); }

Expr* AssignmentStmt::getValue() const { return Value_; }

Expr* AssignmentStmt::getTarget() const { return Target_; }

void AssignmentStmt::setValue(Expr* v) { Value_ = v; }

void AssignmentStmt::setTarget(Expr* t) { Target_ = t; }

bool AssignmentStmt::isDeclaration() const { return isDecl_; }

bool IfStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<IfStmt const*>(other);
    return Condition_->equals(block->getCondition()) && ThenStmt_->equals(block->getThen()) && ElseStmt_->equals(block->getElse());
}

IfStmt* IfStmt::clone() const { return makeIf(Condition_->clone(), ThenStmt_->clone(), ElseStmt_->clone()); }

Expr* IfStmt::getCondition() const { return Condition_; }

Stmt* IfStmt::getThen() const { return ThenStmt_; }

Stmt* IfStmt::getElse() const { return ElseStmt_; }

void IfStmt::setThen(Stmt* t) { ThenStmt_ = t; }

void IfStmt::setElse(Stmt* e) { ElseStmt_ = e; }

bool WhileStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<WhileStmt const*>(other);
    return Condition_->equals(block->getCondition()) && Body_->equals(block->getBody());
}

WhileStmt* WhileStmt::clone() const { return makeWhile(Condition_->clone(), Body_->clone()); }

Expr* WhileStmt::getCondition() const { return Condition_; }

Stmt* WhileStmt::getBody() { return Body_; }

Stmt const* WhileStmt::getBody() const { return Body_; }

void WhileStmt::setBody(Stmt* b) { Body_ = b; }

bool ForStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<ForStmt const*>(other);
    return Target_->equals(block->getTarget()) && Iter_->equals(block->getIter()) && Body_->equals(block->getBody());
}

ForStmt* ForStmt::clone() const { return getAllocator().allocateObject<ForStmt>(Target_->clone(), Iter_->clone(), Body_->clone()); }

NameExpr* ForStmt::getTarget() const { return Target_; }

Expr* ForStmt::getIter() const { return Iter_; }

Stmt* ForStmt::getBody() const { return Body_; }

void ForStmt::setBody(Stmt* b) { Body_ = b; }

bool FunctionDef::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto block = static_cast<FunctionDef const*>(other);
    return Name_->equals(block->getName()) && Params_->equals(block->getParameterList()) && Body_->equals(block->getBody());
}

FunctionDef* FunctionDef::clone() const { return makeFunction(Name_->clone(), Params_->clone(), Body_->clone()); }

NameExpr* FunctionDef::getName() const { return Name_; }

Array<Expr*> const& FunctionDef::getParameters() const { return Params_->getElements(); }

ListExpr* FunctionDef::getParameterList() const { return Params_; }

Stmt* FunctionDef::getBody() const { return Body_; }

void FunctionDef::setBody(Stmt* b) { Body_ = b; }

bool FunctionDef::hasParameters() const { return Params_ && !Params_->isEmpty(); }

bool ReturnStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    auto ret = static_cast<ReturnStmt const*>(other);
    if (!Value_ && !ret->getValue())
        return true;

    return Value_->equals(ret->getValue());
}

ReturnStmt* ReturnStmt::clone() const { return makeReturn(Value_->clone()); }

Expr const* ReturnStmt::getValue() const { return Value_; }

void ReturnStmt::setValue(Expr* v) { Value_ = v; }

bool ReturnStmt::hasValue() const { return Value_ != nullptr; }

} // namespace mylang::ast
