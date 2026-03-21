#include "../include/vm.hpp"

#include <gtest/gtest.h>

using namespace mylang;
using namespace mylang::runtime;

static constexpr uint16_t BX(int v) { return static_cast<uint16_t>(v + 32767); }

struct CB {
  Chunk *ch{nullptr};

  CB() {
    ch = makeChunk();
    ch->name = "<test>";
  }

  CB &ABC(OpCode op, uint8_t A, uint8_t B, uint8_t C, uint32_t ln = 1) {
    ch->emit(make_ABC(op, A, B, C), {ln, 0, 0});
    return *this;
  }
  CB &ABx(OpCode op, uint8_t A, uint16_t Bx, uint32_t ln = 1) {
    ch->emit(make_ABx(op, A, Bx), {ln, 0, 0});
    return *this;
  }
  CB &AsBx(OpCode op, uint8_t A, int sBx, uint32_t ln = 1) {
    ch->emit(make_AsBx(op, A, sBx), {ln, 0, 0});
    return *this;
  }

  CB &load_int(uint8_t r, int v) { return ABx(OpCode::LOAD_INT, r, BX(v)); }
  CB &ret(uint8_t r, uint8_t n = 1) { return ABC(OpCode::RETURN, r, n, 0); }
  CB &ret_nil() { return ABC(OpCode::RETURN_NIL, 0, 0, 0); }
  CB &nop(uint8_t slot = 0) { return ABC(OpCode::NOP, slot, 0, 0); }
  CB &mov(uint8_t d, uint8_t s) { return ABC(OpCode::MOVE, d, s, 0); }

  uint16_t str(char const *s) {
    strs_.emplace_back(std::make_unique<ObjString>(StringRef(s)));
    return ch->addConstant(MAKE_OBJECT(strs_.back().get()));
  }

  CB &ldg(uint8_t r, char const *name) { return ABx(OpCode::LOAD_GLOBAL, r, str(name)); }
  CB &stg(uint8_t r, char const *name) { return ABx(OpCode::STORE_GLOBAL, r, str(name)); }

  CB &regs(int n) {
    ch->localCount = n;
    return *this;
  }
  CB &slot() {
    ch->allocIcSlot();
    return *this;
  }

  void dump() const {
    std::cout << "Disassemebeled chunk:" << '\n';
    if (ch)
      ch->disassemble();
    std::cout << '\n';
  }

private:
  std::vector<std::unique_ptr<ObjString>> strs_;
};

struct VMRunner {
  VM vm;
  Chunk *chunk_ = makeChunk();

  Value run(CB &b) {
    chunk_ = b.ch;
    return vm.run(chunk_);
  }
  Value run(Chunk *c) {
    chunk_ = c;
    return vm.run(chunk_);
  }
};

TEST(VMLoads, Nil) {
  VMRunner r;
  CB b;
  b.regs(1).ABC(OpCode::LOAD_NIL, 0, 0, 1).ret(0);
  b.dump();
  EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMLoads, NilFillsMultiple) {
  VMRunner r;
  CB b;
  b.regs(3).ABC(OpCode::LOAD_NIL, 0, 0, 3).ret(2);
  b.dump();
  EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMLoads, True) {
  VMRunner r;
  CB b;
  b.regs(1).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMLoads, False) {
  VMRunner r;
  CB b;
  b.regs(1).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMLoads, IntPositive) {
  VMRunner r;
  CB b;
  b.regs(1).load_int(0, 42).ret(0);
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), 42);
}

TEST(VMLoads, IntZero) {
  VMRunner r;
  CB b;
  b.regs(1).load_int(0, 0).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), 0);
}

TEST(VMLoads, IntNegative) {
  VMRunner r;
  CB b;
  b.regs(1).load_int(0, -100).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), -100);
}

TEST(VMLoads, IntMaxEncodable) {
  VMRunner r;
  CB b;
  b.regs(1).ABx(OpCode::LOAD_INT, 0, 65535).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), 32768);
}

