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
        list.as_list()->elements.push(m_value);
    return list;
}

std::string as_std_string(Fa_Value m_value)
{
    EXPECT_TRUE(m_value.is_string());
    if (!m_value.is_string())
        return { };
    return std::string(m_value.as_string()->str.data());
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

    ASSERT_TRUE(result.is_list());
    ASSERT_EQ(result.as_list()->elements.size(), 5u);
    EXPECT_EQ(as_std_string(result.as_list()->elements[0]), "");
    EXPECT_EQ(as_std_string(result.as_list()->elements[1]), "alpha");
    EXPECT_EQ(as_std_string(result.as_list()->elements[2]), "");
    EXPECT_EQ(as_std_string(result.as_list()->elements[3]), "omega");
    EXPECT_EQ(as_std_string(result.as_list()->elements[4]), "");
}

TEST(StdlibRegression, JoinStringifiesMixedScalarValues)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_Value::from_int(7),
                                      Fa_Value::from_bool(true),
                                      str("ok"),
                                      Fa_Value::nil(),
                                  });
    Fa_Value m_args[] = { list, str("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "7|صحيح|ok|nil");
}

TEST(StdlibRegression, JoinEmptyListReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, str("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, AppendAddsMultipleValuesInOrder)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, Fa_Value::from_int(1), Fa_Value::from_int(2), Fa_Value::from_int(3) };
    Fa_Value result = vm.Fa_append(4, m_args);

    EXPECT_TRUE(result.is_nil());
    ASSERT_EQ(list.as_list()->elements.size(), 3u);
    EXPECT_EQ(list.as_list()->elements[0].as_int(), 1);
    EXPECT_EQ(list.as_list()->elements[1].as_int(), 2);
    EXPECT_EQ(list.as_list()->elements[2].as_int(), 3);
}

TEST(StdlibRegression, PopRemovesLastElementFromList)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_Value::from_int(10),
                                      Fa_Value::from_int(20),
                                      Fa_Value::from_int(30),
                                  });
    Fa_Value result = vm.Fa_pop(1, &list);

    EXPECT_TRUE(result.is_nil());
    ASSERT_EQ(list.as_list()->elements.size(), 2u);
    EXPECT_EQ(list.as_list()->elements[0].as_int(), 10);
    EXPECT_EQ(list.as_list()->elements[1].as_int(), 20);
}

TEST(StdlibRegression, SliceReturnsCopyNotAlias)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_Value::from_int(1),
                                        Fa_Value::from_int(2),
                                        Fa_Value::from_int(3),
                                        Fa_Value::from_int(4),
                                    });
    Fa_Value m_args[] = { source, Fa_Value::from_int(1), Fa_Value::from_int(2) };
    Fa_Value result = vm.Fa_slice(3, m_args);

    ASSERT_TRUE(result.is_list());
    ASSERT_EQ(result.as_list()->elements.size(), 2u);
    EXPECT_EQ(result.as_list()->elements[0].as_int(), 2);
    EXPECT_EQ(result.as_list()->elements[1].as_int(), 3);

    result.as_list()->elements[0] = Fa_Value::from_int(99);
    EXPECT_EQ(source.as_list()->elements[1].as_int(), 2);
}

TEST(StdlibRegression, SliceTwoArgsReturnsTail)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_Value::from_int(4),
                                        Fa_Value::from_int(5),
                                        Fa_Value::from_int(6),
                                        Fa_Value::from_int(7),
                                    });
    Fa_Value m_args[] = { source, Fa_Value::from_int(2) };
    Fa_Value result = vm.Fa_slice(2, m_args);

    ASSERT_TRUE(result.is_list());
    ASSERT_EQ(result.as_list()->elements.size(), 2u);
    EXPECT_EQ(result.as_list()->elements[0].as_int(), 6);
    EXPECT_EQ(result.as_list()->elements[1].as_int(), 7);
}

TEST(StdlibRegression, SubstrClampsEndPastStringLength)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), Fa_Value::from_int(2), Fa_Value::from_int(99) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "iruz");
}

TEST(StdlibRegression, SubstrZeroWidthRangeReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), Fa_Value::from_int(3), Fa_Value::from_int(3) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, ContainsEmptyNeedleIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), str("") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.as_bool());
}

TEST(StdlibRegression, ContainsExactMatchIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), str("fairuz") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.as_bool());
}

TEST(StdlibRegression, StrStringifiesListsLikePrint)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_Value::from_int(1),
                                      Fa_Value::from_bool(false),
                                      str("z"),
                                  });
    Fa_Value result = vm.Fa_str(1, &list);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), R"([1, خطا, "z"])");
}

TEST(StdlibRegression, StrStringifiesDictsLikePrint)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    dict.as_dict()->data[str("k")] = Fa_Value::from_int(3);
    dict.as_dict()->data[str("name")] = str("fairuz");

    Fa_Value result = vm.Fa_str(1, &dict);

    ASSERT_TRUE(result.is_string());
    EXPECT_TRUE(as_std_string(result).find("\"k\": 3") != std::string::npos);
    EXPECT_TRUE(as_std_string(result).find("\"name\": \"fairuz\"") != std::string::npos);
}

TEST(StdlibRegression, LenSupportsDicts)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    dict.as_dict()->data[str("a")] = Fa_Value::from_int(1);
    dict.as_dict()->data[str("b")] = Fa_Value::from_int(2);

    Fa_Value result = vm.Fa_len(1, &dict);

    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(StdlibRegression, DictConstructorPopulatesPairs)
{
    Fa_VM vm;
    Fa_Value m_args[] = {
        str("a"),
        Fa_Value::from_int(1),
        str("b"),
        Fa_Value::from_bool(true),
    };

    Fa_Value dict = vm.Fa_dict(4, m_args);

    ASSERT_TRUE(dict.is_dict());
    Fa_Value* a = dict.as_dict()->data.find_ptr(m_args[0]);
    Fa_Value* b = dict.as_dict()->data.find_ptr(m_args[2]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->as_int(), 1);
    EXPECT_TRUE(b->is_bool());
    EXPECT_TRUE(b->as_bool());
}

TEST(StdlibRegression, StrScalarConversionsMatchSurfaceSyntax)
{
    Fa_VM vm;

    Fa_Value int_value = Fa_Value::from_int(42);
    Fa_Value bool_value = Fa_Value::from_bool(true);
    Fa_Value nil_value = Fa_Value::nil();

    EXPECT_EQ(as_std_string(vm.Fa_str(1, &int_value)), "42");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &bool_value)), "صحيح");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &nil_value)), "nil");
}

TEST(StdlibRegression, TrimRemovesMixedLeadingAndTrailingWhitespace)
{
    Fa_VM vm;
    Fa_Value arg = str("\n\t  fairuz  \r\n");
    Fa_Value result = vm.Fa_trim(1, &arg);

    ASSERT_TRUE(result.is_string());
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

    ASSERT_TRUE(parts.is_list());
    ASSERT_EQ(parts.as_list()->elements.size(), 2000u);

    Fa_Value join_args[] = { parts, str(",") };
    start = std::chrono::high_resolution_clock::now();
    Fa_Value roundtrip = vm.Fa_join(2, join_args);
    double join_us = elapsed_us(start);

    ASSERT_TRUE(roundtrip.is_string());
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
        ASSERT_TRUE(value.is_int());
        last = value.as_int();
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
