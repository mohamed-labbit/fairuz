#pragma once

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace testing {

class AssertionFailure : public std::runtime_error
{
   public:
    using std::runtime_error::runtime_error;
};

struct AssertionHandle
{
    template<typename T>
    AssertionHandle& operator<<(const T&)
    {
        return *this;
    }
};

inline std::vector<std::pair<std::string, std::function<void()>>>& registry()
{
    static std::vector<std::pair<std::string, std::function<void()>>> tests;
    return tests;
}

inline void InitGoogleTest(int*, char**)
{
}

inline int RunAllTests()
{
    std::size_t failed = 0;
    for (const auto& [name, fn] : registry())
    {
        try
        {
            fn();
            std::cout << "[  PASSED  ] " << name << "\n";
        }
        catch (const std::exception& ex)
        {
            ++failed;
            std::cerr << "[  FAILED  ] " << name << ": " << ex.what() << "\n";
        }
        catch (...)
        {
            ++failed;
            std::cerr << "[  FAILED  ] " << name << ": unknown error\n";
        }
    }

    std::cout << "[  SUMMARY ] " << (registry().size() - failed) << " passed, " << failed << " failed\n";
    return static_cast<int>(failed);
}

class Test
{
   public:
    virtual ~Test() = default;
    virtual void SetUp() {}
    virtual void TearDown() {}
};

}  // namespace testing

#define TEST(SuiteName, TestName)                                                                     \
    class SuiteName##_##TestName##_Test : public testing::Test                                        \
    {                                                                                                 \
       public:                                                                                        \
        void TestBody();                                                                              \
    };                                                                                                \
    static bool SuiteName##_##TestName##_registered = []() {                                          \
        testing::registry().push_back({#SuiteName "." #TestName, []() {                              \
                                         SuiteName##_##TestName##_Test t;                             \
                                         t.SetUp();                                                   \
                                         t.TestBody();                                                \
                                         t.TearDown();                                                \
                                     }});                                                             \
        return true;                                                                                  \
    }();                                                                                              \
    void SuiteName##_##TestName##_Test::TestBody()

#define TEST_F(Fixture, TestName)                                                                     \
    class Fixture##_##TestName##_Test : public Fixture                                                \
    {                                                                                                 \
       public:                                                                                        \
        void TestBody();                                                                              \
    };                                                                                                \
    static bool Fixture##_##TestName##_registered = []() {                                            \
        testing::registry().push_back({#Fixture "." #TestName, []() {                                \
                                         Fixture##_##TestName##_Test t;                               \
                                         t.SetUp();                                                   \
                                         t.TestBody();                                                \
                                         t.TearDown();                                                \
                                     }});                                                             \
        return true;                                                                                  \
    }();                                                                                              \
    void Fixture##_##TestName##_Test::TestBody()

#define EXPECT_TRUE(condition)                                                                        \
    ([&]() {                                                                                          \
        if (!(condition))                                                                             \
            throw testing::AssertionFailure("EXPECT_TRUE failed: " #condition);                      \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_FALSE(condition) EXPECT_TRUE(!(condition))

#define ASSERT_TRUE(condition) EXPECT_TRUE(condition)
#define ASSERT_FALSE(condition) EXPECT_FALSE(condition)

#define EXPECT_EQ(val1, val2)                                                                          \
    ([&]() {                                                                                          \
        if (!((val1) == (val2)))                                                                       \
            throw testing::AssertionFailure("EXPECT_EQ failed: " #val1 " == " #val2);                \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_NE(val1, val2)                                                                          \
    ([&]() {                                                                                          \
        if (!((val1) != (val2)))                                                                       \
            throw testing::AssertionFailure("EXPECT_NE failed: " #val1 " != " #val2);               \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_GT(val1, val2)                                                                          \
    ([&]() {                                                                                          \
        if (!((val1) > (val2)))                                                                        \
            throw testing::AssertionFailure("EXPECT_GT failed: " #val1 " > " #val2);                \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_GE(val1, val2)                                                                          \
    ([&]() {                                                                                          \
        if (!((val1) >= (val2)))                                                                       \
            throw testing::AssertionFailure("EXPECT_GE failed: " #val1 " >= " #val2);               \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_LE(val1, val2)                                                                          \
    ([&]() {                                                                                          \
        if (!((val1) <= (val2)))                                                                       \
            throw testing::AssertionFailure("EXPECT_LE failed: " #val1 " <= " #val2);               \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_FLOAT_EQ(val1, val2)                                                                   \
    ([&]() {                                                                                          \
        if (std::fabs((val1) - (val2)) > 1e-5f)                                                       \
            throw testing::AssertionFailure("EXPECT_FLOAT_EQ failed: " #val1 " == " #val2);         \
        return testing::AssertionHandle{};                                                            \
    }())

#define EXPECT_DOUBLE_EQ(val1, val2)                                                                  \
    ([&]() {                                                                                          \
        if (std::fabs((val1) - (val2)) > 1e-9)                                                        \
            throw testing::AssertionFailure("EXPECT_DOUBLE_EQ failed: " #val1 " == " #val2);        \
        return testing::AssertionHandle{};                                                            \
    }())

#define ASSERT_EQ(val1, val2) EXPECT_EQ(val1, val2)
#define ASSERT_NE(val1, val2) EXPECT_NE(val1, val2)

#define EXPECT_THROW(statement, expected_exception)                                                   \
    ([&]() {                                                                                          \
        bool _caught = false;                                                                         \
        try                                                                                           \
        {                                                                                             \
            statement;                                                                                \
        }                                                                                             \
        catch (const expected_exception&)                                                             \
        {                                                                                             \
            _caught = true;                                                                           \
        }                                                                                             \
        catch (...)                                                                                   \
        {                                                                                             \
        }                                                                                             \
        if (!_caught)                                                                                 \
            throw testing::AssertionFailure("EXPECT_THROW failed: " #statement " did not throw " #expected_exception); \
        return testing::AssertionHandle{};                                                            \
    }())

#define ASSERT_THROW(statement, expected_exception) EXPECT_THROW(statement, expected_exception)

#define ADD_FAILURE() ([]() { return testing::AssertionHandle{}; }())
#define GTEST_SKIP() testing::AssertionHandle{}

#define RUN_ALL_TESTS() testing::RunAllTests()