TEST(VMLoads, IntMinEncodable) {
  VMRunner r;
  CB b;
  b.regs(1).ABx(OpCode::LOAD_INT, 0, 0).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), -32767);
}

TEST(VMLoads, ConstDouble) {
  VMRunner r;
  CB b;
  b.regs(1);
  uint16_t k = b.ch->addConstant(MAKE_REAL(3.14));
  b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_DOUBLE(v));
  EXPECT_DOUBLE_EQ(AS_DOUBLE(v), 3.14);
}

TEST(VMLoads, ConstString) {
  VMRunner r;
  CB b;
  b.regs(1);
  uint16_t k = b.str("hello");
  b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_STRING(v));
  EXPECT_EQ(AS_STRING(v)->str, "hello");
}

TEST(VMLoads, ConstLargeInt) {
  VMRunner r;
  CB b;
  b.regs(1);
  uint16_t k = b.ch->addConstant(MAKE_INTEGER(1000000LL));
  b.ABx(OpCode::LOAD_CONST, 0, k).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), 1000000LL);
}

TEST(VMLoads, ReturnNil) {
  VMRunner r;
  CB b;
  b.regs(1).ret_nil();
  EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMMove, Copies) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 77).mov(1, 0).ret(1);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), 77);
}

TEST(VMMove, SourceUnchanged) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 55).mov(1, 0).ret(0);
  b.dump();
  Value v = r.run(b);
  EXPECT_EQ(AS_INTEGER(v), 55);
}

TEST(VMArith, AddIntFastPath) {
  VMRunner r;
  CB b;
  b.regs(3).slot().load_int(0, 10).load_int(1, 32).ABC(OpCode::OP_ADD, 2, 0, 1).nop().ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_INTEGER(v));
  EXPECT_EQ(AS_INTEGER(v), 42);
}

TEST(VMArith, AddDoubles) {
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t k0 = b.ch->addConstant(MAKE_REAL(1.5));
  uint16_t k1 = b.ch->addConstant(MAKE_REAL(2.5));
  b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_DOUBLE(v));
  EXPECT_DOUBLE_EQ(AS_DOUBLE(v), 4.0);
}

TEST(VMArith, AddStringsConcat) {
  GTEST_SKIP() << "Not supported yet.";
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t k0 = b.str("foo");
  uint16_t k1 = b.str("bar");
  b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_ADD, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_STRING(v));
  EXPECT_EQ(AS_STRING(v)->str, "foobar");
}

TEST(VMArith, SubInt) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 100).load_int(1, 58).ABC(OpCode::OP_SUB, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 42);
}

TEST(VMArith, MulInt) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 6).load_int(1, 7).ABC(OpCode::OP_MUL, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 42);
}

TEST(VMArith, DivDouble) {
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t k0 = b.ch->addConstant(MAKE_REAL(84.0));
  uint16_t k1 = b.ch->addConstant(MAKE_REAL(2.0));
  b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), 42.0);
}

TEST(VMArith, ModPositive) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 17).load_int(1, 5).ABC(OpCode::OP_MOD, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), 2.0);
}

TEST(VMArith, Pow) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 2).load_int(1, 10).ABC(OpCode::OP_POW, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), 1024.0);
}

TEST(VMArith, NegInt) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 7).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), -7);
}

TEST(VMArith, NegDouble) {
  VMRunner r;
  CB b;
  b.regs(2);
  uint16_t k = b.ch->addConstant(MAKE_REAL(3.5));
  b.ABx(OpCode::LOAD_CONST, 0, k).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_DOUBLE_EQ(AS_DOUBLE(r.run(b)), -3.5);
}

