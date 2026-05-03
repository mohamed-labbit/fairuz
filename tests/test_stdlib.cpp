#include "../fairuz/diagnostic.hpp"
#include "../fairuz/vm.hpp"

#include <chrono>
#include <cstdio>
#include <gtest/gtest.h>
#include <string>

using namespace fairuz;
using namespace fairuz::runtime;

namespace {

Fa_Value make_list(Fa_VM& vm, std::initializer_list<Fa_Value> values)
{
    Fa_Value list = vm.Fa_list(0, nullptr);
    for (Fa_Value m_value : values)
        Fa_AS_LIST(list)->elements.push(m_value);
    return list;
}

std::string as_std_string(Fa_Value m_value)
{
    EXPECT_TRUE(Fa_IS_STRING(m_value));
    if (!Fa_IS_STRING(m_value))
        return { };
    return std::string(Fa_AS_STRING(m_value)->str.data());
}

double elapsed_us(std::chrono::high_resolution_clock::time_point start)
{
    using namespace std::chrono;
    return static_cast<double>(
               duration_cast<nanoseconds>(high_resolution_clock::now() - start).count())
        / 1000.0;
}

} // namespace

TEST(StdlibRegression, SplitPreservesEmptyFieldsAtBothEnds)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING(",alpha,,omega,"), Fa_MAKE_STRING(",") };
    Fa_Value result = vm.Fa_split(2, m_args);

    ASSERT_TRUE(Fa_IS_LIST(result));
    ASSERT_EQ(Fa_AS_LIST(result)->elements.size(), 5u);
    EXPECT_EQ(as_std_string(Fa_AS_LIST(result)->elements[0]), "");
    EXPECT_EQ(as_std_string(Fa_AS_LIST(result)->elements[1]), "alpha");
    EXPECT_EQ(as_std_string(Fa_AS_LIST(result)->elements[2]), "");
    EXPECT_EQ(as_std_string(Fa_AS_LIST(result)->elements[3]), "omega");
    EXPECT_EQ(as_std_string(Fa_AS_LIST(result)->elements[4]), "");
}

TEST(StdlibRegression, JoinStringifiesMixedScalarValues)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_MAKE_INTEGER(7),
                                      Fa_MAKE_BOOL(true),
                                      Fa_MAKE_STRING("ok"),
                                      NIL_VAL,
                                  });
    Fa_Value m_args[] = { list, Fa_MAKE_STRING("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), "7|true|ok|nil");
}

TEST(StdlibRegression, JoinEmptyListReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, Fa_MAKE_STRING("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, AppendAddsMultipleValuesInOrder)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, Fa_MAKE_INTEGER(1), Fa_MAKE_INTEGER(2), Fa_MAKE_INTEGER(3) };
    Fa_Value result = vm.Fa_append(4, m_args);

    EXPECT_TRUE(Fa_IS_NIL(result));
    ASSERT_EQ(Fa_AS_LIST(list)->elements.size(), 3u);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(list)->elements[0]), 1);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(list)->elements[1]), 2);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(list)->elements[2]), 3);
}

TEST(StdlibRegression, AppendTypeErrorPopulatesDiagnostics)
{
    diagnostic::reset();

    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_INTEGER(7), Fa_MAKE_INTEGER(9) };
    Fa_Value result = vm.Fa_append(2, m_args);

    EXPECT_TRUE(Fa_IS_NIL(result));
    EXPECT_TRUE(diagnostic::has_errors());
    EXPECT_GE(diagnostic::error_count(), 2u);
    EXPECT_NE(diagnostic::engine.to_json().find("append() expects a list as the first argument"), std::string::npos);
}

TEST(StdlibRegression, PopRemovesLastElementFromList)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_MAKE_INTEGER(10),
                                      Fa_MAKE_INTEGER(20),
                                      Fa_MAKE_INTEGER(30),
                                  });
    Fa_Value result = vm.Fa_pop(1, &list);

    EXPECT_TRUE(Fa_IS_NIL(result));
    ASSERT_EQ(Fa_AS_LIST(list)->elements.size(), 2u);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(list)->elements[0]), 10);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(list)->elements[1]), 20);
}

