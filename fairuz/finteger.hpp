#ifndef FA_INTEGER_HPP
#define FA_INTEGER_HPP

#include "fstring.hpp"
#include "fvalue.hpp"
#include <vector>

namespace fairuz::integer {

using namespace runtime;
using Limbs = std::vector<u32>;
// Zero is an empty magnitude, with positive sign. Results eagerly demote.
struct Data {
    Limbs limbs;
    bool positive { true };
    bool operator==(Data const&) const = default;
};

Data from_i64(i64 value);
Data parse(StringRef const& text, int base);

Value finish(Data data, GarbageCollector& gc);
Value add(Value lhs, Value rhs, GarbageCollector& gc);
Value sub(Value lhs, Value rhs, GarbageCollector& gc);
Value mul(Value lhs, Value rhs, GarbageCollector& gc);
Value neg(Value value, GarbageCollector& gc);
Value div(Value lhs, Value rhs, GarbageCollector& gc, bool remainder = false);
Value bitwise(Value lhs, Value rhs, char op, GarbageCollector& gc);
Value shift(Value value, Value count, bool left, GarbageCollector& gc);
Value pow(Value base, Value exponent, GarbageCollector& gc);
Value from_double(double value, GarbageCollector& gc);

int compare(Value lhs, Value rhs) noexcept;
// Numeric comparison is exact even when only one operand is an integer.
// Returns 2 for unordered (NaN).
int compare_numbers(Value lhs, Value rhs);
bool to_i64(Value value, i64& result) noexcept;
double to_double(Value value) noexcept;
std::string to_string(Value value);

}
#endif