TEST(VMArith, DivByZeroThrows) {
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t k0 = b.ch->addConstant(MAKE_REAL(1.0));
  uint16_t k1 = b.ch->addConstant(MAKE_REAL(0.0));
  b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_DIV, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMArith, NegOnStringThrows) {
  VMRunner r;
  CB b;
  b.regs(2).ABx(OpCode::LOAD_CONST, 0, b.str("x")).ABC(OpCode::OP_NEG, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMBitwise, And) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 0b1111).load_int(1, 0b1010).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 0b1010);
}

TEST(VMBitwise, Or) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 0b1100).load_int(1, 0b0011).ABC(OpCode::OP_BITOR, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 0b1111);
}

TEST(VMBitwise, Xor) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 0b1111).load_int(1, 0b0101).ABC(OpCode::OP_BITXOR, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 0b1010);
}

TEST(VMBitwise, Not) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 0).ABC(OpCode::OP_BITNOT, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), ~int64_t(0));
}

TEST(VMBitwise, Shl) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 1).load_int(1, 8).ABC(OpCode::OP_LSHIFT, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 256);
}

TEST(VMBitwise, Shr) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 1024).load_int(1, 3).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 128);
}

TEST(VMBitwise, ShrLogical_NegativeInput) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, -8).load_int(1, 1).ABC(OpCode::OP_RSHIFT, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_DOUBLE(v));
  EXPECT_GT(AS_DOUBLE(v), 0.0);
}

TEST(VMBitwise, AndOnDoubleThrows) {
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t k = b.ch->addConstant(MAKE_REAL(1.0));
  b.ABx(OpCode::LOAD_CONST, 0, k).load_int(1, 1).ABC(OpCode::OP_BITAND, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCompare, EqIntsTrue) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, EqIntsFalse) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, NeqTrue) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 5).load_int(1, 6).ABC(OpCode::OP_NEQ, 2, 0, 1).ret(2);
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LtTrue) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 3).load_int(1, 7).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LtFalseEqual) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, LeTrue_Equal) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 5).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LeFalse) {
  VMRunner r;
  CB b;
  b.regs(3).load_int(0, 6).load_int(1, 5).ABC(OpCode::OP_LTE, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, EqSameString) {
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t k0 = b.str("hello");
  uint16_t k1 = b.str("hello");
  b.ABx(OpCode::LOAD_CONST, 0, k0).ABx(OpCode::LOAD_CONST, 1, k1).ABC(OpCode::OP_EQ, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, NotFalseIsTrue) {
  VMRunner r;
  CB b;
  b.regs(2).ABC(OpCode::LOAD_FALSE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, NotTrueIsFalse) {
  VMRunner r;
  CB b;
  b.regs(2).ABC(OpCode::LOAD_TRUE, 0, 0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && !AS_BOOL(v));
}

TEST(VMCompare, NotNilIsTrue) {
  VMRunner r;
  CB b;
  b.regs(2).ABC(OpCode::LOAD_NIL, 0, 0, 1).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, NotZeroIsTrue) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 0).ABC(OpCode::OP_NOT, 1, 0, 0).ret(1);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMCompare, LtStrings) {
  VMRunner r;
  CB b;
  b.regs(3);
  uint16_t ka = b.str("apple");
  uint16_t kb = b.str("banana");
  b.ABx(OpCode::LOAD_CONST, 0, ka).ABx(OpCode::LOAD_CONST, 1, kb).ABC(OpCode::OP_LT, 2, 0, 1).ret(2);
  b.dump();
  Value v = r.run(b);
  EXPECT_TRUE(IS_BOOL(v) && AS_BOOL(v));
}

TEST(VMGlobals, StoreAndLoad) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 123).stg(0, "g").load_int(0, 0).ldg(1, "g").ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 123);
}

TEST(VMGlobals, MissingGlobalIsNil) {
  VMRunner r;
  CB b;
  b.regs(1).ldg(0, "nope").ret(0);
  EXPECT_TRUE(IS_NIL(r.run(b)));
}

