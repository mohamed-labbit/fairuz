//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Builtin date-time utilities implementation.
//===----------------------------------------------------------------------===//

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include "util.hpp"

#include <sstream>

namespace fairuz::runtime {

namespace {

bool utc_zone(Value value)
{
    return value.is_string() && value.as_string()->str == "UTC";
}

bool supported_zone(Value value)
{
    return value.is_string()
        && (value.as_string()->str == "UTC" || value.as_string()->str == "محلي");
}

bool calendar_fields(std::time_t timestamp, bool utc, std::tm* output)
{
#if defined(_WIN32)
    return (utc ? ::gmtime_s(output, &timestamp) : ::localtime_s(output, &timestamp)) == 0;
#else
    return (utc ? ::gmtime_r(&timestamp, output) : ::localtime_r(&timestamp, output)) != nullptr;
#endif
}

std::time_t utc_timestamp(std::tm* value)
{
#if defined(_WIN32)
    return ::_mkgmtime(value);
#else
    return ::timegm(value);
#endif
}

} // namespace anonymous

VM::RuntimeResult VM::datetime_now(int argc, Value* argv)
{
    (void)argv;
    if (argc != 0)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR, "current time takes no arguments");
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return Value::from_int(std::chrono::duration_cast<std::chrono::seconds>(now).count(), m_gc);
}

VM::RuntimeResult VM::datetime_from_fields(int argc, Value* argv)
{
    if (argc != 7 || argv == nullptr || !supported_zone(argv[6]))
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime construction expects six integers and a supported zone");
    for (int i = 0; i < 6; ++i) {
        if (!argv[i].is_int())
            return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
                "datetime fields must be integers");
    }
    auto field = [&](int index, i64 offset = 0) -> ErrorOr<int, RuntimeError> {
        i64 n = argv[index].as_int();
        if (n < static_cast<i64>(std::numeric_limits<int>::min()) + offset
            || n > static_cast<i64>(std::numeric_limits<int>::max()) + offset)
            return raise_runtime_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
        return static_cast<int>(n - offset);
    };
    std::tm value { };
    value.tm_year = field(0, 1900).value();
    value.tm_mon = field(1, 1).value();
    value.tm_mday = field(2).value();
    value.tm_hour = field(3).value();
    value.tm_min = field(4).value();
    value.tm_sec = field(5).value();
    value.tm_isdst = -1;
    std::time_t timestamp = utc_zone(argv[6]) ? utc_timestamp(&value) : std::mktime(&value);
    if (timestamp == static_cast<std::time_t>(-1))
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    return Value::from_int(static_cast<i64>(timestamp), m_gc);
}

VM::RuntimeResult VM::datetime_to_fields(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_number() || !supported_zone(argv[1]))
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime fields expect epoch and supported zone");
    std::time_t timestamp = static_cast<std::time_t>(argv[0].is_int()
            ? argv[0].as_int()
            : integer::from_double(argv[0].as_double(), m_gc).as_int());
    std::tm fields { };
    if (!calendar_fields(timestamp, utc_zone(argv[1]), &fields))
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    Value result = m_gc.make_list();
    i64 values[] = {
        fields.tm_year + 1900,
        fields.tm_mon + 1,
        fields.tm_mday,
        fields.tm_hour,
        fields.tm_min,
        fields.tm_sec,
        fields.tm_wday,
        fields.tm_yday + 1,
    };
    for (i64 value : values)
        result.as_list()->elements.push(Value::from_int(value, m_gc));
    return result;
}

VM::RuntimeResult VM::datetime_parse(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !supported_zone(argv[2]))
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime parse expects text, format, and supported zone");
    std::string text = native_path(argv[0]);
    std::string format = native_path(argv[1]);
    if (format == "ISO8601")
        format = "%Y-%m-%dT%H:%M:%SZ";
    std::tm value { };
    std::istringstream stream(text);
    stream >> std::get_time(&value, format.c_str());
    if (stream.fail() || stream.peek() != std::char_traits<char>::eof())
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    value.tm_isdst = -1;
    std::time_t timestamp = utc_zone(argv[2]) ? utc_timestamp(&value) : std::mktime(&value);
    return timestamp == static_cast<std::time_t>(-1)
        ? Value::nil()
        : Value::from_int(static_cast<i64>(timestamp), m_gc);
}

VM::RuntimeResult VM::datetime_format(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_number() || !supported_zone(argv[1])
        || !argv[2].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime format expects epoch, zone, and format");
    std::time_t timestamp = static_cast<std::time_t>(argv[0].is_int()
            ? argv[0].as_int()
            : integer::from_double(argv[0].as_double(), m_gc).as_int());
    std::tm fields { };
    if (!calendar_fields(timestamp, utc_zone(argv[1]), &fields))
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    std::string format = native_path(argv[2]);
    if (format == "ISO8601")
        format = utc_zone(argv[1]) ? "%Y-%m-%dT%H:%M:%SZ" : "%Y-%m-%dT%H:%M:%S%z";
    char output[256];
    size_t size = std::strftime(output, sizeof(output), format.c_str(), &fields);
    return size == 0 ? Value::nil() : m_gc.make_string(output);
}

VM::RuntimeResult VM::clock(int argc, Value* argv)
{
    if (argc != 0)
        return raise_runtime_error(ErrorCode::CLOCK_ARG_COUNT);
    (void)argv;
    auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return Value::from_real(std::chrono::duration<f64>(elapsed).count());
}

VM::RuntimeResult VM::error(int argc, Value* argv)
{
    std::string detail;
    if (argc > 0 && argv != nullptr && argv[0].is_string()) {
        auto const& message = argv[0].as_string()->str;
        detail.assign(message.data(), message.len());
    }
    return raise_runtime_error(ErrorCode::RUNTIME_ERROR, detail);
}

VM::RuntimeResult VM::time(int /*argc*/, Value* /*argv*/) { return Value::nil(); }

} // namespace fairuz::runtime