TEST(StdlibRegression, SliceReturnsCopyNotAlias)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_MAKE_INTEGER(1),
                                        Fa_MAKE_INTEGER(2),
                                        Fa_MAKE_INTEGER(3),
                                        Fa_MAKE_INTEGER(4),
                                    });
    Fa_Value m_args[] = { source, Fa_MAKE_INTEGER(1), Fa_MAKE_INTEGER(2) };
    Fa_Value result = vm.Fa_slice(3, m_args);

    ASSERT_TRUE(Fa_IS_LIST(result));
    ASSERT_EQ(Fa_AS_LIST(result)->elements.size(), 2u);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(result)->elements[0]), 2);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(result)->elements[1]), 3);

    Fa_AS_LIST(result)->elements[0] = Fa_MAKE_INTEGER(99);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(source)->elements[1]), 2);
}

TEST(StdlibRegression, SliceTwoArgsReturnsTail)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_MAKE_INTEGER(4),
                                        Fa_MAKE_INTEGER(5),
                                        Fa_MAKE_INTEGER(6),
                                        Fa_MAKE_INTEGER(7),
                                    });
    Fa_Value m_args[] = { source, Fa_MAKE_INTEGER(2) };
    Fa_Value result = vm.Fa_slice(2, m_args);

    ASSERT_TRUE(Fa_IS_LIST(result));
    ASSERT_EQ(Fa_AS_LIST(result)->elements.size(), 2u);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(result)->elements[0]), 6);
    EXPECT_EQ(Fa_AS_INTEGER(Fa_AS_LIST(result)->elements[1]), 7);
}

TEST(StdlibRegression, SubstrClampsEndPastStringLength)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("fairuz"), Fa_MAKE_INTEGER(2), Fa_MAKE_INTEGER(99) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), "iruz");
}

TEST(StdlibRegression, SubstrZeroWidthRangeReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("fairuz"), Fa_MAKE_INTEGER(3), Fa_MAKE_INTEGER(3) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, ContainsEmptyNeedleIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("fairuz"), Fa_MAKE_STRING("") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(Fa_IS_BOOL(result));
    EXPECT_TRUE(Fa_AS_BOOL(result));
}

TEST(StdlibRegression, ContainsExactMatchIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_STRING("fairuz"), Fa_MAKE_STRING("fairuz") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(Fa_IS_BOOL(result));
    EXPECT_TRUE(Fa_AS_BOOL(result));
}

TEST(StdlibRegression, StrStringifiesListsLikePrint)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_MAKE_INTEGER(1),
                                      Fa_MAKE_BOOL(false),
                                      Fa_MAKE_STRING("z"),
                                  });
    Fa_Value result = vm.Fa_str(1, &list);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), R"([1, false, "z"])");
}

TEST(StdlibRegression, StrStringifiesDictsLikePrint)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    Fa_AS_DICT(dict)->data[Fa_MAKE_STRING("k")] = Fa_MAKE_INTEGER(3);
    Fa_AS_DICT(dict)->data[Fa_MAKE_STRING("name")] = Fa_MAKE_STRING("fairuz");

    Fa_Value result = vm.Fa_str(1, &dict);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), R"({"k": 3, "name": "fairuz"})");
}

TEST(StdlibRegression, LenSupportsDicts)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    Fa_AS_DICT(dict)->data[Fa_MAKE_STRING("a")] = Fa_MAKE_INTEGER(1);
    Fa_AS_DICT(dict)->data[Fa_MAKE_STRING("b")] = Fa_MAKE_INTEGER(2);

    Fa_Value result = vm.Fa_len(1, &dict);

    ASSERT_TRUE(Fa_IS_INTEGER(result));
    EXPECT_EQ(Fa_AS_INTEGER(result), 2);
}