TEST(VMGlobals, Overwrite) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 1).stg(0, "x").load_int(0, 2).stg(0, "x").ldg(1, "x").ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 2);
}

TEST(VMLists, NewEmpty) {
  VMRunner r;
  CB b;
  b.regs(2).ABC(OpCode::LIST_NEW, 0, 0, 0).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 0);
}

TEST(VMLists, AppendAndLen) {
  VMRunner r;
  CB b;
  b.regs(2).ABC(OpCode::LIST_NEW, 0, 3, 0);
  for (int v : {10, 20, 30})
    b.load_int(1, v).ABC(OpCode::LIST_APPEND, 0, 1, 0);
  b.ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 3);
}

TEST(VMLists, GetFirst) {
  VMRunner r;
  CB b;
  b.regs(3)
      .ABC(OpCode::LIST_NEW, 0, 2, 0)
      .load_int(1, 77)
      .ABC(OpCode::LIST_APPEND, 0, 1, 0)
      .load_int(1, 88)
      .ABC(OpCode::LIST_APPEND, 0, 1, 0)
      .load_int(1, 0)
      .ABC(OpCode::LIST_GET, 2, 0, 1)
      .ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 77);
}

TEST(VMLists, GetLast) {
  VMRunner r;
  CB b;
  b.regs(3)
      .ABC(OpCode::LIST_NEW, 0, 2, 0)
      .load_int(1, 10)
      .ABC(OpCode::LIST_APPEND, 0, 1, 0)
      .load_int(1, 20)
      .ABC(OpCode::LIST_APPEND, 0, 1, 0)
      .load_int(1, 1)
      .ABC(OpCode::LIST_GET, 2, 0, 1)
      .ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 20);
}

TEST(VMLists, Set) {
  VMRunner r;
  CB b;
  b.regs(3)
      .ABC(OpCode::LIST_NEW, 0, 1, 0)
      .load_int(1, 0)
      .ABC(OpCode::LIST_APPEND, 0, 1, 0)
      .load_int(1, 0)
      .load_int(2, 99)
      .ABC(OpCode::LIST_SET, 0, 1, 2)
      .load_int(1, 0)
      .ABC(OpCode::LIST_GET, 2, 0, 1)
      .ret(2);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 99);
}

TEST(VMLists, OutOfBoundsThrows) {
  VMRunner r;
  CB b;
  b.regs(3).ABC(OpCode::LIST_NEW, 0, 0, 0).load_int(1, 0).ABC(OpCode::LIST_GET, 2, 0, 1).ret(2);
  b.dump();
  EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMLists, LenOnNonListThrows) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 5).ABC(OpCode::LIST_LEN, 1, 0, 0).ret(1);
  b.dump();
  EXPECT_THROW(r.run(b), std::runtime_error);
}

static Chunk *make_adder_chunk() {
  auto fn = makeChunk();
  fn->name = "add2";
  fn->arity = 2;
  fn->localCount = 3;
  // r0=a r1=b (params); r2=a+b
  fn->emit(make_ABC(OpCode::OP_ADD, 2, 0, 1), {});
  fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), {});
  return fn;
}

TEST(VMCalls, CallClosure_TwoArgs) {
  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 4;
  top->functions.push(make_adder_chunk());

  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(3)), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(4)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 2, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();
  VMRunner r;
  EXPECT_EQ(AS_INTEGER(r.run(top)), 7);
}

TEST(VMCalls, WrongArgcThrows) {
  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 3;
  top->functions.push(make_adder_chunk());

  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();
  VMRunner r;
  EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMCalls, CallNonFunctionThrows) {
  VMRunner r;
  CB b;
  b.regs(2).load_int(0, 5).ABC(OpCode::CALL, 0, 0, 0).ret(0);
  b.dump();
  EXPECT_THROW(r.run(b), std::runtime_error);
}

