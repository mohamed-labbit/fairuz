#include "fvalue.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>

using fairuz::runtime::Value;

TEST(ValueReal, NaNSignsAndPayloadsRemainNumbers)
{
    // Construct the bits directly so coverage does not depend on the host
    // math library's choice of NaN sign or payload (e.g. pow(-1, 0.5)).
    for (uint64_t sign : { UINT64_C(0), UINT64_C(0x8000000000000000) }) {
        for (unsigned bit = 0; bit < 52; ++bit) {
            for (uint64_t payload : { UINT64_C(1) << bit,
                     (UINT64_C(1) << bit) | UINT64_C(0x0008000000000000),
                     UINT64_C(0x000fffffffffffff) }) {
                uint64_t bits = sign | UINT64_C(0x7ff0000000000000) | payload;
                SCOPED_TRACE(bits);
                Value value = Value::from_real(std::bit_cast<double>(bits));
                ASSERT_FALSE(value.is_obj());
                ASSERT_TRUE(value.is_double());
                EXPECT_TRUE(value.is_number());
                EXPECT_FALSE(value.is_int());
                EXPECT_FALSE(value.is_nil());
                EXPECT_FALSE(value.is_bool());
                EXPECT_TRUE(std::isnan(value.as_double()));
            }
        }
    }
}

TEST(ValueReal, NonNaNBitsArePreserved)
{
    for (uint64_t sign : { UINT64_C(0), UINT64_C(0x8000000000000000) }) {
        for (uint64_t magnitude : { UINT64_C(0), UINT64_C(1),
                 UINT64_C(0x000fffffffffffff), UINT64_C(0x0010000000000000),
                 UINT64_C(0x3ff0000000000000), UINT64_C(0x7fefffffffffffff),
                 UINT64_C(0x7ff0000000000000) }) {
            uint64_t bits = sign | magnitude;
            SCOPED_TRACE(bits);
            Value value = Value::from_real(std::bit_cast<double>(bits));
            ASSERT_TRUE(value.is_double());
            EXPECT_EQ(std::bit_cast<uint64_t>(value.as_double()), bits);
        }
    }
}
