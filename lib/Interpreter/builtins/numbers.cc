//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Builtin mathematical utilities implementation.
//===----------------------------------------------------------------------===//

#include "Diag/errc.hpp"
#include "Interpreter/integer.hpp"
#include "Interpreter/vm.hpp"
#include "util.hpp"

#include <cmath>

namespace fairuz::runtime {

VM::RuntimeResult VM::floor(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::FLOOR_ARG_COUNT, "got " + std::to_string(argc));
    if (!argv[0].is_number()) return raise_runtime_error(ErrorCode::FLOOR_TYPE_ERROR);
    if (argv[0].is_int()) return argv[0];

    return integer::from_double(std::floor(argv[0].as_double()), m_gc);
}

VM::RuntimeResult VM::ceil(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::CEIL_ARG_COUNT, "got " + std::to_string(argc));
    if (!argv[0].is_number()) return raise_runtime_error(ErrorCode::CEIL_TYPE_ERROR);
    if (argv[0].is_int()) return argv[0];

    return integer::from_double(std::ceil(argv[0].as_double()), m_gc);
}

VM::RuntimeResult VM::round(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::ROUND_ARG_COUNT, "got " + std::to_string(argc));
    if (!argv[0].is_number()) return raise_runtime_error(ErrorCode::ROUND_TYPE_ERROR);
    if (argv[0].is_int()) return argv[0];

    return integer::from_double(std::round(argv[0].as_double()), m_gc);
}

VM::RuntimeResult VM::abs(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::ABS_ARG_COUNT, "got " + std::to_string(argc));
    if (!argv[0].is_number()) return raise_runtime_error(ErrorCode::ABS_TYPE_ERROR);
    if (argv[0].is_int()) {
        return integer::compare(argv[0], Value::from_int(0, m_gc)) < 0 ? integer::neg(argv[0], m_gc) : argv[0];
    }
    return Value::from_real(std::fabs(argv[0].as_double()));
}

VM::RuntimeResult VM::min(int argc, Value* argv)
{
    if (argc < 1 || !argv) return raise_runtime_error(ErrorCode::MIN_ARG_COUNT, "got " + std::to_string(argc));

    // Determine mode from argv[0]
    bool all_numbers = argv[0].is_number();
    bool all_ints = argv[0].is_int();
    bool all_strs = argv[0].is_string();

    // Validate all args match the expected type
    for (int i = 1; i < argc; i++) {
        all_numbers = all_numbers && argv[i].is_number();
        all_ints = all_ints && argv[i].is_int();
        all_strs = all_strs && argv[i].is_string();
    }

    if (all_strs) {
        Value ret = argv[0];
        for (int i = 1; i < argc; i++) {
            if (argv[i].as_string()->str < ret.as_string()->str) ret = argv[i];
        }

        return ret;
    }

    if (!all_numbers)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR, "min expects either all numbers or all strings");

    if (all_ints) {
        Value ret = argv[0];
        for (int i = 1; i < argc; ++i)
            if (integer::compare(argv[i], ret) < 0) ret = argv[i];
        return ret;
    }
    Value ret = Value::from_real(argv[0].as_double_any());
    for (int i = 1; i < argc; i++) ret = Value::from_real(std::fmin(ret.as_double_any(), argv[i].as_double_any()));

    return ret;
}

VM::RuntimeResult VM::max(int argc, Value* argv)
{
    if (argc < 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::MAX_ARG_COUNT, "got " + std::to_string(argc));

    // Determine mode from argv[0]
    bool all_numbers = argv[0].is_number();
    bool all_ints = argv[0].is_int();
    bool all_strs = argv[0].is_string();

    // Validate all args match the expected type
    for (int i = 1; i < argc; i++) {
        all_numbers = all_numbers && argv[i].is_number();
        all_ints = all_ints && argv[i].is_int();
        all_strs = all_strs && argv[i].is_string();
    }

    if (all_strs) {
        Value ret = argv[0];
        for (int i = 1; i < argc; i++) {
            if (argv[i].as_string()->str > ret.as_string()->str) ret = argv[i];
        }
        return ret;
    }

    if (!all_numbers)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR, "max expects either all numbers or all strings");

    if (all_ints) {
        Value ret = argv[0];
        for (int i = 1; i < argc; ++i)
            if (integer::compare(argv[i], ret) > 0) ret = argv[i];
        return ret;
    }
    Value ret = Value::from_real(argv[0].as_double_any());
    for (int i = 1; i < argc; i++) ret = Value::from_real(std::fmax(ret.as_double_any(), argv[i].as_double_any()));

    return ret;
}