TEST(VMCalls, ICCallNativeLen) {
  VMRunner r;
  CB b;
  b.regs(3)
      .slot()
      .ABC(OpCode::LIST_NEW, 0, 3, 0)
      .load_int(2, 1)
      .ABC(OpCode::LIST_APPEND, 0, 2, 0)
      .load_int(2, 2)
      .ABC(OpCode::LIST_APPEND, 0, 2, 0)
      .load_int(2, 3)
      .ABC(OpCode::LIST_APPEND, 0, 2, 0)
      .ldg(1, "len")
      .mov(2, 0)
      .ABC(OpCode::IC_CALL, 1, 1, 0)
      .ret(1);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames) {
  auto fn = makeChunk();
  fn->name = "cd";
  fn->arity = 1;
  fn->localCount = 4;

  uint16_t nk = fn->addConstant(MAKE_OBJECT(MAKE_OBJ_STRING("cd")));

  fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(0)), {});
  fn->emit(make_ABC(OpCode::OP_EQ, 1, 0, 1), {});
  fn->emit(make_AsBx(OpCode::JUMP_IF_FALSE, 1, 1), {});
  fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), {});
  fn->emit(make_ABC(OpCode::OP_SUB, 0, 0, 1), {});
  fn->emit(make_ABx(OpCode::LOAD_GLOBAL, 2, nk), {});
  fn->emit(make_ABC(OpCode::MOVE, 3, 0, 0), {});
  fn->emit(make_ABC(OpCode::CALL_TAIL, 2, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 3;
  top->functions.push(fn);

  uint16_t tk = top->addConstant(MAKE_OBJECT(MAKE_OBJ_STRING("cd")));
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(300)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();
  VMRunner r;
  Value v = r.run(std::move(top));
  EXPECT_EQ(AS_INTEGER(v), 0);
}

TEST(VMCalls, StackOverflowDetected) {
  auto fn = makeChunk();
  fn->name = "inf";
  fn->arity = 0;
  fn->localCount = 1;
  uint16_t nk = fn->addConstant(MAKE_OBJECT(MAKE_OBJ_STRING("inf")));
  fn->emit(make_ABx(OpCode::LOAD_GLOBAL, 0, nk), {});
  fn->emit(make_ABC(OpCode::CALL, 0, 0, 0), {});
  fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 1;
  top->functions.push(fn);
  uint16_t tk = top->addConstant(MAKE_OBJECT(MAKE_OBJ_STRING("inf")));
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), {});
  top->emit(make_ABC(OpCode::CALL, 0, 0, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();
  VMRunner r;
  EXPECT_THROW(r.run(top), std::runtime_error);
}

TEST(VMClosures, CaptureLocal_GetUpvalue) {
  auto inner = makeChunk();
  inner->name = "inner";
  inner->arity = 0;
  inner->upvalueCount = 1;
  inner->localCount = 1;
  inner->emit(make_ABC(OpCode::GET_UPVALUE, 0, 0, 0), {});
  inner->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});

  auto outer = makeChunk();
  outer->name = "outer";
  outer->arity = 0;
  outer->localCount = 2;
  outer->upvalueCount = 0;
  outer->functions.push(inner);
  outer->emit(make_ABx(OpCode::LOAD_INT, 0, BX(10)), {});
  outer->emit(make_ABx(OpCode::CLOSURE, 1, 0), {});
  outer->emit(make_ABC(OpCode::MOVE, 1, 0, 0), {});
  outer->emit(make_ABC(OpCode::CALL, 1, 0, 0), {});
  outer->emit(make_ABC(OpCode::RETURN, 1, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 2;
  top->functions.push(outer);
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABC(OpCode::CALL, 0, 0, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();
  VMRunner r;
  EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 10);
}

TEST(VMClosures, CloseUpvalue_SurvivesFrame) {
  auto add = makeChunk();
  add->name = "add";
  add->arity = 1;
  add->upvalueCount = 1;
  add->localCount = 3;
  add->emit(make_ABC(OpCode::GET_UPVALUE, 1, 0, 0), {});
  add->emit(make_ABC(OpCode::OP_ADD, 2, 1, 0), {});
  add->emit(make_ABC(OpCode::RETURN, 2, 1, 0), {});

  auto ma = makeChunk();
  ma->name = "ma";
  ma->arity = 1;
  ma->localCount = 2;
  ma->upvalueCount = 0;
  ma->functions.push(add);
  ma->emit(make_ABx(OpCode::CLOSURE, 1, 0), {});
  ma->emit(make_ABC(OpCode::MOVE, 1, 0, 0), {});
  ma->emit(make_ABC(OpCode::CLOSE_UPVALUE, 0, 0, 0), {});
  ma->emit(make_ABC(OpCode::RETURN, 1, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 3;
  top->functions.push(ma);
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(10)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(5)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();

  VMRunner r;
  EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 15);
}

TEST(VMClosures, SharedUpvalue_TwoClosuresOneSlot) {
  auto get_fn = makeChunk();
  get_fn->name = "get";
  get_fn->arity = 0;
  get_fn->upvalueCount = 1;
  get_fn->localCount = 1;
  get_fn->emit(make_ABC(OpCode::GET_UPVALUE, 0, 0, 0), {});
  get_fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});

  auto set_fn = makeChunk();
  set_fn->name = "set";
  set_fn->arity = 1;
  set_fn->upvalueCount = 1;
  set_fn->localCount = 1;
  set_fn->emit(make_ABC(OpCode::SET_UPVALUE, 0, 0, 0), {});
  set_fn->emit(make_ABC(OpCode::RETURN_NIL, 0, 0, 0), {});

  auto mp = makeChunk();
  mp->name = "mp";
  mp->arity = 0;
  mp->localCount = 4;
  mp->upvalueCount = 0;
  mp->functions.push(get_fn);
  mp->functions.push(set_fn);
  mp->emit(make_ABx(OpCode::LOAD_INT, 0, BX(0)), {});
  mp->emit(make_ABx(OpCode::CLOSURE, 1, 0), {});
  mp->emit(make_ABC(OpCode::MOVE, 1, 0, 0), {});
  mp->emit(make_ABx(OpCode::CLOSURE, 2, 1), {});
  mp->emit(make_ABC(OpCode::MOVE, 1, 0, 0), {});
  mp->emit(make_ABC(OpCode::LIST_NEW, 3, 2, 0), {});
  mp->emit(make_ABC(OpCode::LIST_APPEND, 3, 1, 0), {});
  mp->emit(make_ABC(OpCode::LIST_APPEND, 3, 2, 0), {});
  mp->emit(make_ABC(OpCode::CLOSE_UPVALUE, 0, 0, 0), {});
  mp->emit(make_ABC(OpCode::RETURN, 3, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 5;
  top->functions.push(mp);
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABC(OpCode::CALL, 0, 0, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(1)), {});
  top->emit(make_ABC(OpCode::LIST_GET, 1, 0, 2), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(99)), {});
  top->emit(make_ABC(OpCode::CALL, 1, 1, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(0)), {});
  top->emit(make_ABC(OpCode::LIST_GET, 1, 0, 2), {});
  top->emit(make_ABC(OpCode::CALL, 1, 0, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 1, 1, 0), {});
  top->disassemble();

  VMRunner r;
  EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 99);
}

