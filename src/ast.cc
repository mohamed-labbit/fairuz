// ast_node.cc

#include "../include/ast.hpp"

namespace mylang {
namespace ast {

typename ASTNode::NodeType ASTNode::getNodeType() const { return NodeType_; }
uint32_t ASTNode::getLine() const { return Line_; }
uint32_t ASTNode::getColumn() const { return Column_; }
void ASTNode::setLine(uint32_t const l) { Line_ = l; }
void ASTNode::setColumn(uint32_t const c) { Column_ = c; }

// binary expr
bool BinaryExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    BinaryExpr const* bin = dynamic_cast<BinaryExpr const*>(other);
    return Operator_ == bin->getOperator()
        && Left_->equals(bin->getLeft())
        && Right_->equals(bin->getRight());
}

BinaryExpr* BinaryExpr::clone() const { return makeBinary(Left_->clone(), Right_->clone(), Operator_); }
Expr* BinaryExpr::getLeft() const { return Left_; }
Expr* BinaryExpr::getRight() const { return Right_; }
BinaryOp BinaryExpr::getOperator() const { return Operator_; }
void BinaryExpr::setLeft(Expr* l) { Left_ = l; }
void BinaryExpr::setRight(Expr* r) { Right_ = r; }
void BinaryExpr::setOperator(BinaryOp op) { Operator_ = op; }

// unary expr
bool UnaryExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(other);
    return Operator_ == un->getOperator() && Operand_->equals(un->getOperand());
}

UnaryExpr* UnaryExpr::clone() const { return makeUnary(Operand_->clone(), Operator_); }
Expr* UnaryExpr::getOperand() const { return Operand_; }
UnaryOp UnaryExpr::getOperator() const { return Operator_; }

// literal expr
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

bool LiteralExpr::isInteger() const
{
    return Type_ == Type::INTEGER
        || Type_ == Type::HEX
        || Type_ == Type::OCTAL
        || Type_ == Type::BINARY;
}

bool LiteralExpr::isDecimal() const { return Type_ == Type::FLOAT; }
bool LiteralExpr::isBoolean() const { return Type_ == Type::BOOLEAN; }
bool LiteralExpr::isString() const { return Type_ == Type::STRING; }
bool LiteralExpr::isNumeric() const { return isInteger() || isDecimal(); }
bool LiteralExpr::isNil() const { return Type_ == Type::NIL; }

bool LiteralExpr::equals(Expr const* other) const
{
    if (!other || other->getKind() != Kind_)
        return false;

    auto const* lit = dynamic_cast<LiteralExpr const*>(other);
    if (!lit || Type_ != lit->Type_)
        return false;

    switch (Type_) {
    case Type::INTEGER:
    case Type::HEX:
    case Type::OCTAL:
    case Type::BINARY: return IntValue_ == lit->IntValue_;
    case Type::FLOAT: return FloatValue_ == lit->FloatValue_;
    case Type::BOOLEAN: return BoolValue_ == lit->BoolValue_;
    case Type::STRING: return StrValue_ == lit->StrValue_;
    case Type::NIL: return true; // two nil objects are always equal
    }
    return false;
}

LiteralExpr* LiteralExpr::clone() const
{
    switch (Type_) {
    case Type::INTEGER:
    case Type::HEX:
    case Type::OCTAL:
    case Type::BINARY: return makeLiteralInt(IntValue_);
    case Type::FLOAT: return makeLiteralFloat(FloatValue_);
    case Type::BOOLEAN: return makeLiteralBool(BoolValue_);
    case Type::STRING: return makeLiteralString(StrValue_);
    case Type::NIL: return makeLiteralNil();
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

// name expr
bool NameExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    NameExpr const* name = dynamic_cast<NameExpr const*>(other);
    return Value_ == name->getValue();
}

NameExpr* NameExpr::clone() const { return makeName(Value_); }
StringRef NameExpr::getValue() const { return Value_; }

// list expr

Expr* ListExpr::operator[](size_t const i) { return Elements_[i]; }
Expr const* ListExpr::operator[](size_t const i) const { return Elements_[i]; }

bool ListExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    ListExpr const* list = dynamic_cast<ListExpr const*>(other);

    if (Elements_.size() != list->Elements_.size())
        return false;

    for (size_t i = 0; i < Elements_.size(); ++i)
        if (!Elements_[i]->equals(list->Elements_[i]))
            return false;

    return true;
}

ListExpr* ListExpr::clone() const { return makeList(Elements_); }
std::vector<Expr*> const& ListExpr::getElements() const { return Elements_; }
std::vector<Expr*>& ListExpr::getElements() { return Elements_; }
bool ListExpr::isEmpty() const { return Elements_.empty(); }
size_t ListExpr::size() const { return Elements_.size(); }

// call expr
bool CallExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    CallExpr const* call = dynamic_cast<CallExpr const*>(other);
    return Callee_->equals(call->getCallee())
        && Args_->equals(call->getArgsAsListExpr())
        && CallLocation_ == call->getCallLocation();
}

CallExpr* CallExpr::clone() const { return makeCall(Callee_->clone(), Args_->clone()); }
Expr* CallExpr::getCallee() const { return Callee_; }

std::vector<Expr*> const& CallExpr::getArgs() const
{
    static std::vector<Expr*> const empty;
    return Args_ ? Args_->getElements() : empty;
}

std::vector<Expr*>& CallExpr::getArgs()
{
    assert(Args_ && "Attempting to get mutable args when Args_ is null");
    return Args_->getElements();
}

ListExpr* CallExpr::getArgsAsListExpr() { return Args_; }
ListExpr const* CallExpr::getArgsAsListExpr() const { return Args_; }
typename CallExpr::CallLocation CallExpr::getCallLocation() const { return CallLocation_; }
bool CallExpr::hasArguments() const { return Args_ && !Args_->isEmpty(); }

