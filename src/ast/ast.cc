// ast_node.cc

#include "../../include/ast/ast.hpp"

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

inline BinaryExpr* BinaryExpr::clone() const { return makeBinary(Left_->clone(), Right_->clone(), Operator_); }
inline Expr* BinaryExpr::getLeft() const { return Left_; }
inline Expr* BinaryExpr::getRight() const { return Right_; }
inline BinaryOp BinaryExpr::getOperator() const { return Operator_; }
inline void BinaryExpr::setLeft(Expr* l) { Left_ = l; }
inline void BinaryExpr::setRight(Expr* r) { Right_ = r; }
inline void BinaryExpr::setOperator(BinaryOp op) { Operator_ = op; }

// unary expr
bool UnaryExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    UnaryExpr const* un = dynamic_cast<UnaryExpr const*>(other);
    return Operator_ == un->getOperator() && Operand_->equals(un->getOperand());
}

inline UnaryExpr* UnaryExpr::clone() const { return makeUnary(Operand_->clone(), Operator_); }
inline Expr* UnaryExpr::getOperand() const { return Operand_; }
inline UnaryOp UnaryExpr::getOperator() const { return Operator_; }

// literal expr
inline typename LiteralExpr::Type LiteralExpr::getType() const { return Type_; }

inline int64_t LiteralExpr::getInt() const
{
    assert(isInteger());
    return IntValue_;
}

inline double LiteralExpr::getFloat() const
{
    assert(isDecimal());
    return FloatValue_;
}

inline bool LiteralExpr::getBool() const
{
    assert(isBoolean());
    return BoolValue_;
}

inline StringRef LiteralExpr::getStr() const
{
    assert(isString());
    return StrValue_;
}

inline bool LiteralExpr::isInteger() const
{
    return Type_ == Type::INTEGER
        || Type_ == Type::HEX
        || Type_ == Type::OCTAL
        || Type_ == Type::BINARY;
}

inline bool LiteralExpr::isDecimal() const { return Type_ == Type::FLOAT; }
inline bool LiteralExpr::isBoolean() const { return Type_ == Type::BOOLEAN; }
inline bool LiteralExpr::isString() const { return Type_ == Type::STRING; }
inline bool LiteralExpr::isNumeric() const { return isInteger() || isDecimal(); }
inline bool LiteralExpr::isNil() const { return Type_ == Type::NIL; }

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

inline NameExpr* NameExpr::clone() const { return makeName(Value_); }
inline StringRef NameExpr::getValue() const { return Value_; }

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

inline ListExpr* ListExpr::clone() const { return makeList(Elements_); }
inline std::vector<Expr*> const& ListExpr::getElements() const { return Elements_; }
inline std::vector<Expr*>& ListExpr::getElements() { return Elements_; }
inline bool ListExpr::isEmpty() const { return Elements_.empty(); }
inline size_t ListExpr::size() const { return Elements_.size(); }

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

inline CallExpr* CallExpr::clone() const { return makeCall(Callee_->clone(), Args_->clone()); }
inline Expr* CallExpr::getCallee() const { return Callee_; }

inline std::vector<Expr*> const& CallExpr::getArgs() const
{
    static std::vector<Expr*> const empty;
    return Args_ ? Args_->getElements() : empty;
}

inline std::vector<Expr*>& CallExpr::getArgs()
{
    assert(Args_ && "Attempting to get mutable args when Args_ is null");
    return Args_->getElements();
}

inline ListExpr* CallExpr::getArgsAsListExpr() { return Args_; }
inline ListExpr const* CallExpr::getArgsAsListExpr() const { return Args_; }
inline typename CallExpr::CallLocation CallExpr::getCallLocation() const { return CallLocation_; }
inline bool CallExpr::hasArguments() const { return Args_ && !Args_->isEmpty(); }

// assignment expr
bool AssignmentExpr::equals(Expr const* other) const
{
    if (other->getKind() != Kind_)
        return false;

    AssignmentExpr const* bin = dynamic_cast<AssignmentExpr const*>(other);
    return Target_->equals(bin->getTarget()) && Value_->equals(bin->getValue());
}