TEST(VMICProfile, BinaryOpUpdatesSlot) {
  VMRunner r;
  CB b;
  b.regs(3).slot().load_int(0, 3).load_int(1, 4).ABC(OpCode::OP_ADD, 2, 0, 1).nop(0).ret(2);
  r.run(b);
  b.dump();
  auto const &s = r.chunk_->icSlots[0];
  EXPECT_TRUE(hasTag(TypeTag(s.seenLhs), TypeTag::INT));
  EXPECT_TRUE(hasTag(TypeTag(s.seenRhs), TypeTag::INT));
  EXPECT_TRUE(hasTag(TypeTag(s.seenRet), TypeTag::INT));
  EXPECT_GE(s.hitCount, 1u);
}

TEST(VMICProfile, SubUpdatesSlot) {
  VMRunner r;
  CB b;
  b.regs(3).slot().load_int(0, 10).load_int(1, 3).ABC(OpCode::OP_SUB, 2, 0, 1).nop(0).ret(2);
  b.dump();
  r.run(b);
  EXPECT_GE(r.chunk_->icSlots[0].hitCount, 1u);
}

TEST(VMICProfile, ICCallUpdatesSlot) {
  VMRunner r;
  CB b;
  b.regs(3)
      .slot()
      .ABC(OpCode::LIST_NEW, 0, 2, 0)
      .load_int(2, 1)
      .ABC(OpCode::LIST_APPEND, 0, 2, 0)
      .load_int(2, 2)
      .ABC(OpCode::LIST_APPEND, 0, 2, 0)
      .ldg(1, "len")
      .mov(2, 0)
      .ABC(OpCode::IC_CALL, 1, 1, 0)
      .ret(1);
  b.dump();
  r.run(b);
  auto const &s = r.chunk_->icSlots[0];
  EXPECT_TRUE(hasTag(TypeTag(s.seenLhs), TypeTag::NATIVE));
  EXPECT_TRUE(hasTag(TypeTag(s.seenRet), TypeTag::INT));
  EXPECT_GE(s.hitCount, 1u);
}

