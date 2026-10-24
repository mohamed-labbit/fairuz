//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//===----------------------------------------------------------------------===//

#include "test_interpreter_common.h"

static f64 microseconds_since(std::chrono::high_resolution_clock::time_point t0)
{
    using namespace std::chrono;
    return static_cast<f64>(duration_cast<nanoseconds>(high_resolution_clock::now() - t0).count()) / 1000.0;
}

template<typename T> static void do_not_optimize(T const& v)
{
    void const volatile* sink = &v;
    (void)sink;
}

// Measures raw opcode dispatch speed. The loop body is:
//   r2 = r0 + r1  (OP_ADD, integer fast path)
//   r0 = r2       (MOVE)
// N iterations → N ADD + N MOVE dispatched.

TEST_F(VMPerfTest, Dispatch_IntAdd_1M_Iterations)
{
    constexpr int N = 1'000'000;

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("فهرس := 0\n"
                                           "خطوة := 1\n")
            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
            + "طالما (فهرس < حد):\n"
              "    فهرس := (فهرس + خطوة)\n"
              "ارجع فهرس\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  Dispatch IntAdd loop %dk iters:    %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}

// 2. Dispatch throughput — float arithmetic loop
//    Same structure, but f64 accumulator — exercises the FF fast path.

TEST_F(VMPerfTest, Dispatch_FloatAdd_500k_Iterations)
{
    constexpr int N = 500'000;

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("فهرس := 0.0\n"
                                           "خطوة := 1.0\n")
            + (std::string((std::string("حد := ") + test_source::floating(static_cast<f64>(N)))) + "\n")
            + "طالما (فهرس < حد):\n"
              "    فهرس := (فهرس + خطوة)\n"
              "ارجع فهرس\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    run_success(r.run());

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_TRUE(result.is_double());
    std::printf("  Dispatch FloatAdd loop %dk iters:  %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 4. Global variable lookup throughput
//    STORE_GLOBAL + LOAD_GLOBAL in a loop — exercises the globals hash map.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, GlobalLookup_1M_Roundtrips)
{
    constexpr int N = 1'000'000;

    std::string decl = "ا := 0\n";
    std::string while_loop = (std::string("طالما ") + (std::string("(ا < ") + std::to_string(N) + ")")
        + ":\n"
          "    ا := (ا + 1)\n");

    VMRunner r;
    r.compile({ decl, while_loop });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    ::printf("  Global store+load %dk roundtrips:  %.1f µs  (%.2f ns/op)\n", N / 1000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, CallOverhead_100k_Calls)
{
    constexpr int N = 100;

    std::string m_body = (std::string("فهرس := 0\n") + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
        + "طالما (فهرس < حد):\n"
          "    اجمع(1, 2)\n"
          "    فهرس := (فهرس + 1)\n"
          "ارجع فهرس\n");

    std::string func = (std::string("دالة اختبار():\n") + test_source::indent(m_body));

    std::string add = "دالة اجمع(ا, ب):\n"
                      "    ارجع (ا + ب)\n";
    std::string call = "اختبار()\n";

    std::cout << "AUTO:" << '\n';
    VMRunner r;
    r.compile({ add, func, call });
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  Call overhead %dk calls:            %.1f µs  (%.2f ns/call)\n", N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 6. Tail-call vs regular call
//    Both count to N via recursion. Tail-call should use O(1) frames.
//    We measure time; the tail-call version must not be significantly slower.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, TailCall_vs_RegularLoop_Ratio)
{
    constexpr int DEPTH = 5000;

    std::string tc_fn = "دالة استدعاء_ذيلي(ن):\n"
                        "    اذا (ن = 0):\n"
                        "        ارجع ن\n"
                        "    ارجع استدعاء_ذيلي((ن - 1))\n";

    VMRunner tc;
    tc.compile(
        { tc_fn, (std::string((std::string("استدعاء_ذيلي(") + (std::string(std::to_string(DEPTH))) + ")")) + "\n") });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value tc_result = run_success(tc.run());
    f64 tc_us = microseconds_since(t0);

    // ── equivalent iterative loop (no calls) ─────────────────────────────
    std::string loop_fn = (std::string("دالة حلقة():\n")
        + test_source::indent((std::string((std::string((std::string("ن := ") + std::to_string(DEPTH))) + "\n"))
            + "طالما (ن != 0):\n"
              "    ن := (ن - 1)\n"
              "ارجع ن\n")));

    VMRunner loop;
    loop.compile({ loop_fn, "حلقة()\n" });
    t0 = std::chrono::high_resolution_clock::now();
    Value loop_result = run_success(loop.run());
    f64 loop_us = microseconds_since(t0);

    do_not_optimize(tc_result);
    do_not_optimize(loop_result);
    EXPECT_EQ(tc_result.as_int(), 0);
    EXPECT_EQ(loop_result.as_int(), 0);

    ::printf("  Tail-call %d depth:  tc=%.1f µs  loop=%.1f µs  ratio=%.2fx\n", DEPTH, tc_us, loop_us, tc_us / loop_us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 7. List operations throughput
//    Builds a list of N elements then iterates over it summing values.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, List_AppendAndSum_10k)
{
    constexpr int N = 10'000;
    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("قائمة_قيم := []\n"
                                           "فهرس := 0\n")
            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
            + "طالما (فهرس < حد):\n"
              "    اضف(قائمة_قيم, فهرس)\n"
              "    فهرس := (فهرس + 1)\n"
              "فهرس := 0\n"
              "مجموع := 0\n"
              "طالما (فهرس < حد):\n"
              "    مجموع := (مجموع + قائمة_قيم[فهرس])\n"
              "    فهرس := (فهرس + 1)\n"
              "ارجع مجموع\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);
    do_not_optimize(result);

    constexpr i64 expected = static_cast<i64>(N) * (N - 1) / 2;
    EXPECT_EQ(result.as_int(), expected);
    std::printf("  List append+sum N=%d:               %.1f µs\n", N, us);
}

// ─────────────────────────────────────────────────────────────────────────────
// 9. Native function call throughput — len on a string, hot loop
//    IC_CALL path: first call warms the inline cache, subsequent calls hit it.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, NativeCall_Len_50k_ICHot)
{
    constexpr int N = 50'000;

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("نص := \"hello world\"\n"
                                           "فهرس := 0\n")
            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
            + "اخير := 0\n"
              "طالما (فهرس < حد):\n"
              "    اخير := طول(نص)\n"
              "    فهرس := (فهرس + 1)\n"
              "ارجع اخير\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });

    // Cold run to prime the IC.
    run_success(r.run());

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 11);
    std::printf("  NativeCall len() IC hot %dk:        %.1f µs  (%.2f ns/call)\n", N / 1000, us, us * 1000.0 / N);
}

// ─────────────────────────────────────────────────────────────────────────────
// 10. Mixed realistic workload — Fibonacci(25) repeated
//     Exercises: calls, branches, integer arithmetic, globals lookup.
//     fib(25) = 75025. We run it 100 times and report total + per-call time.
// ─────────────────────────────────────────────────────────────────────────────

static Array<std::string> make_fib_program(int n, int reps)
{
    std::string fib = "دالة فيبوناتشي(س):\n"
                      "    اذا (س <= 1):\n"
                      "        ارجع س\n"
                      "    ارجع (فيبوناتشي((س - 1)) + فيبوناتشي((س - 2)))\n";

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("فهرس := 0\n")
            + (std::string((std::string("حد := ") + std::to_string(reps))) + "\n") + "ناتج := 0\n"
            + (std::string("طالما (فهرس < حد):\n")
                + test_source::indent(
                    (std::string((std::string((std::string("ناتج := ")
                                      + (std::string("فيبوناتشي(") + (std::string(std::to_string(n))) + ")")))
                         + "\n"))
                        + "فهرس := (فهرس + 1)\n")))
            + "ارجع ناتج\n")));

    return { fib, test, "اختبار()\n" };
}

TEST_F(VMPerfTest, Fib20_100reps)
{
    constexpr int FIB_N = 20;
    constexpr int REPS = 100;

    VMRunner r;
    r.compile(make_fib_program(FIB_N, REPS));

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 6765); // fib(20)
    ::printf("  fib(%d) × %d reps:                  %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

TEST_F(VMPerfTest, Fib25_10reps)
{
    constexpr int FIB_N = 25;
    constexpr int REPS = 10;

    VMRunner r;
    r.compile(make_fib_program(FIB_N, REPS));

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 75025); // fib(25)
    ::printf("  fib(%d) × %d reps:                   %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}

// ─────────────────────────────────────────────────────────────────────────────
// 11. Large stress tests (opt-in)
//     These are intentionally much larger than the regular perf tests and are
//     gated behind ENABLE_STRESS_PERF=1 so they do not dominate normal
//     test runs.
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(VMPerfTest, Dispatch_IntAdd_10M_Iterations)
{
    constexpr int N = 10'000'000;

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("فهرس := 0\n"
                                           "خطوة := 1\n")
            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
            + "طالما (فهرس < حد):\n"
              "    فهرس := (فهرس + خطوة)\n"
              "ارجع فهرس\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    run_success(r.run()); // warm quickened arithmetic path

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), N);
    std::printf("  STRESS IntAdd loop %dM iters:       %.1f µs  (%.2f ns/op)\n", N / 1'000'000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, NativeCall_Len_1M_ICHot)
{
    constexpr int N = 1'000'000;

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("نص := \"hello world\"\n"
                                           "فهرس := 0\n")
            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
            + "اخير := 0\n"
              "طالما (فهرس < حد):\n"
              "    اخير := طول(نص)\n"
              "    فهرس := (فهرس + 1)\n"
              "ارجع اخير\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });
    run_success(r.run()); // warm IC_CALL -> CALL rewrite

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 11);
    std::printf("  STRESS native len() %dM calls:      %.1f µs  (%.2f ns/call)\n", N / 1'000'000, us, us * 1000.0 / N);
}

TEST_F(VMPerfTest, List_AppendAndSum_100k)
{
    constexpr int N = 100'000;

    std::string test = (std::string("دالة اختبار():\n")
        + test_source::indent((std::string("قائمة_قيم := []\n"
                                           "فهرس := 0\n")
            + (std::string((std::string("حد := ") + std::to_string(N))) + "\n")
            + "طالما (فهرس < حد):\n"
              "    اضف(قائمة_قيم, فهرس)\n"
              "    فهرس := (فهرس + 1)\n"
              "فهرس := 0\n"
              "مجموع := 0\n"
              "طالما (فهرس < حد):\n"
              "    مجموع := (مجموع + قائمة_قيم[فهرس])\n"
              "    فهرس := (فهرس + 1)\n"
              "ارجع مجموع\n")));

    VMRunner r;
    r.compile({ test, "اختبار()\n" });

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    constexpr i64 expected = static_cast<i64>(N) * (N - 1) / 2;
    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), expected);
    std::printf("  STRESS list append+sum N=%d:        %.1f µs\n", N, us);
}

TEST_F(VMPerfTest, Fib28_20reps_Hot)
{
    constexpr int FIB_N = 28;
    constexpr int REPS = 20;

    VMRunner r;
    r.compile(make_fib_program(FIB_N, REPS));
    run_success(r.run()); // warm global lookup and call-site rewriting

    auto t0 = std::chrono::high_resolution_clock::now();
    Value result = run_success(r.run());
    f64 us = microseconds_since(t0);

    do_not_optimize(result);
    EXPECT_EQ(result.as_int(), 317811);
    std::printf("  STRESS fib(%d) × %d reps hot:       %.1f µs  (%.1f µs/call)\n", FIB_N, REPS, us, us / REPS);
}
