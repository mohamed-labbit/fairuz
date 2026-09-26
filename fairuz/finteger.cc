#include "finteger.hpp"
#include "fgc.hpp"
#include "futil.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <span>

namespace fairuz::integer {
namespace {

using View = std::span<u32 const>;
// Resource policy: at most 2^24 bits per integer (2 MiB of limbs).
constexpr size_t max_limbs = 1u << 19;

void check_size(size_t a, size_t b = 0)
{
    if (a > max_limbs || b > max_limbs - a)
        diagnostic::fatal_error(ErrorCode::ALLOC_FAILED, "integer exceeds the 2^24-bit resource limit");
}

void normalize(Data& d)
{
    while (!d.limbs.empty() && d.limbs.back() == 0)
        d.limbs.pop_back();
    if (d.limbs.empty())
        d.positive = true;
}

struct Operand {
    u32 local[2] { };
    View limbs;
    bool positive;

    explicit Operand(Value v)
    {
        if (v.is_big_int()) {
            limbs = v.as_big_int()->limbs;
            positive = v.as_big_int()->sign;
        } else {
            i64 n = v.as_int();
            positive = n >= 0;
            u64 bits = static_cast<u64>(n);
            u64 mag = positive ? bits : u64 { 0 } - bits;
            local[0] = static_cast<u32>(mag);
            local[1] = static_cast<u32>(mag >> 32);
            limbs = View(local, local[1] ? 2 : local[0] ? 1
                                                        : 0);
        }
    }
};

int cmp(View a, View b) noexcept
{
    if (a.size() != b.size())
        return a.size() < b.size() ? -1 : 1;

    for (size_t i = a.size(); i-- > 0;) {
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    }

    return 0;
}

Limbs plus(View a, View b)
{
    size_t n = std::max(a.size(), b.size());
    check_size(n, 1);
    Limbs out;
    out.reserve(n + 1);
    u64 carry = 0;

    for (size_t i = 0; i < n; ++i) {
        u64 total = (i < a.size() ? u64(a[i]) : 0) + (i < b.size() ? u64(b[i]) : 0) + carry;
        out.push_back(static_cast<u32>(total));
        carry = total >> 32;
    }

    if (carry)
        out.push_back(static_cast<u32>(carry));

    return out;
}

Limbs minus(View a, View b)
{
    assert(cmp(a, b) >= 0);
    Limbs out(a.size());
    u64 borrow = 0;

    for (size_t i = 0; i < a.size(); ++i) {
        u64 sub = (i < b.size() ? u64(b[i]) : 0) + borrow;
        out[i] = static_cast<u32>(u64(a[i]) - sub);
        borrow = u64(a[i]) < sub;
    }

    while (!out.empty() && !out.back())
        out.pop_back();

    return out;
}

Data signed_add(Operand const& a, Operand const& b, bool subtract)
{
    bool bp = b.positive != subtract;

    if (a.positive == bp)
        return { plus(a.limbs, b.limbs), a.positive };

    int order = cmp(a.limbs, b.limbs);
    if (!order)
        return { };

    return order > 0
        ? Data { minus(a.limbs, b.limbs), a.positive }
        : Data { minus(b.limbs, a.limbs), bp };
}

Data copy(Value v)
{
    Operand a(v);
    return { Limbs(a.limbs.begin(), a.limbs.end()), a.positive };
}

size_t bits(View a)
{
    return a.empty() ? 0 : (a.size() - 1) * 32 + std::bit_width(a.back());
}

std::pair<Limbs, Limbs> divmod(View a, View b)
{
    if (b.empty())
        diagnostic::fatal_error(ErrorCode::DIVISION_BY_ZERO);

    if (cmp(a, b) < 0)
        return { { }, Limbs(a.begin(), a.end()) };

    Limbs q(a.size()), r;

    if (b.size() == 1) {
        u64 rem = 0;

        for (size_t i = a.size(); i-- > 0;) {
            u64 cur = (rem << 32) | a[i];
            q[i] = static_cast<u32>(cur / b[0]);
            rem = cur % b[0];
        }

        if (rem)
            r.push_back(static_cast<u32>(rem));
    } else {
        check_size(b.size(), 1);
        r.reserve(b.size() + 1);

        for (size_t bit = bits(a); bit-- > 0;) {
            u64 carry = (a[bit / 32] >> (bit % 32)) & 1;

            for (auto& limb : r) {
                u64 v = (u64(limb) << 1) | carry;
                limb = static_cast<u32>(v);
                carry = v >> 32;
            }

            if (carry)
                r.push_back(static_cast<u32>(carry));

            if (cmp(r, b) >= 0) {
                r = minus(r, b);
                q[bit / 32] |= u32 { 1 } << (bit % 32);
            }
        }
    }

    while (!q.empty() && !q.back())
        q.pop_back();

    return { std::move(q), std::move(r) };
}

void twos(Limbs& a)
{
    u64 carry = 1;
    for (auto& limb : a) {
        u64 n = u64(~limb) + carry;
        limb = static_cast<u32>(n);
        carry = n >> 32;
    }
}

}

Data from_i64(i64 value)
{
    u64 bits = static_cast<u64>(value);
    u64 magnitude = value < 0 ? u64 { 0 } - bits : bits;
    Data d { { }, value >= 0 };

    if (magnitude)
        d.limbs.push_back(static_cast<u32>(magnitude));
    if (magnitude >> 32)
        d.limbs.push_back(static_cast<u32>(magnitude >> 32));

    return d;
}

Value finish(Data d, GarbageCollector& gc)
{
    normalize(d);
    check_size(d.limbs.size());

    if (d.limbs.size() <= 2) {
        u64 mag = d.limbs.empty() ? 0 : d.limbs[0];
        if (d.limbs.size() == 2)
            mag |= u64(d.limbs[1]) << 32;

        u64 bound = d.positive ? u64(Value::int_max()) : u64 { 0 } - u64(Value::int_min());
        if (mag <= bound)
            return Value::from_int(d.positive ? static_cast<i64>(mag) : -static_cast<i64>(mag));
    }

    // GC is nonmoving and collection occurs only at VM dispatch boundaries.
    // Operand views have gone out of use before publishing this allocation.
    return Value::from_obj(&gc.make_obj_int(std::move(d))->obj);
}

Value add(Value lhs, Value rhs, GarbageCollector& gc)
{
    if (!lhs.is_big_int() && !rhs.is_big_int()) {
#if defined(__GNUC__) || defined(__clang__)
        i64 result;
        if (!__builtin_add_overflow(lhs.as_int(), rhs.as_int(), &result))
            return Value::from_int(result, gc);
#else
        // Two signed 48-bit payloads have a signed 49-bit sum.
        return Value::from_int(lhs.as_int() + rhs.as_int(), gc);
#endif
    }

    Operand a(lhs), b(rhs);
    return finish(signed_add(a, b, false), gc);
}

Value sub(Value lhs, Value rhs, GarbageCollector& gc)
{
    if (!lhs.is_big_int() && !rhs.is_big_int()) {
#if defined(__GNUC__) || defined(__clang__)
        i64 result;
        if (!__builtin_sub_overflow(lhs.as_int(), rhs.as_int(), &result))
            return Value::from_int(result, gc);
#else
        // The difference of signed 48-bit payloads also fits signed 49 bits.
        return Value::from_int(lhs.as_int() - rhs.as_int(), gc);
#endif
    }

    Operand a(lhs), b(rhs);
    return finish(signed_add(a, b, true), gc);
}

Value mul(Value lhs, Value rhs, GarbageCollector& gc)
{
#if defined(__GNUC__) || defined(__clang__)
    if (!lhs.is_big_int() && !rhs.is_big_int()) {
        i64 result;
        if (!__builtin_mul_overflow(lhs.as_int(), rhs.as_int(), &result))
            return Value::from_int(result, gc);
    }
#endif

    Operand a(lhs), b(rhs);
    if (a.limbs.empty() || b.limbs.empty())
        return Value::from_int(0);

    check_size(a.limbs.size(), b.limbs.size());
    Limbs out(a.limbs.size() + b.limbs.size());

    for (size_t i = 0; i < a.limbs.size(); ++i) {
        u64 carry = 0;
        for (size_t j = 0; j < b.limbs.size(); ++j) {
            u64 total = u64(a.limbs[i]) * b.limbs[j] + out[i + j] + carry;
            out[i + j] = static_cast<u32>(total);
            carry = total >> 32;
        }
        out[i + b.limbs.size()] = static_cast<u32>(carry);
    }

    return finish({ std::move(out), a.positive == b.positive }, gc);
}

Value neg(Value v, GarbageCollector& gc)
{
    if (!v.is_big_int())
        return Value::from_int(-v.as_int(), gc); // signed 48-bit payload

    Data d = copy(v);
    d.positive = !d.positive;
    return finish(std::move(d), gc);
}

int compare(Value lhs, Value rhs) noexcept
{
    if (!lhs.is_big_int() && !rhs.is_big_int())
        return lhs.as_int() < rhs.as_int() ? -1 : lhs.as_int() > rhs.as_int() ? 1
                                                                              : 0;

    Operand a(lhs), b(rhs);
    if (a.positive != b.positive)
        return a.positive ? 1 : -1;

    int c = cmp(a.limbs, b.limbs);
    return a.positive ? c : -c;
}

bool to_i64(Value v, i64& result) noexcept
{
    Operand a(v);
    if (a.limbs.size() > 2)
        return false;

    u64 n = a.limbs.empty() ? 0 : a.limbs[0];

    if (a.limbs.size() == 2)
        n |= u64(a.limbs[1]) << 32;

    if (a.positive) {
        if (n > INT64_MAX)
            return false;
        result = static_cast<i64>(n);
    } else {
        if (n > (u64 { 1 } << 63))
            return false;
        result = n == (u64 { 1 } << 63) ? INT64_MIN : -static_cast<i64>(n);
    }

    return true;
}

double to_double(Value v) noexcept
{
    if (!v.is_big_int())
        return static_cast<double>(v.as_int());

    Operand a(v);
    double out = 0;
    for (size_t i = a.limbs.size(); i-- > 0;)
        out = std::ldexp(out, 32) + a.limbs[i];

    return a.positive ? out : -out;
}

Value div(Value lhs, Value rhs, GarbageCollector& gc, bool remainder)
{
    if (!lhs.is_big_int() && !rhs.is_big_int()) {
        i64 a = lhs.as_int(), b = rhs.as_int();
        if (b == 0)
            diagnostic::fatal_error(remainder ? ErrorCode::MODULO_BY_ZERO : ErrorCode::DIVISION_BY_ZERO);
        // Signed 48-bit operands cannot hit INT64_MIN / -1.
        i64 rem = a % b;
        if (remainder)
            return Value::from_int(rem);
        if (!rem)
            return Value::from_int(a / b, gc);
        return Value::from_real(static_cast<double>(a) / static_cast<double>(b));
    }

    Operand a(lhs), b(rhs);
    if (b.limbs.empty())
        diagnostic::fatal_error(remainder ? ErrorCode::MODULO_BY_ZERO : ErrorCode::DIVISION_BY_ZERO);

    auto [q, r] = divmod(a.limbs, b.limbs);
    if (remainder)
        return finish({ std::move(r), a.positive }, gc);
    if (r.empty())
        return finish({ std::move(q), a.positive == b.positive }, gc);

    // Scale both magnitudes to avoid inf/inf for large but finite ratios.
    auto head = [](View v) {
        size_t n = std::min(size_t { 3 }, v.size());
        double x = 0;
        for (size_t i = v.size(); i-- > v.size() - n;)
            x = std::ldexp(x, 32) + v[i];
        return x;
    };

    long exponent = (static_cast<long>(a.limbs.size() > 3
                             ? a.limbs.size() - 3
                             : 0)
                        - static_cast<long>(b.limbs.size() > 3 ? b.limbs.size() - 3 : 0))
        * 32;
    double ratio = std::ldexp(head(a.limbs) / head(b.limbs), static_cast<int>(exponent));
    return Value::from_real(a.positive == b.positive ? ratio : -ratio);
}

Value bitwise(Value lhs, Value rhs, char op, GarbageCollector& gc)
{
    if (!lhs.is_big_int() && !rhs.is_big_int()) {
        i64 a = lhs.as_int(), b = rhs.as_int();
        return Value::from_int(op == '&' ? a & b : op == '|' ? a | b
                                                             : a ^ b);
    }

    Operand a(lhs), b(rhs);
    size_t n = std::max(a.limbs.size(), b.limbs.size());
    check_size(n, 1);
    ++n;
    Limbs x(a.limbs.begin(), a.limbs.end()), y(b.limbs.begin(), b.limbs.end());
    x.resize(n);
    y.resize(n);

    if (!a.positive)
        twos(x);
    if (!b.positive)
        twos(y);
    for (size_t i = 0; i < n; ++i)
        x[i] = op == '&' ? x[i] & y[i] : op == '|' ? x[i] | y[i]
                                                   : x[i] ^ y[i];

    bool positive = !(x.back() & 0x80000000u);
    if (!positive)
        twos(x);

    return finish({ std::move(x), positive }, gc);
}

Value shift(Value value, Value count, bool left, GarbageCollector& gc)
{
    Operand a(value), c(count);
    if (!c.positive)
        diagnostic::fatal_error(ErrorCode::SHIFT_AMOUNT_OUT_OF_RANGE);

    i64 amount;
    if (!to_i64(count, amount)) {
        if (left && !a.limbs.empty())
            diagnostic::fatal_error(ErrorCode::ALLOC_FAILED);
        return Value::from_int(left || a.positive ? 0 : -1);
    }

    if (a.limbs.empty())
        return Value::from_int(0);
    if (!value.is_big_int()) {
        if (!left)
            return Value::from_int(amount >= 64 ? (a.positive ? 0 : -1) : value.as_int() >> amount);
#if defined(__GNUC__) || defined(__clang__)
        i64 result;
        if (amount < 63 && !__builtin_mul_overflow(value.as_int(), i64 { 1 } << amount, &result))
            return Value::from_int(result, gc);
#endif
    }

    u64 words = static_cast<u64>(amount) / 32;
    unsigned shift = static_cast<unsigned>(amount % 32);

    if (!left && words >= a.limbs.size())
        return Value::from_int(a.positive ? 0 : -1);
    if (left) {
        if (words > max_limbs)
            diagnostic::fatal_error(ErrorCode::ALLOC_FAILED);

        check_size(a.limbs.size(), static_cast<size_t>(words) + (shift != 0));
        Limbs out(a.limbs.size() + static_cast<size_t>(words) + (shift != 0));
        u64 carry = 0;
        for (size_t i = 0; i < a.limbs.size(); ++i) {
            u64 n = (u64(a.limbs[i]) << shift) | carry;
            out[i + words] = static_cast<u32>(n);
            carry = n >> 32;
        }

        if (shift)
            out.back() = static_cast<u32>(carry);
        return finish({ std::move(out), a.positive }, gc);
    }

    Limbs out(a.limbs.size() - static_cast<size_t>(words));
    bool discarded = false;

    for (size_t i = 0; i < words; ++i)
        discarded |= a.limbs[i] != 0;

    if (shift)
        discarded |= (a.limbs[words] & ((u32 { 1 } << shift) - 1)) != 0;

    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = a.limbs[i + words] >> shift;
        if (shift && i + words + 1 < a.limbs.size())
            out[i] |= a.limbs[i + words + 1] << (32 - shift);
    }