// assignment expr
bool AssignmentExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    AssignmentExpr const* bin = dynamic_cast<AssignmentExpr const*>(other);
    return Target_->equals(bin->getTarget()) && Value_->equals(bin->getValue());
}

AssignmentExpr* AssignmentExpr::clone() const { return makeAssignmentExpr(Target_->clone(), Value_->clone()); }
NameExpr* AssignmentExpr::getTarget() const { return Target_; }
Expr* AssignmentExpr::getValue() const { return Value_; }
bool AssignmentExpr::isDeclaration() const { return isDecl_; }

// block stmt
bool BlockStmt::equals(Stmt const* other) const
{
    if (!other || other->getKind() != Kind::BLOCK)
        return false;

    BlockStmt const* block = static_cast<BlockStmt const*>(other);

    if (Statements_.size() != block->Statements_.size())
        return false;

    for (size_t i = 0; i < Statements_.size(); ++i)
        if (!Statements_[i]->equals(block->Statements_[i]))
            return false;

    return true;
}

BlockStmt* BlockStmt::clone() const { return makeBlock(Statements_); }
std::vector<Stmt*> const& BlockStmt::getStatements() const { return Statements_; }
void BlockStmt::setStatements(std::vector<Stmt*>& stmts) { Statements_ = stmts; }
bool BlockStmt::isEmpty() const { return Statements_.empty(); }

// expr stmt

bool ExprStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    ExprStmt const* block = dynamic_cast<ExprStmt const*>(other);
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

    AssignmentStmt const* block = dynamic_cast<AssignmentStmt const*>(other);
    return Value_->equals(block->getValue()) && Target_->equals(block->getTarget());
}

AssignmentStmt* AssignmentStmt::clone() const { return makeAssignmentStmt(Target_->clone(), Target_->clone()); }
Expr* AssignmentStmt::getValue() const { return Value_; }
Expr* AssignmentStmt::getTarget() const { return Target_; }
void AssignmentStmt::setValue(Expr* v) { Value_ = v; }
void AssignmentStmt::setTarget(NameExpr* t) { Target_ = t; }
bool AssignmentStmt::isDeclaration() const { return isDecl_; }

// if stmt

bool IfStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    IfStmt const* block = dynamic_cast<IfStmt const*>(other);
    return Condition_->equals(block->getCondition())
        && ThenBlock_->equals(block->getThenBlock())
        && ElseBlock_->equals(block->getElseBlock());
}

IfStmt* IfStmt::clone() const { return makeIf(Condition_->clone(), ThenBlock_->clone(), ElseBlock_->clone()); }
Expr* IfStmt::getCondition() const { return Condition_; }
BlockStmt* IfStmt::getThenBlock() const { return ThenBlock_; }
BlockStmt* IfStmt::getElseBlock() const { return ElseBlock_; }
void IfStmt::setThenBlock(BlockStmt* t) { ThenBlock_ = t; }
void IfStmt::setElseBlock(BlockStmt* e) { ElseBlock_ = e; }

// while stmt

bool WhileStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    WhileStmt const* block = dynamic_cast<WhileStmt const*>(other);
    return Condition_->equals(block->getCondition()) && Block_->equals(block->getBlock());
}

WhileStmt* WhileStmt::clone() const { return makeWhile(Condition_->clone(), Block_->clone()); }
Expr* WhileStmt::getCondition() const { return Condition_; }
BlockStmt* WhileStmt::getBlock() { return Block_; }
BlockStmt const* WhileStmt::getBlock() const { return Block_; }
void WhileStmt::setBlock(BlockStmt* b) { Block_ = b; }

// for stmt
bool ForStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    ForStmt const* block = dynamic_cast<ForStmt const*>(other);
    return Target_->equals(block->getTarget())
        && Iter_->equals(block->getIter())
        && Block_->equals(block->getBlock());
}

ForStmt* ForStmt::clone() const { return AST_allocator.allocateObject<ForStmt>(Target_->clone(), Iter_->clone(), Block_->clone()); }
NameExpr* ForStmt::getTarget() const { return Target_; }
Expr* ForStmt::getIter() const { return Iter_; }
BlockStmt* ForStmt::getBlock() const { return Block_; }
void ForStmt::setBlock(BlockStmt* b) { Block_ = b; }

// function def

bool FunctionDef::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    FunctionDef const* block = dynamic_cast<FunctionDef const*>(other);
    return Name_->equals(block->getName())
        && Params_->equals(block->getParameterList())
        && Body_->equals(block->getBody());
}

FunctionDef* FunctionDef::clone() const { return makeFunction(Name_->clone(), Params_->clone(), Body_->clone()); }
NameExpr* FunctionDef::getName() const { return Name_; }
std::vector<Expr*> const& FunctionDef::getParameters() const { return Params_->getElements(); }
ListExpr* FunctionDef::getParameterList() const { return Params_; }
BlockStmt* FunctionDef::getBody() const { return Body_; }
void FunctionDef::setBody(BlockStmt* b) { Body_ = b; }
bool FunctionDef::hasParameters() const { return Params_ && !Params_->isEmpty(); }

// return stmt

bool ReturnStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    ReturnStmt const* block = dynamic_cast<ReturnStmt const*>(other);
    return Value_->equals(block->getValue());
}

ReturnStmt* ReturnStmt::clone() const { return makeReturn(Value_->clone()); }
Expr const* ReturnStmt::getValue() const { return Value_; }
void ReturnStmt::setValue(Expr* v) { Value_ = v; }
bool ReturnStmt::hasValue() const { return Value_ != nullptr; }

}
}
