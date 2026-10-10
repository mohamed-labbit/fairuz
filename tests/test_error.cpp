#include "../fairuz/ferror.hpp"

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

using fairuz::ErrorOr;

namespace {

struct Payload {
    inline static int alive = 0;
    inline static bool fail_copy = false;
    inline static bool fail_move = false;
    inline static bool fail_assign = false;
    int id;
    explicit Payload(int value)
        : id(value)
    {
        ++alive;
    }
    Payload(Payload const& other)
        : id(other.id)
    {
        if (fail_copy)
            throw std::runtime_error("copy");
        ++alive;
    }
    Payload(Payload&& other) noexcept(false)
        : id(other.id)
    {
        if (fail_move)
            throw std::runtime_error("move");
        ++alive;
    }
    Payload& operator=(Payload const& other)
    {
        if (fail_assign)
            throw std::runtime_error("assign");
        id = other.id;
        return *this;
    }
    Payload& operator=(Payload&& other) noexcept(false)
    {
        if (fail_assign)
            throw std::runtime_error("assign");
        id = other.id;
        return *this;
    }
    ~Payload() { --alive; }
};
struct ResetPayload : testing::Test {
    void SetUp() override
    {
        ASSERT_EQ(Payload::alive, 0);
        Payload::fail_copy = Payload::fail_move = Payload::fail_assign = false;
    }
    void TearDown() override
    {
        EXPECT_EQ(Payload::alive, 0);
        Payload::fail_copy = Payload::fail_move = Payload::fail_assign = false;
    }
};
using ErrorOrLifetime = ResetPayload;

static_assert(!std::is_default_constructible_v<ErrorOr<int>>);
static_assert(!std::is_nothrow_move_constructible_v<ErrorOr<int, Payload>>);
static_assert(!std::is_nothrow_move_assignable_v<ErrorOr<int, Payload>>);
static_assert(std::is_nothrow_move_constructible_v<ErrorOr<int, std::unique_ptr<int>>>);
static_assert(!std::is_copy_constructible_v<ErrorOr<int, std::unique_ptr<int>>>);

}

TEST_F(ErrorOrLifetime, FailedCopyAssignmentPreservesBothAlternatives)
{
    ErrorOr<Payload, int> value(Payload(7));
    ErrorOr<Payload, int> error(42);
    Payload::fail_copy = true;
    EXPECT_THROW({ auto copy = value; (void)copy; }, std::runtime_error);
    EXPECT_THROW(error = value, std::runtime_error);
    ASSERT_TRUE(error.has_error());
    EXPECT_EQ(error.error(), 42);
    EXPECT_EQ(value.value().id, 7);
    EXPECT_EQ(Payload::alive, 1);

    ErrorOr<int, Payload> source(Payload(9));
    ErrorOr<int, Payload> target(11);
    EXPECT_THROW(target = source, std::runtime_error);
    ASSERT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), 11);
    EXPECT_EQ(Payload::alive, 2);
}

TEST_F(ErrorOrLifetime, FailedMoveConstructionAndAssignmentDoNotTerminateOrDestroyInactiveStorage)
{
    ErrorOr<int, Payload> source(Payload(9));
    ErrorOr<int, Payload> target(11);
    Payload::fail_move = true;
    EXPECT_THROW({ auto moved = std::move(source); (void)moved; }, std::runtime_error);
    EXPECT_THROW(target = std::move(source), std::runtime_error);
    EXPECT_TRUE(source.has_error());
    EXPECT_TRUE(target.has_value());
    EXPECT_EQ(target.value(), 11);
    EXPECT_EQ(Payload::alive, 1);

    Payload::fail_move = false;
    target = std::move(source);
    EXPECT_TRUE(source.has_error()); // Moved-from alternatives remain destructible.
    EXPECT_EQ(target.error().id, 9);
    EXPECT_EQ(Payload::alive, 2);
}

TEST_F(ErrorOrLifetime, ThrowingSettersPreserveAValidAlternative)
{
    ErrorOr<Payload, int> result(42);
    Payload value(7);
    Payload::fail_copy = true;
    EXPECT_THROW(result.set_value(value), std::runtime_error);
    EXPECT_EQ(result.error(), 42);
    Payload::fail_copy = false;
    Payload::fail_move = true;
    EXPECT_THROW(result.set_value(value), std::runtime_error);
    EXPECT_EQ(result.error(), 42);
    Payload::fail_move = false;
    result.set_value(value);
    EXPECT_EQ(result.value().id, 7);
    result.set_error(8);
    EXPECT_EQ(result.error(), 8);

    ErrorOr<int, Payload> other(12);
    Payload::fail_copy = true;
    EXPECT_THROW(other.set_error(value), std::runtime_error);
    EXPECT_EQ(other.value(), 12);
    Payload::fail_copy = false;
    Payload::fail_move = true;
    EXPECT_THROW(other.set_error(value), std::runtime_error);
    EXPECT_EQ(other.value(), 12);
}

TEST_F(ErrorOrLifetime, SameAlternativeAssignmentAndSelfAssignmentRemainValid)
{
    ErrorOr<Payload, int> first(Payload(1)), second(Payload(2));
    Payload::fail_assign = true;
    EXPECT_THROW(first = second, std::runtime_error);
    EXPECT_TRUE(first.has_value());
    EXPECT_EQ(Payload::alive, 2);
    Payload::fail_assign = false;
    auto& alias = first;
    first = alias;
    first = std::move(alias);
    EXPECT_TRUE(first.has_value());
    first.set_value(first.value());
    EXPECT_EQ(first.value().id, 1);
    EXPECT_EQ(Payload::alive, 2);
}

TEST(ErrorOrTest, CustomErrorTypeSurvivesRebinding)
{
    auto failure = ErrorOr<int, std::string>::from_error("retained");
    auto rebound = failure.error_or(2.5);
    static_assert(std::is_same_v<decltype(rebound), ErrorOr<double, std::string>>);
    EXPECT_EQ(rebound.error(), "retained");
    auto success = ErrorOr<int, std::string>::from_value(7).error_or(2.5);
    EXPECT_DOUBLE_EQ(success.value(), 2.5);
}

TEST(ErrorOrTest, MoveOnlyErrorsValuesAndEqualAlternativeTypes)
{
    auto failure = ErrorOr<int, std::unique_ptr<int>>::from_error(std::make_unique<int>(7));
    auto rebound = std::move(failure).error_or(2.5);
    EXPECT_EQ(*rebound.error(), 7);
    auto success = ErrorOr<std::unique_ptr<int>, int>::from_value(std::make_unique<int>(9));
    auto pointer = std::move(success).value();
    EXPECT_EQ(*pointer, 9);
    auto same_value = ErrorOr<int, int>::from_value(1);
    auto same_error = ErrorOr<int, int>::from_error(2);
    EXPECT_EQ(same_value.value(), 1);
    EXPECT_EQ(same_error.error(), 2);
}