TEST(StdlibRegression, DictConstructorPopulatesPairs)
{
    Fa_VM vm;
    Fa_Value m_args[] = {
        Fa_MAKE_STRING("a"),
        Fa_MAKE_INTEGER(1),
        Fa_MAKE_STRING("b"),
        Fa_MAKE_BOOL(true),
    };

    Fa_Value dict = vm.Fa_dict(4, m_args);

    ASSERT_TRUE(Fa_IS_DICT(dict));
    Fa_Value* a = Fa_AS_DICT(dict)->data.find_ptr(m_args[0]);
    Fa_Value* b = Fa_AS_DICT(dict)->data.find_ptr(m_args[2]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(Fa_AS_INTEGER(*a), 1);
    EXPECT_TRUE(Fa_IS_BOOL(*b));
    EXPECT_TRUE(Fa_AS_BOOL(*b));
}

TEST(StdlibRegression, StrScalarConversionsMatchSurfaceSyntax)
{
    Fa_VM vm;

    Fa_Value int_value = Fa_MAKE_INTEGER(42);
    Fa_Value bool_value = Fa_MAKE_BOOL(true);
    Fa_Value nil_value = NIL_VAL;

    EXPECT_EQ(as_std_string(vm.Fa_str(1, &int_value)), "42");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &bool_value)), "true");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &nil_value)), "nil");
}

TEST(StdlibRegression, TrimRemovesMixedLeadingAndTrailingWhitespace)
{
    Fa_VM vm;
    Fa_Value arg = Fa_MAKE_STRING("\n\t  fairuz  \r\n");
    Fa_Value result = vm.Fa_trim(1, &arg);

    ASSERT_TRUE(Fa_IS_STRING(result));
    EXPECT_EQ(as_std_string(result), "fairuz");
}

TEST(StdlibRegression, AssertStopsBeingSilentWhenAnyArgumentIsFalsy)
{
    diagnostic::reset();

    Fa_VM vm;
    Fa_Value m_args[] = { Fa_MAKE_BOOL(true), Fa_MAKE_BOOL(false), Fa_MAKE_INTEGER(1) };
    Fa_Value result = vm.Fa_assert(3, m_args);

    EXPECT_TRUE(Fa_IS_NIL(result));
    EXPECT_TRUE(diagnostic::has_errors());
}

TEST(StdlibPerf, SplitJoinRoundTripLargeCsv)
{
    Fa_VM vm;
    std::string csv;
    csv.reserve(32 * 2000);
    for (int i = 0; i < 2000; i += 1) {
        if (i)
            csv += ',';
        csv += "field";
        csv += std::to_string(i);
    }

    Fa_Value split_args[] = { Fa_MAKE_STRING(csv.c_str()), Fa_MAKE_STRING(",") };
    auto start = std::chrono::high_resolution_clock::now();
    Fa_Value parts = vm.Fa_split(2, split_args);
    double split_us = elapsed_us(start);

    ASSERT_TRUE(Fa_IS_LIST(parts));
    ASSERT_EQ(Fa_AS_LIST(parts)->elements.size(), 2000u);

    Fa_Value join_args[] = { parts, Fa_MAKE_STRING(",") };
    start = std::chrono::high_resolution_clock::now();
    Fa_Value roundtrip = vm.Fa_join(2, join_args);
    double join_us = elapsed_us(start);

    ASSERT_TRUE(Fa_IS_STRING(roundtrip));
    EXPECT_EQ(as_std_string(roundtrip), csv);
    std::printf("  stdlib split 2k fields: %.1f us, join: %.1f us\n", split_us, join_us);
}

TEST(StdlibPerf, LenOnLargeString100kCalls)
{
    Fa_VM vm;
    std::string payload(8192, 'x');
    Fa_Value arg = Fa_MAKE_STRING(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    i64 last = -1;
    for (int i = 0; i < 100000; i += 1) {
        Fa_Value m_value = vm.Fa_len(1, &arg);
        ASSERT_TRUE(Fa_IS_INTEGER(m_value));
        last = Fa_AS_INTEGER(m_value);
    }
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, 8192);
    std::printf("  stdlib len 100k calls (8 KiB string): %.1f us\n", total_us);
}

TEST(StdlibPerf, TrimLargePaddedString50kCalls)
{
    Fa_VM vm;
    std::string payload(1024, ' ');
    payload += "fairuz";
    payload.append(1024, '\t');
    Fa_Value arg = Fa_MAKE_STRING(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    std::string last;
    for (int i = 0; i < 50000; i += 1)
        last = as_std_string(vm.Fa_trim(1, &arg));
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, "fairuz");
    std::printf("  stdlib trim 50k calls (2 KiB padding): %.1f us\n", total_us);
}
