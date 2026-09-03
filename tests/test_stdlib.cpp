#include "../fairuz/fvm.hpp"
#include "test_common.h"

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
        Fa_as_list(list)->elements.push(m_value);
    return list;
}

std::string as_std_string(Fa_Value m_value)
{
    EXPECT_TRUE(Fa_is_string(m_value));
    if (!Fa_is_string(m_value))
        return { };
    return std::string(Fa_as_string(m_value)->str.data());
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
    Fa_Value m_args[] = { str(",alpha,,omega,"), str(",") };
    Fa_Value result = vm.Fa_split(2, m_args);

    ASSERT_TRUE(Fa_is_list(result));
    ASSERT_EQ(Fa_as_list(result)->elements.size(), 5u);
    EXPECT_EQ(as_std_string(Fa_as_list(result)->elements[0]), "");
    EXPECT_EQ(as_std_string(Fa_as_list(result)->elements[1]), "alpha");
    EXPECT_EQ(as_std_string(Fa_as_list(result)->elements[2]), "");
    EXPECT_EQ(as_std_string(Fa_as_list(result)->elements[3]), "omega");
    EXPECT_EQ(as_std_string(Fa_as_list(result)->elements[4]), "");
}

TEST(StdlibRegression, JoinStringifiesMixedScalarValues)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_make_int(7),
                                      Fa_make_bool(true),
                                      str("ok"),
                                      Fa_make_nil(),
                                  });
    Fa_Value m_args[] = { list, str("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), "7|صحيح|ok|nil");
}

TEST(StdlibRegression, JoinEmptyListReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, str("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, AppendAddsMultipleValuesInOrder)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, Fa_make_int(1), Fa_make_int(2), Fa_make_int(3) };
    Fa_Value result = vm.Fa_append(4, m_args);

    EXPECT_TRUE(Fa_is_nil(result));
    ASSERT_EQ(Fa_as_list(list)->elements.size(), 3u);
    EXPECT_EQ(Fa_as_int(Fa_as_list(list)->elements[0]), 1);
    EXPECT_EQ(Fa_as_int(Fa_as_list(list)->elements[1]), 2);
    EXPECT_EQ(Fa_as_int(Fa_as_list(list)->elements[2]), 3);
}

TEST(StdlibRegression, PopRemovesLastElementFromList)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_make_int(10),
                                      Fa_make_int(20),
                                      Fa_make_int(30),
                                  });
    Fa_Value result = vm.Fa_pop(1, &list);

    EXPECT_TRUE(Fa_is_nil(result));
    ASSERT_EQ(Fa_as_list(list)->elements.size(), 2u);
    EXPECT_EQ(Fa_as_int(Fa_as_list(list)->elements[0]), 10);
    EXPECT_EQ(Fa_as_int(Fa_as_list(list)->elements[1]), 20);
}

TEST(StdlibRegression, SliceReturnsCopyNotAlias)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_make_int(1),
                                        Fa_make_int(2),
                                        Fa_make_int(3),
                                        Fa_make_int(4),
                                    });
    Fa_Value m_args[] = { source, Fa_make_int(1), Fa_make_int(2) };
    Fa_Value result = vm.Fa_slice(3, m_args);

    ASSERT_TRUE(Fa_is_list(result));
    ASSERT_EQ(Fa_as_list(result)->elements.size(), 2u);
    EXPECT_EQ(Fa_as_int(Fa_as_list(result)->elements[0]), 2);
    EXPECT_EQ(Fa_as_int(Fa_as_list(result)->elements[1]), 3);

    Fa_as_list(result)->elements[0] = Fa_make_int(99);
    EXPECT_EQ(Fa_as_int(Fa_as_list(source)->elements[1]), 2);
}

TEST(StdlibRegression, SliceTwoArgsReturnsTail)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_make_int(4),
                                        Fa_make_int(5),
                                        Fa_make_int(6),
                                        Fa_make_int(7),
                                    });
    Fa_Value m_args[] = { source, Fa_make_int(2) };
    Fa_Value result = vm.Fa_slice(2, m_args);

    ASSERT_TRUE(Fa_is_list(result));
    ASSERT_EQ(Fa_as_list(result)->elements.size(), 2u);
    EXPECT_EQ(Fa_as_int(Fa_as_list(result)->elements[0]), 6);
    EXPECT_EQ(Fa_as_int(Fa_as_list(result)->elements[1]), 7);
}

TEST(StdlibRegression, SubstrClampsEndPastStringLength)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), Fa_make_int(2), Fa_make_int(99) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), "iruz");
}

