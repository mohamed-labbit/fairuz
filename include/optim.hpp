#ifndef OPTIM_HPP
#define OPTIM_HPP

#include "ast.hpp"
#include "value.hpp"

namespace mylang::runtime {

using namespace ast;

static std::optional<Value> constValue(Expr const *e) {
  if (!e || e->getKind() != Expr::Kind::LITERAL)
    return std::nullopt;

  LiteralExpr const *lit = static_cast<LiteralExpr const *>(e);

  if (lit->isNil())
    return NIL_VAL;
  if (lit->isBoolean())
    return MAKE_BOOL(lit->getBool());
  if (lit->isInteger())
    return MAKE_INTEGER(lit->getInt());
  if (lit->isDecimal())
    return MAKE_REAL(lit->getFloat());

  return std::nullopt;
}

static std::optional<Value> tryFoldUnary(UnaryExpr const *e) {
  std::optional<Value> cv = constValue(e->getOperand());
  if (!cv)
    return std::nullopt;

  switch (e->getOperator()) {
  case UnaryOp::OP_NEG:
    if (IS_INTEGER(*cv))
      return IS_INTEGER(*cv) ? MAKE_INTEGER(-AS_INTEGER(*cv)) : MAKE_REAL(-AS_DOUBLE(*cv));

    return std::nullopt;
  case UnaryOp::OP_NOT:
    return MAKE_BOOL(!IS_TRUTHY(*cv));
  case UnaryOp::OP_BITNOT:
    if (IS_INTEGER(*cv))
      return MAKE_INTEGER(~AS_INTEGER(*cv));

    return std::nullopt;
  default:
    return std::nullopt;
  }
}

static std::optional<Value> _tryFoldBinary(BinaryExpr const *e) {
  auto L = constValue(e->getLeft());
  auto R = constValue(e->getRight());

  if (!L || !R)
    return std::nullopt;

  BinaryOp op = e->getOperator();

  if (op == BinaryOp::OP_EQ)
    return MAKE_BOOL(*L == *R);

  if (op == BinaryOp::OP_NEQ)
    return MAKE_BOOL(*L != *R);

  if (!IS_INTEGER(*L) || !IS_INTEGER(*R))
    return std::nullopt;

  bool both_ints = IS_INTEGER(*L) && IS_INTEGER(*R);

  double ld = AS_DOUBLE(*L);
  double rd = AS_DOUBLE(*R);

  int64_t li = IS_INTEGER(*L) ? AS_INTEGER(*L) : static_cast<int64_t>(AS_DOUBLE(*L));
  int64_t ri = IS_INTEGER(*R) ? AS_INTEGER(*R) : static_cast<int64_t>(AS_DOUBLE(*R));

  switch (op) {
  case BinaryOp::OP_ADD:
    return both_ints ? MAKE_INTEGER(li + ri) : MAKE_REAL(ld + rd);
  case BinaryOp::OP_SUB:
    return both_ints ? MAKE_INTEGER(li - ri) : MAKE_REAL(ld - rd);
  case BinaryOp::OP_MUL:
    return both_ints ? MAKE_INTEGER(li * ri) : MAKE_REAL(ld * rd);
  case BinaryOp::OP_DIV:
    if (rd == 0.0)
      return std::nullopt;

    return MAKE_REAL(ld / rd);
  case BinaryOp::OP_MOD: {
    if (rd == 0.0)
      return std::nullopt;

    if (both_ints)
      return MAKE_INTEGER(li % ri);

    return MAKE_REAL(std::fmod(ld, rd));
  }
  case BinaryOp::OP_POW:
    return MAKE_REAL(std::pow(ld, rd));
  case BinaryOp::OP_LT:
    return MAKE_BOOL(ld < rd);
  case BinaryOp::OP_GT:
    return MAKE_BOOL(ld > rd);
  case BinaryOp::OP_LTE:
    return MAKE_BOOL(ld <= rd);
  case BinaryOp::OP_GTE:
    return MAKE_BOOL(ld >= rd);
  case BinaryOp::OP_BITAND:
    return both_ints ? MAKE_INTEGER(li & ri) : std::optional<Value>{};
  case BinaryOp::OP_BITOR:
    return both_ints ? MAKE_INTEGER(li | ri) : std::optional<Value>{};
  case BinaryOp::OP_BITXOR:
    return both_ints ? MAKE_INTEGER(li ^ ri) : std::optional<Value>{};
  case BinaryOp::OP_LSHIFT:
    if (!both_ints || ri < 0 || ri >= 64)
      return std::nullopt;

    return MAKE_INTEGER(li << ri);
  case BinaryOp::OP_RSHIFT:
    if (!both_ints || ri < 0 || ri >= 64)
      return std::nullopt;

    return MAKE_INTEGER(li >> ri);
  default:
    return std::nullopt;
  }
}

static std::optional<Value> tryFoldBinary(BinaryExpr const *e) {
  if (!e)
    return std::nullopt;

  Expr *LE = e->getLeft();
  Expr *RE = e->getRight();

  if (!LE || !RE)
    return std::nullopt;

  if (LE->getKind() == Expr::Kind::LITERAL && RE->getKind() == Expr::Kind::LITERAL)
    return _tryFoldBinary(e);

  std::optional<Value> L, R;

  if (LE->getKind() == Expr::Kind::BINARY)
    L = tryFoldBinary(static_cast<BinaryExpr const *>(LE));
  else if (LE->getKind() == Expr::Kind::UNARY)
    L = tryFoldUnary(static_cast<UnaryExpr const *>(LE));

  if (RE->getKind() == Expr::Kind::BINARY)
    R = tryFoldBinary(static_cast<BinaryExpr const *>(RE));
  else if (RE->getKind() == Expr::Kind::UNARY)
    R = tryFoldUnary(static_cast<UnaryExpr const *>(RE));

  if (!R && !L)
    return std::nullopt;

  BinaryExpr *ce = e->clone();

  auto makeLiteralFromVal = [](Value const v) {
    if (IS_DOUBLE(v))
      return makeLiteralFloat(AS_DOUBLE(v));
    if (IS_INTEGER(v))
      return makeLiteralInt(AS_INTEGER(v));
    if (IS_BOOL(v))
      return makeLiteralBool(AS_BOOL(v));

    return makeLiteralNil();
  };

  if (L)
    ce->setLeft(makeLiteralFromVal(*L));
  if (R)
    ce->setRight(makeLiteralFromVal(*R));

  return _tryFoldBinary(ce);
}

static std::optional<Value> tryFoldExpr(Expr const *e) {
  if (!e)
    return std::nullopt;

  switch (e->getKind()) {
  case Expr::Kind::LITERAL:
    return constValue(e);
  case Expr::Kind::UNARY:
    return tryFoldUnary(static_cast<UnaryExpr const *>(e));
  case Expr::Kind::BINARY:
    return tryFoldBinary(static_cast<BinaryExpr const *>(e));
  default:
    return std::nullopt;
  }
}

} // namespace mylang::runtime

#endif // OPTIM_HPP