TEST(VMICProfile, SlotAccumulatesAcrossLoopIterations) {
  VMRunner r;
  CB b;
  b.regs(5)
      .slot()
      .load_int(0, 0)
      .load_int(1, 5)
      .load_int(2, 0)
      .ABC(OpCode::OP_LT, 3, 0, 1)
      .AsBx(OpCode::JUMP_IF_FALSE, 3, 4)
      .load_int(4, 1)
      .ABC(OpCode::OP_ADD, 0, 0, 4)
      .nop(0)
      .AsBx(OpCode::LOOP, 0, -6)
      .ret(0);
  b.dump();
  r.run(b);
  EXPECT_EQ(r.chunk_->icSlots[0].hitCount, 5u);
}

TEST(VMIntegration, Fibonacci_fib10_equals_55) {
  auto fib = makeChunk();
  fib->name = "fib";
  fib->arity = 1;
  fib->localCount = 9;
  uint16_t nk = fib->addConstant(MAKE_OBJECT(MAKE_OBJ_STRING("fib")));

  fib->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), {});
  fib->emit(make_ABC(OpCode::OP_LTE, 2, 0, 1), {});
  fib->emit(make_AsBx(OpCode::JUMP_IF_FALSE, 2, 1), {});
  fib->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  fib->emit(make_ABC(OpCode::OP_SUB, 3, 0, 1), {});
  fib->emit(make_ABx(OpCode::LOAD_GLOBAL, 4, nk), {});
  fib->emit(make_ABC(OpCode::MOVE, 5, 3, 0), {});
  fib->emit(make_ABC(OpCode::CALL, 4, 1, 0), {});
  fib->emit(make_ABx(OpCode::LOAD_INT, 1, BX(2)), {});
  fib->emit(make_ABC(OpCode::OP_SUB, 6, 0, 1), {});
  fib->emit(make_ABx(OpCode::LOAD_GLOBAL, 7, nk), {});
  fib->emit(make_ABC(OpCode::MOVE, 8, 6, 0), {});
  fib->emit(make_ABC(OpCode::CALL, 7, 1, 0), {});
  fib->emit(make_ABC(OpCode::OP_ADD, 4, 4, 7), {});
  fib->emit(make_ABC(OpCode::RETURN, 4, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 3;
  top->functions.push(fib);
  uint16_t tk = top->addConstant(MAKE_OBJECT(MAKE_OBJ_STRING("fib")));
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(10)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();
  VMRunner r;
  EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 55);
}