TEST(StdlibRegression, SubstrZeroWidthRangeReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), Fa_make_int(3), Fa_make_int(3) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, ContainsEmptyNeedleIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), str("") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(Fa_is_bool(result));
    EXPECT_TRUE(Fa_as_bool(result));
}

TEST(StdlibRegression, ContainsExactMatchIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), str("fairuz") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(Fa_is_bool(result));
    EXPECT_TRUE(Fa_as_bool(result));
}

TEST(StdlibRegression, StrStringifiesListsLikePrint)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_make_int(1),
                                      Fa_make_bool(false),
                                      str("z"),
                                  });
    Fa_Value result = vm.Fa_str(1, &list);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), R"([1, خطا, "z"])");
}

TEST(StdlibRegression, StrStringifiesDictsLikePrint)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    Fa_as_dict(dict)->data[str("k")] = Fa_make_int(3);
    Fa_as_dict(dict)->data[str("name")] = str("fairuz");

    Fa_Value result = vm.Fa_str(1, &dict);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), R"({"k": 3, "name": "fairuz"})");
}

TEST(StdlibRegression, LenSupportsDicts)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    Fa_as_dict(dict)->data[str("a")] = Fa_make_int(1);
    Fa_as_dict(dict)->data[str("b")] = Fa_make_int(2);

    Fa_Value result = vm.Fa_len(1, &dict);

    ASSERT_TRUE(Fa_is_int(result));
    EXPECT_EQ(Fa_as_int(result), 2);
}

TEST(StdlibRegression, DictConstructorPopulatesPairs)
{
    Fa_VM vm;
    Fa_Value m_args[] = {
        str("a"),
        Fa_make_int(1),
        str("b"),
        Fa_make_bool(true),
    };

    Fa_Value dict = vm.Fa_dict(4, m_args);

    ASSERT_TRUE(Fa_is_dict(dict));
    Fa_Value* a = Fa_as_dict(dict)->data.find_ptr(m_args[0]);
    Fa_Value* b = Fa_as_dict(dict)->data.find_ptr(m_args[2]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(Fa_as_int(*a), 1);
    EXPECT_TRUE(Fa_is_bool(*b));
    EXPECT_TRUE(Fa_as_bool(*b));
}

TEST(StdlibRegression, StrScalarConversionsMatchSurfaceSyntax)
{
    Fa_VM vm;

    Fa_Value int_value = Fa_make_int(42);
    Fa_Value bool_value = Fa_make_bool(true);
    Fa_Value nil_value = Fa_make_nil();

    EXPECT_EQ(as_std_string(vm.Fa_str(1, &int_value)), "42");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &bool_value)), "صحيح");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &nil_value)), "nil");
}

TEST(StdlibRegression, TrimRemovesMixedLeadingAndTrailingWhitespace)
{
    Fa_VM vm;
    Fa_Value arg = str("\n\t  fairuz  \r\n");
    Fa_Value result = vm.Fa_trim(1, &arg);

    ASSERT_TRUE(Fa_is_string(result));
    EXPECT_EQ(as_std_string(result), "fairuz");
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

    Fa_Value split_args[] = { str(csv.c_str()), str(",") };
    auto start = std::chrono::high_resolution_clock::now();
    Fa_Value parts = vm.Fa_split(2, split_args);
    double split_us = elapsed_us(start);

    ASSERT_TRUE(Fa_is_list(parts));
    ASSERT_EQ(Fa_as_list(parts)->elements.size(), 2000u);

    Fa_Value join_args[] = { parts, str(",") };
    start = std::chrono::high_resolution_clock::now();
    Fa_Value roundtrip = vm.Fa_join(2, join_args);
    double join_us = elapsed_us(start);

    ASSERT_TRUE(Fa_is_string(roundtrip));
    EXPECT_EQ(as_std_string(roundtrip), csv);
    std::printf("  stdlib split 2k fields: %.1f us, join: %.1f us\n", split_us, join_us);
}

TEST(StdlibPerf, LenOnLargeString100kCalls)
{
    Fa_VM vm;
    std::string payload(8192, 'x');
    Fa_Value arg = str(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    i64 last = -1;
    for (int i = 0; i < 100000; i += 1) {
        Fa_Value value = vm.Fa_len(1, &arg);
        ASSERT_TRUE(Fa_is_int(value));
        last = Fa_as_int(value);
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
    Fa_Value arg = str(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    std::string last;
    for (int i = 0; i < 50000; i += 1)
        last = as_std_string(vm.Fa_trim(1, &arg));
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, "fairuz");
    std::printf("  stdlib trim 50k calls (2 KiB padding): %.1f us\n", total_us);
}