inline AssignmentExpr* AssignmentExpr::clone() const { return makeAssignmentExpr(Target_->clone(), Value_->clone()); }
inline NameExpr* AssignmentExpr::getTarget() const { return Target_; }
inline Expr* AssignmentExpr::getValue() const { return Value_; }
inline bool AssignmentExpr::isDeclaration() const { return isDecl_; }

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

inline BlockStmt* BlockStmt::clone() const { return makeBlock(Statements_); }
inline std::vector<Stmt*> const& BlockStmt::getStatements() const { return Statements_; }
inline void BlockStmt::setStatements(std::vector<Stmt*>& stmts) { Statements_ = stmts; }
inline bool BlockStmt::isEmpty() const { return Statements_.empty(); }

// expr stmt

bool ExprStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    ExprStmt const* block = dynamic_cast<ExprStmt const*>(other);
    return Expr_->equals(block->getExpr());
}

inline ExprStmt* ExprStmt::clone() const { return makeExprStmt(Expr_->clone()); }
inline Expr* ExprStmt::getExpr() const { return Expr_; }
inline void ExprStmt::setExpr(Expr* e) { Expr_ = e; }

// assignment stmt

bool AssignmentStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    AssignmentStmt const* block = dynamic_cast<AssignmentStmt const*>(other);
    return Value_->equals(block->getValue()) && Target_->equals(block->getTarget());
}

inline AssignmentStmt* AssignmentStmt::clone() const { return makeAssignmentStmt(Target_->clone(), Target_->clone()); }
inline Expr* AssignmentStmt::getValue() const { return Value_; }
inline Expr* AssignmentStmt::getTarget() const { return Target_; }
inline void AssignmentStmt::setValue(Expr* v) { Value_ = v; }
inline void AssignmentStmt::setTarget(NameExpr* t) { Target_ = t; }
inline bool AssignmentStmt::isDeclaration() const { return isDecl_; }

// if stmt

inline bool IfStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    IfStmt const* block = dynamic_cast<IfStmt const*>(other);
    return Condition_->equals(block->getCondition())
        && ThenBlock_->equals(block->getThenBlock())
        && ElseBlock_->equals(block->getElseBlock());
}

inline IfStmt* IfStmt::clone() const { return makeIf(Condition_->clone(), ThenBlock_->clone(), ElseBlock_->clone()); }
inline Expr* IfStmt::getCondition() const { return Condition_; }
inline BlockStmt* IfStmt::getThenBlock() const { return ThenBlock_; }
inline BlockStmt* IfStmt::getElseBlock() const { return ElseBlock_; }
inline void IfStmt::setThenBlock(BlockStmt* t) { ThenBlock_ = t; }
inline void IfStmt::setElseBlock(BlockStmt* e) { ElseBlock_ = e; }

// while stmt

bool WhileStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    WhileStmt const* block = dynamic_cast<WhileStmt const*>(other);
    return Condition_->equals(block->getCondition()) && Block_->equals(block->getBlock());
}

inline WhileStmt* WhileStmt::clone() const { return makeWhile(Condition_->clone(), Block_->clone()); }
inline Expr* WhileStmt::getCondition() const { return Condition_; }
inline BlockStmt* WhileStmt::getBlock() { return Block_; }
inline BlockStmt const* WhileStmt::getBlock() const { return Block_; }
inline void WhileStmt::setBlock(BlockStmt* b) { Block_ = b; }

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

inline ForStmt* ForStmt::clone() const { return AST_allocator.allocateObject<ForStmt>(Target_->clone(), Iter_->clone(), Block_->clone()); }
inline NameExpr* ForStmt::getTarget() const { return Target_; }
inline Expr* ForStmt::getIter() const { return Iter_; }
inline BlockStmt* ForStmt::getBlock() const { return Block_; }
inline void ForStmt::setBlock(BlockStmt* b) { Block_ = b; }

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

inline FunctionDef* FunctionDef::clone() const { return makeFunction(Name_->clone(), Params_->clone(), Body_->clone()); }
inline NameExpr* FunctionDef::getName() const { return Name_; }
inline std::vector<Expr*> const& FunctionDef::getParameters() const { return Params_->getElements(); }
inline ListExpr* FunctionDef::getParameterList() const { return Params_; }
inline BlockStmt* FunctionDef::getBody() const { return Body_; }
inline void FunctionDef::setBody(BlockStmt* b) { Body_ = b; }
inline bool FunctionDef::hasParameters() const { return Params_ && !Params_->isEmpty(); }