    if (!a.positive && discarded) {
        u32 one = 1;
        out = plus(out, View(&one, 1));
    }

    return finish({ std::move(out), a.positive }, gc);
}

Value pow(Value base, Value exponent, GarbageCollector& gc)
{
    if (compare(exponent, Value::from_int(0)) < 0)
        return Value::from_real(std::pow(to_double(base), to_double(exponent)));

    Operand e(exponent);
    Value out = Value::from_int(1);
    size_t n = bits(e.limbs);

    for (size_t i = 0; i < n; ++i) {
        if ((e.limbs[i / 32] >> (i % 32)) & 1)
            out = mul(out, base, gc);
        if (i + 1 < n)
            base = mul(base, base, gc);
    }

    return out;
}

Data parse(StringRef const& text, int base)
{
    Data d;
    size_t pos = 0;
    if (text.len() && text.at(0) == '-') {
        d.positive = false;
        ++pos;
    }

    auto c_1 = text.len() >= pos + 2 ? text.at(pos) : ' ';
    auto c_2 = text.len() >= pos + 2 ? text.at(pos + 1) : ' ';

    if (c_1 == '0' && (c_2 == 'x' || c_2 == 'X' || c_2 == 'b' || c_2 == 'B' || c_2 == 'o' || c_2 == 'O'))
        pos += 2;

    while (pos < text.len()) {
        u64 bytes;
        u32 cp = util::decode_utf8_at(text, pos, &bytes);
        pos += bytes;
        if (cp == '_' || cp == '\'')
            continue;

        int digit = cp >= '0' && cp <= '9'
            ? cp - '0'
            : util::is_arab_digit(cp) ? util::arab_digit_to_canon(cp)
            : cp >= 'a' && cp <= 'f'  ? cp - 'a' + 10
            : cp >= 'A' && cp <= 'F'  ? cp - 'A' + 10
                                      : -1;

        if (digit < 0 || digit >= base)
            diagnostic::fatal_error(ErrorCode::INVALID_NUMBER_LITERAL);

        u64 carry = digit;
        for (auto& limb : d.limbs) {
            u64 v = u64(limb) * base + carry;
            limb = static_cast<u32>(v);
            carry = v >> 32;
        }

        if (carry) {
            check_size(d.limbs.size(), 1);
            d.limbs.push_back(static_cast<u32>(carry));
        }
    }
    normalize(d);
    return d;
}
namespace {

Data double_data(double value)
{
    Data d { { }, !std::signbit(value) };
    double n = std::trunc(std::fabs(value));

    while (n >= 1) {
        d.limbs.push_back(static_cast<u32>(std::fmod(n, 0x1p32)));
        n = std::floor(n / 0x1p32);
    }

    normalize(d);
    return d;
}

}