TEST(VMIntegration, SumForLoop_1_to_100) {
  GTEST_SKIP() << "Not implemented yet.";
  VMRunner r;
  CB b;
  b.regs(5)
      .slot()
      .load_int(0, 1)
      .load_int(1, 100)
      .load_int(2, 1)
      .load_int(4, 0)
      .AsBx(OpCode::FOR_PREP, 0, 3)
      .ABC(OpCode::OP_ADD, 4, 4, 3)
      .nop(0)
      .AsBx(OpCode::FOR_STEP, 0, -3)
      .ret(4);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 5050);
}

TEST(VMIntegration, StringConcat_3Parts) {
  GTEST_SKIP() << "Not implemented yet.";
  VMRunner r;
  CB b;
  b.regs(4);
  b.ABx(OpCode::LOAD_CONST, 0, b.str("hello"))
      .ABx(OpCode::LOAD_CONST, 1, b.str(", "))
      .ABx(OpCode::LOAD_CONST, 2, b.str("world"))
      .ABC(OpCode::CONCAT, 3, 0, 3)
      .ret(3);
  b.dump();
  Value v = r.run(b);
  EXPECT_EQ(AS_STRING(v)->str, "hello, world");
}

TEST(VMIntegration, ListSquaresViaForLoop) {
  GTEST_SKIP() << "Not implemented yet.";
  VMRunner r;
  CB b;
  b.regs(6)
      .slot()
      .slot()
      .load_int(0, 0)
      .load_int(1, 9)
      .load_int(2, 1)
      .ABC(OpCode::LIST_NEW, 4, 10, 0)
      .AsBx(OpCode::FOR_PREP, 0, 5)
      .ABC(OpCode::OP_MUL, 5, 3, 3)
      .nop(0)
      .ABC(OpCode::LIST_APPEND, 4, 5, 0)
      .ABC(OpCode::NOP, 1, 0, 0)
      .AsBx(OpCode::FOR_STEP, 0, -5)
      .load_int(5, 5)
      .ABC(OpCode::LIST_GET, 5, 4, 5)
      .ret(5);
  b.dump();
  EXPECT_EQ(AS_INTEGER(r.run(b)), 25);
}

TEST(VMIntegration, NestedAdderClosure) {
  auto add_fn = makeChunk();
  add_fn->name = "add";
  add_fn->arity = 1;
  add_fn->upvalueCount = 1;
  add_fn->localCount = 3;
  add_fn->emit(make_ABC(OpCode::GET_UPVALUE, 1, 0, 0), {});
  add_fn->emit(make_ABC(OpCode::OP_ADD, 2, 1, 0), {});
  add_fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), {});

  auto make_adder = makeChunk();
  make_adder->name = "ma";
  make_adder->arity = 1;
  make_adder->localCount = 2;
  make_adder->functions.push(add_fn);
  make_adder->emit(make_ABx(OpCode::CLOSURE, 1, 0), {});
  make_adder->emit(make_ABC(OpCode::MOVE, 1, 0, 0), {});
  make_adder->emit(make_ABC(OpCode::CLOSE_UPVALUE, 0, 0, 0), {});
  make_adder->emit(make_ABC(OpCode::RETURN, 1, 1, 0), {});

  auto top = makeChunk();
  top->name = "<test>";
  top->localCount = 3;
  top->functions.push(make_adder);
  top->emit(make_ABx(OpCode::CLOSURE, 0, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(5)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(3)), {});
  top->emit(make_ABC(OpCode::CALL, 0, 1, 0), {});
  top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), {});
  top->disassemble();

  VMRunner r;
  EXPECT_EQ(AS_INTEGER(r.run(std::move(top))), 8);
}