// return stmt

bool ReturnStmt::equals(Stmt const* other) const
{
    if (Kind_ != other->getKind())
        return false;

    ReturnStmt const* block = dynamic_cast<ReturnStmt const*>(other);
    return Value_->equals(block->getValue());
}

inline ReturnStmt* ReturnStmt::clone() const { return makeReturn(Value_->clone()); }
inline Expr const* ReturnStmt::getValue() const { return Value_; }
inline void ReturnStmt::setValue(Expr* v) { Value_ = v; }
inline bool ReturnStmt::hasValue() const { return Value_ != nullptr; }

// makers
static constexpr BinaryExpr* makeBinary(Expr* l, Expr* r, BinaryOp const op) { return AST_allocator.allocateObject<BinaryExpr>(l, r, op); }
static constexpr UnaryExpr* makeUnary(Expr* operand, UnaryOp const op) { return AST_allocator.allocateObject<UnaryExpr>(operand, op); }
static constexpr LiteralExpr* makeLiteralNil() { return AST_allocator.allocateObject<LiteralExpr>(); }
static constexpr LiteralExpr* makeLiteralInt(int value) { return AST_allocator.allocateObject<LiteralExpr>(static_cast<int64_t>(value), LiteralExpr::Type::INTEGER); }
static constexpr LiteralExpr* makeLiteralInt(int64_t value) { return AST_allocator.allocateObject<LiteralExpr>(value, LiteralExpr::Type::INTEGER); }
static constexpr LiteralExpr* makeLiteralFloat(double value) { return AST_allocator.allocateObject<LiteralExpr>(value, LiteralExpr::Type::FLOAT); }
static constexpr LiteralExpr* makeLiteralString(StringRef value) { return AST_allocator.allocateObject<LiteralExpr>(value); }
static constexpr LiteralExpr* makeLiteralBool(bool value) { return AST_allocator.allocateObject<LiteralExpr>(value); }
static constexpr NameExpr* makeName(StringRef const str) { return AST_allocator.allocateObject<NameExpr>(str); }
static constexpr ListExpr* makeList(std::vector<Expr*> elements) { return AST_allocator.allocateObject<ListExpr>(elements); }
static constexpr CallExpr* makeCall(Expr* callee, ListExpr* args) { return AST_allocator.allocateObject<CallExpr>(callee, args); }
static constexpr AssignmentExpr* makeAssignmentExpr(NameExpr* target, Expr* value, bool decl) { return AST_allocator.allocateObject<AssignmentExpr>(target, value, decl); }
static constexpr BlockStmt* makeBlock(std::vector<Stmt*> stmts) { return AST_allocator.allocateObject<BlockStmt>(stmts); }
static constexpr ExprStmt* makeExprStmt(Expr* expr) { return AST_allocator.allocateObject<ExprStmt>(expr); }
static constexpr AssignmentStmt* makeAssignmentStmt(Expr* target, Expr* value, bool decl) { return AST_allocator.allocateObject<AssignmentStmt>(target, value, decl); }
static constexpr IfStmt* makeIf(Expr* condition, BlockStmt* then_block, BlockStmt* else_block) { return AST_allocator.allocateObject<IfStmt>(condition, then_block, else_block); }
static constexpr WhileStmt* makeWhile(Expr* condition, BlockStmt* block) { return AST_allocator.allocateObject<WhileStmt>(condition, block); }
static constexpr ForStmt* makeFor(NameExpr* target, Expr* iter, BlockStmt* block) { return AST_allocator.allocateObject<ForStmt>(target, iter, block); }
static constexpr FunctionDef* makeFunction(NameExpr* name, ListExpr* params, BlockStmt* body) { return AST_allocator.allocateObject<FunctionDef>(name, params, body); }
static constexpr ReturnStmt* makeReturn(Expr* value) { return AST_allocator.allocateObject<ReturnStmt>(value); }

}
}