VM::RuntimeResult VM::pow(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::POW_ARG_COUNT, "got " + std::to_string(argc));

    Value base = argv[0];
    Value exponent = argv[1];

    if (UNLIKELY(!base.is_number() || !exponent.is_number())) return raise_runtime_error(ErrorCode::POW_TYPE_ERROR);
    if (base.is_int() && exponent.is_int()) return integer::pow(base, exponent, m_gc);
    return Value::from_real(std::pow(base.as_double_any(), exponent.as_double_any()));
}

VM::RuntimeResult VM::sqrt(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::SQRT_ARG_COUNT, "got " + std::to_string(argc));

    Value n = argv[0];

    if (UNLIKELY(!n.is_number())) return raise_runtime_error(ErrorCode::SQRT_TYPE_ERROR);

    f64 val = n.as_double_any();
    if (val < 0.0) return raise_runtime_error(ErrorCode::RUNTIME_ERROR);

    return Value::from_real(std::sqrt(val));
}

VM::RuntimeResult VM::math_unary(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT, "got " + std::to_string(argc));
    if (!argv[0].is_string() || !argv[1].is_number()) return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
    std::string_view operation = string_bytes(argv[0]);
    f64 value = argv[1].as_double_any();
    if (operation == "sin") return Value::from_real(std::sin(value));
    if (operation == "cos") return Value::from_real(std::cos(value));
    if (operation == "tan") return Value::from_real(std::tan(value));
    if (operation == "log") return Value::from_real(std::log(value));
    if (operation == "exp") return Value::from_real(std::exp(value));
    return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
}

VM::RuntimeResult VM::math_binary(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT, "got " + std::to_string(argc));
    if (!argv[0].is_string() || !argv[1].is_number() || !argv[2].is_number())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);

    std::string_view operation = string_bytes(argv[0]);
    f64 first = argv[1].as_double_any();
    f64 second = argv[2].as_double_any();
    if (operation == "hypot") return Value::from_real(std::hypot(first, second));
    if (operation == "atan2") return Value::from_real(std::atan2(first, second));
    return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
}

VM::RuntimeResult VM::number_from_text(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR, "number parsing expects one string");
    StringRef text = argv[0].as_string()->str;
    if (text.empty() || text[0] == '+') return raise_runtime_error(ErrorCode::RUNTIME_ERROR);

    bool integral = true;
    for (size_t i = 0; i < text.len(); ++i) {
        if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') {
            integral = false;
            break;
        }
    }
    if (integral) {
        i64 value = 0;
        auto parsed = std::from_chars(text.data(), text.data() + text.len(), value);
        if (parsed.ec == std::errc() && parsed.ptr == text.data() + text.len()) return Value::from_int(value, m_gc);
        if (parsed.ec == std::errc::result_out_of_range && parsed.ptr == text.data() + text.len())
            return integer::finish(integer::parse(text, 10), m_gc);
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }

    std::string owned(text.data(), text.len());
    char* end = nullptr;
    errno = 0;
    double value = std::strtod(owned.c_str(), &end);
    if (errno == ERANGE || end != owned.c_str() + owned.size() || !std::isfinite(value))
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    return Value::from_real(value);
}

VM::RuntimeResult VM::number_finite(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_number()) return Value::from_bool(false);
    return Value::from_bool(argv[0].is_int() || std::isfinite(argv[0].as_double()));
}

VM::RuntimeResult VM::number_is_nan(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_number()) return Value::from_bool(false);
    return Value::from_bool(argv[0].is_double() && std::isnan(argv[0].as_double()));
}

} // fairuz::runtime