Value from_double(double value, GarbageCollector& gc)
{
    if (!std::isfinite(value))
        diagnostic::fatal_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
    return finish(double_data(value), gc);
}

int compare_numbers(Value lhs, Value rhs)
{
    if (lhs.is_int() && rhs.is_int())
        return compare(lhs, rhs);

    if (lhs.is_double() && rhs.is_double()) {
        double a = lhs.as_double(), b = rhs.as_double();
        return std::isnan(a) || std::isnan(b) ? 2 : a < b ? -1
            : a > b                                       ? 1
                                                          : 0;
    }

    bool reversed = lhs.is_double();
    Value iv = reversed ? rhs : lhs;
    double f = reversed ? lhs.as_double() : rhs.as_double();
    if (std::isnan(f))
        return 2;

    int c;
    if (std::isinf(f))
        c = f > 0 ? -1 : 1;
    else {
        Data d = double_data(f);
        Operand a(iv);
        c = a.positive != d.positive ? (a.positive ? 1 : -1) : (a.positive ? cmp(a.limbs, d.limbs) : -cmp(a.limbs, d.limbs));
        if (!c && f != std::trunc(f))
            c = f > 0 ? -1 : 1;
    }

    return reversed ? -c : c;
}

std::string to_string(Value value)
{
    if (!value.is_big_int())
        return std::to_string(value.as_int());

    Data d = copy(value);
    if (d.limbs.empty())
        return "0";

    std::string out;
    while (!d.limbs.empty()) {
        u64 rem = 0;
        for (size_t i = d.limbs.size(); i-- > 0;) {
            u64 cur = (rem << 32) | d.limbs[i];
            d.limbs[i] = static_cast<u32>(cur / 10);
            rem = cur % 10;
        }

        out.push_back(static_cast<char>('0' + rem));
        while (!d.limbs.empty() && !d.limbs.back())
            d.limbs.pop_back();
    }

    if (!d.positive)
        out.push_back('-');

    std::reverse(out.begin(), out.end());
    return out;
}

}
