//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

static Chunk* make_adder_chunk()
{
    auto fn = make_chunk();
    fn->name = "اجمع_اثنين";
    fn->arity = 2;
    fn->local_count = 3;
    // r0=a r1=b (params); r2=a+b
    fn->emit(make_ABC(OpCode::OP_ADD, 2, 0, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 2, 1, 0), { });
    return fn;
}

TEST(VMCalls, CallClosure_TwoArgs)
{
    auto top = make_chunk();
    top->name = "<اختبار>";
    top->local_count = 4;
    top->functions.push(make_adder_chunk());

    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(3)), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 2, BX(4)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 2, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    if (test_config::dump_bytecode) top->disassemble();
    VMRunner r;
    EXPECT_EQ(run_success(r.run(top)).as_int(), 7);
}

TEST(VMCalls, WrongArgcThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("دالة اجمع(س، ص):\n"
                                                   "    ارجع س + ص\n"
                                                   "اجمع(1)\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMCalls, CallNonFunctionThrows)
{
    VMRunner runner;
    auto error = run_failed(runner.compile_and_run("قيمة := 5\n"
                                                   "قيمة()\n"));
    ASSERT_NE(error.source, nullptr);
    EXPECT_GT(error.loc.line, 0u);
    EXPECT_LE(error.loc.line, error.source->lines.size());
}

TEST(VMCalls, ICCallBuiltinLen)
{
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
        .ldg(1, "طول")
        .mov(2, 0)
        .ABC(OpCode::IC_CALL, 1, 1, 0)
        .ret(1);
    if (test_config::dump_bytecode) b.dump();
    EXPECT_EQ(run_success(r.run(b)).as_int(), 3);
}

TEST(VMCalls, TailCall_DoesNotOverflowFrames)
{
    auto fn = make_chunk();
    fn->name = "تنازل";
    fn->arity = 1;
    fn->local_count = 4;

    u16 nk = fn->add_constant(str("cd")).value;

    fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(0)), { });
    fn->emit(make_ABC(OpCode::OP_EQ, 1, 0, 1), { });
    fn->emit(make_AsBx(OpCode::JUMP_IF_FALSE, 1, 1), { });
    fn->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    fn->emit(make_ABx(OpCode::LOAD_INT, 1, BX(1)), { });
    fn->emit(make_ABC(OpCode::OP_SUB, 0, 0, 1), { });
    fn->emit(make_ABx(OpCode::LOAD_GLOBAL, 2, nk), { });
    fn->emit(make_ABC(OpCode::MOVE, 3, 0, 0), { });
    fn->emit(make_ABC(OpCode::CALL_TAIL, 2, 1, 0), { });

    auto top = make_chunk();
    top->name = "<اختبار>";
    top->local_count = 3;
    top->functions.push(fn);

    u16 tk = top->add_constant(str("cd")).value;
    top->emit(make_ABx(OpCode::CLOSURE, 0, 0), { });
    top->emit(make_ABx(OpCode::STORE_GLOBAL, 0, tk), { });
    top->emit(make_ABx(OpCode::LOAD_INT, 1, BX(300)), { });
    top->emit(make_ABC(OpCode::CALL, 0, 1, 0), { });
    top->emit(make_ABC(OpCode::RETURN, 0, 1, 0), { });
    if (test_config::dump_bytecode) top->disassemble();
    VMRunner r;
    Value v = run_success(r.run(std::move(top)));
    EXPECT_EQ(v.as_int(), 0);
}

TEST(VMCalls, StackOverflowDetected)
{
    VMRunner r;
    auto ast = Array<std::string> {
        "دالة تكرار_لا_نهائي():\n"
        "    تكرار_لا_نهائي()\n"
        "    ارجع عدم\n",
        "تكرار_لا_نهائي()\n",
    };
    run_failed(r.compile_and_run(ast));
}
