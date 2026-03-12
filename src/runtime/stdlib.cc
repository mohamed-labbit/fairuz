#include "../../include/runtime/stdlib.hpp"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iostream>
#include <sstream>

namespace mylang::runtime {

// Convert any numeric Value to double; returns quiet NaN on failure.
static double toNum(Value const& v)
{
    if (v.isInteger())
        return static_cast<double>(v.asInteger());
    if (v.isDouble())
        return v.asDouble();
    return std::numeric_limits<double>::quiet_NaN();
}

// I/O

Value nativePrint(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("print() is called with no arguments");
        return Value::nil();
    }

    for (int i = 0; i < argc; i++) {
        if (i > 0)
            std::cout << ' ';
        std::cout << argv[i];
    }
    std::cout << '\n';
    return Value::nil();
}

Value nativeInput(int argc, Value* argv)
{
    // optional prompt
    if (argc >= 1 && argv[0].isString())
        std::cout << argv[0].asString()->str;

    std::string line;
    std::getline(std::cin, line);
    return Value::object(makeObjectString(StringRef(line.c_str())));
}

// Type introspection / conversion

Value nativeType(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("type() is called with no arguments");
        return Value::nil();
    }

    // single arg → return one string; multiple args → return list of strings
    auto typeName = [](Value const& v) -> char const* {
        if (v.isNil())
            return "nil";
        if (v.isBoolean())
            return "bool";
        if (v.isInteger())
            return "int";
        if (v.isDouble())
            return "float";
        if (v.isString())
            return "string";
        if (v.isList())
            return "list";
        if (v.isClosure() || v.isFunction())
            return "function";
        if (v.isNative())
            return "native";
        return "unknown";
    };

    if (argc == 1)
        return Value::object(makeObjectString(StringRef(typeName(argv[0]))));

    ObjList* ret = makeObjectList();
    for (int i = 0; i < argc; i++)
        ret->elements.push(Value::object(makeObjectString(StringRef(typeName(argv[i])))));
    return Value::object(ret);
}

Value nativeInt(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("int() is called with no arguments");
        return Value::nil();
    }

    auto convert = [](Value const& v) -> Value {
        if (v.isInteger())
            return v;
        if (v.isDouble())
            return Value::integer(static_cast<int64_t>(v.asDouble()));
        if (v.isBoolean())
            return Value::integer(v.asBoolean() ? 1LL : 0LL);
        if (v.isString()) {
            StringRef const& s = v.asString()->str;
            std::string tmp(s.data(), s.len());
            char* end = nullptr;
            int64_t r = std::strtoll(tmp.c_str(), &end, 10);
            if (end == tmp.c_str()) {
                diagnostic::emit("int() cannot convert string \"" + tmp + "\" to integer");
                return Value::nil();
            }
            return Value::integer(r);
        }
        diagnostic::emit("int() cannot convert value");
        return Value::nil();
    };

    if (argc == 1)
        return convert(argv[0]);

    ObjList* ret = makeObjectList();
    for (int i = 0; i < argc; i++)
        ret->elements.push(convert(argv[i]));
    return Value::object(ret);
}

Value nativeFloat(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("float() is called with no arguments");
        return Value::nil();
    }

    auto convert = [](Value const& v) -> Value {
        if (v.isDouble())
            return v;
        if (v.isInteger())
            return Value::real(static_cast<double>(v.asInteger()));
        if (v.isBoolean())
            return Value::real(v.asBoolean() ? 1.0 : 0.0);
        if (v.isString()) {
            StringRef const& s = v.asString()->str;
            std::string tmp(s.data(), s.len());
            char* end = nullptr;
            double r = std::strtod(tmp.c_str(), &end);
            if (end == tmp.c_str()) {
                diagnostic::emit("float() cannot convert string \"" + tmp + "\" to float");
                return Value::nil();
            }
            return Value::real(r);
        }
        diagnostic::emit("float() cannot convert value");
        return Value::nil();
    };

    if (argc == 1)
        return convert(argv[0]);

    ObjList* ret = makeObjectList();
    for (int i = 0; i < argc; i++)
        ret->elements.push(convert(argv[i]));
    return Value::object(ret);
}

Value nativeStr(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("str() is called with no arguments");
        return Value::nil();
    }

    auto convert = [](Value const& v) -> Value {
        if (v.isString())
            return v;
        std::ostringstream oss;
        oss << v;
        return Value::object(makeObjectString(StringRef(oss.str().c_str())));
    };

    if (argc == 1)
        return convert(argv[0]);

    ObjList* ret = makeObjectList();
    for (int i = 0; i < argc; i++)
        ret->elements.push(convert(argv[i]));
    return Value::object(ret);
}

Value nativeBool(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("bool() is called with no arguments");
        return Value::nil();
    }

    if (argc == 1)
        return Value::boolean(argv[0].isTruthy());

    ObjList* ret = makeObjectList();
    for (int i = 0; i < argc; i++)
        ret->elements.push(Value::boolean(argv[i].isTruthy()));
    return Value::object(ret);
}

Value nativeList(int argc, Value* argv)
{
    ObjList* ret = makeObjectList();
    for (int i = 0; i < argc; i++)
        ret->elements.push(argv[i]);
    return Value::object(ret);
}

// Collection utilities

Value nativeLen(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("len() is called with no arguments");
        return Value::nil();
    }

    Value const& v = argv[0];
    if (v.isList())
        return Value::integer(static_cast<int64_t>(v.asList()->size()));
    if (v.isString())
        return Value::integer(static_cast<int64_t>(v.asString()->str.len()));

    diagnostic::emit("len() requires a string or list argument");
    return Value::nil();
}

Value nativeAppend(int argc, Value* argv)
{
    // append(list, val1, val2, ...)  →  mutates list, returns it
    if (argc < 2 || !argv) {
        diagnostic::emit("append() requires at least 2 arguments: append(list, value...)");
        return Value::nil();
    }
    if (!argv[0].isList()) {
        diagnostic::emit("append() first argument must be a list");
        return Value::nil();
    }

    ObjList* list = argv[0].asList();
    for (int i = 1; i < argc; i++)
        list->elements.push(argv[i]);

    return argv[0];
}

Value nativePop(int argc, Value* argv)
{
    // pop(list)  or  pop(list, index)  →  returns removed element
    if (argc < 1 || !argv) {
        diagnostic::emit("pop() requires at least 1 argument");
        return Value::nil();
    }
    if (!argv[0].isList()) {
        diagnostic::emit("pop() first argument must be a list");
        return Value::nil();
    }

    ObjList* list = argv[0].asList();
    if (list->elements.empty()) {
        diagnostic::emit("pop() called on empty list");
        return Value::nil();
    }

    if (argc >= 2 && argv[1].isInteger()) {
        int64_t idx = argv[1].asInteger();
        int64_t sz = static_cast<int64_t>(list->elements.size());
        if (idx < 0)
            idx += sz;
        if (idx < 0 || idx >= sz) {
            diagnostic::emit("pop() index out of range");
            return Value::nil();
        }
        Value removed = list->elements[static_cast<uint32_t>(idx)];
        list->elements.erase(static_cast<uint32_t>(idx));
        return removed;
    }

    return list->elements.pop();
}

Value nativeSlice(int argc, Value* argv)
{
    // slice(list_or_string, start [, end])
    if (argc < 2 || !argv) {
        diagnostic::emit("slice() requires at least 2 arguments: slice(collection, start [, end])");
        return Value::nil();
    }
    if (!argv[1].isInteger()) {
        diagnostic::emit("slice() start must be an integer");
        return Value::nil();
    }

    auto clamp = [](int64_t v, int64_t lo, int64_t hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };

    if (argv[0].isList()) {
        ObjList* src = argv[0].asList();
        int64_t sz = static_cast<int64_t>(src->size());
        int64_t start = argv[1].asInteger();
        int64_t end = (argc >= 3 && argv[2].isInteger()) ? argv[2].asInteger() : sz;

        if (start < 0)
            start += sz;
        if (end < 0)
            end += sz;
        start = clamp(start, 0, sz);
        end = clamp(end, 0, sz);

        ObjList* ret = makeObjectList();
        for (int64_t i = start; i < end; i++)
            ret->elements.push(src->elements[static_cast<uint32_t>(i)]);
        return Value::object(ret);
    }

    if (argv[0].isString()) {
        StringRef const& s = argv[0].asString()->str;
        int64_t sz = static_cast<int64_t>(s.len());
        int64_t start = argv[1].asInteger();
        int64_t end = (argc >= 3 && argv[2].isInteger()) ? argv[2].asInteger() : sz;

        if (start < 0)
            start += sz;
        if (end < 0)
            end += sz;
        start = clamp(start, 0, sz);
        end = clamp(end, 0, sz);

        StringRef sliced = s.slice(static_cast<size_t>(start),
            std::optional<size_t>(static_cast<size_t>(end)));
        return Value::object(makeObjectString(sliced));
    }

    diagnostic::emit("slice() first argument must be a list or string");
    return Value::nil();
}

// String utilities

Value nativeSplit(int argc, Value* argv)
{
    // split(string, delimiter)  →  list of strings
    if (argc < 2 || !argv) {
        diagnostic::emit("split() requires 2 arguments: split(string, delimiter)");
        return Value::nil();
    }
    if (!argv[0].isString()) {
        diagnostic::emit("split() first argument must be a string");
        return Value::nil();
    }
    if (!argv[1].isString()) {
        diagnostic::emit("split() delimiter must be a string");
        return Value::nil();
    }

    StringRef const& src = argv[0].asString()->str;
    StringRef const& delim = argv[1].asString()->str;
    ObjList* ret = makeObjectList();

    size_t const slen = src.len();
    size_t const dlen = delim.len();

    if (dlen == 0) {
        // split into individual characters
        for (size_t i = 0; i < slen; i++)
            ret->elements.push(Value::object(
                makeObjectString(src.slice(i, std::optional<size_t>(i + 1)))));
        return Value::object(ret);
    }

    size_t start = 0;
    size_t i = 0;
    while (i + dlen <= slen) {
        if (::memcmp(src.data() + i, delim.data(), dlen) == 0) {
            ret->elements.push(Value::object(
                makeObjectString(src.slice(start, std::optional<size_t>(i)))));
            start = i + dlen;
            i = start;
        } else {
            i++;
        }
    }
    // remainder
    ret->elements.push(Value::object(
        makeObjectString(src.slice(start, std::optional<size_t>(slen)))));

    return Value::object(ret);
}

Value nativeJoin(int argc, Value* argv)
{
    // join(list, separator)  →  string
    if (argc < 2 || !argv) {
        diagnostic::emit("join() requires 2 arguments: join(list, separator)");
        return Value::nil();
    }
    if (!argv[0].isList()) {
        diagnostic::emit("join() first argument must be a list");
        return Value::nil();
    }
    if (!argv[1].isString()) {
        diagnostic::emit("join() separator must be a string");
        return Value::nil();
    }

    ObjList* list = argv[0].asList();
    StringRef const& sep = argv[1].asString()->str;
    uint32_t const sz = list->size();

    StringRef result("");
    for (uint32_t i = 0; i < sz; i++) {
        std::ostringstream oss;
        oss << list->elements[i];
        result += StringRef(oss.str().c_str());
        if (i + 1 < sz)
            result += sep;
    }

    return Value::object(makeObjectString(result));
}

Value nativeSubstr(int argc, Value* argv)
{
    // substr(string, start [, end])
    if (argc < 2 || !argv) {
        diagnostic::emit("substr() requires at least 2 arguments: substr(string, start [, end])");
        return Value::nil();
    }
    if (!argv[0].isString()) {
        diagnostic::emit("substr() first argument must be a string");
        return Value::nil();
    }
    if (!argv[1].isInteger()) {
        diagnostic::emit("substr() start must be an integer");
        return Value::nil();
    }

    StringRef const& s = argv[0].asString()->str;
    int64_t sz = static_cast<int64_t>(s.len());
    int64_t start = argv[1].asInteger();
    if (start < 0)
        start += sz;
    start = std::max<int64_t>(0, std::min(start, sz));

    std::optional<size_t> end_opt = std::nullopt;
    if (argc >= 3 && argv[2].isInteger()) {
        int64_t end = argv[2].asInteger();
        if (end < 0)
            end += sz;
        end = std::max<int64_t>(0, std::min(end, sz));
        end_opt = static_cast<size_t>(end);
    }

    StringRef result = s.substr(static_cast<size_t>(start), end_opt);
    return Value::object(makeObjectString(result));
}

Value nativeContains(int argc, Value* argv)
{
    // contains(string, substring)  or  contains(list, value)
    if (argc < 2 || !argv) {
        diagnostic::emit("contains() requires 2 arguments");
        return Value::nil();
    }

    if (argv[0].isString()) {
        if (!argv[1].isString()) {
            diagnostic::emit("contains() needle must be a string when haystack is a string");
            return Value::nil();
        }
        return Value::boolean(argv[0].asString()->str.find(argv[1].asString()->str));
    }

    if (argv[0].isList()) {
        ObjList* list = argv[0].asList();
        Value const& needle = argv[1];
        for (uint32_t i = 0; i < list->size(); i++)
            if (list->elements[i] == needle)
                return Value::boolean(true);
        return Value::boolean(false);
    }

    diagnostic::emit("contains() first argument must be a string or list");
    return Value::nil();
}

Value nativeTrim(int argc, Value* argv)
{
    // trim(string [, "leading"|"trailing"])
    if (argc < 1 || !argv) {
        diagnostic::emit("trim() requires at least 1 argument");
        return Value::nil();
    }
    if (!argv[0].isString()) {
        diagnostic::emit("trim() first argument must be a string");
        return Value::nil();
    }

    StringRef s = argv[0].asString()->str; // copy — trimWhitespace mutates in-place

    std::optional<bool> leading = std::nullopt; // nullopt trims both sides
    std::optional<bool> trailing = std::nullopt;

    if (argc >= 2 && argv[1].isString()) {
        StringRef const& side = argv[1].asString()->str;
        if (side == StringRef("leading")) {
            leading = true;
            trailing = false;
        } else if (side == StringRef("trailing")) {
            leading = false;
            trailing = true;
        }
        // anything else → trim both (default)
    }

    s.trimWhitespace(leading, trailing);
    return Value::object(makeObjectString(s));
}

// Math utilities

Value nativeFloor(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("floor() requires 1 argument");
        return Value::nil();
    }
    double v = toNum(argv[0]);
    if (std::isnan(v)) {
        diagnostic::emit("floor() argument must be numeric");
        return Value::nil();
    }
    if (argv[0].isInteger())
        return argv[0]; // already an integer
    return Value::integer(static_cast<int64_t>(std::floor(v)));
}

Value nativeCeil(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("ceil() requires 1 argument");
        return Value::nil();
    }
    double v = toNum(argv[0]);
    if (std::isnan(v)) {
        diagnostic::emit("ceil() argument must be numeric");
        return Value::nil();
    }
    if (argv[0].isInteger())
        return argv[0];
    return Value::integer(static_cast<int64_t>(std::ceil(v)));
}

Value nativeRound(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("round() requires 1 argument");
        return Value::nil();
    }
    double v = toNum(argv[0]);
    if (std::isnan(v)) {
        diagnostic::emit("round() argument must be numeric");
        return Value::nil();
    }
    if (argv[0].isInteger())
        return argv[0];
    return Value::integer(static_cast<int64_t>(std::round(v)));
}

Value nativeAbs(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("abs() requires 1 argument");
        return Value::nil();
    }
    if (argv[0].isInteger())
        return Value::integer(std::abs(argv[0].asInteger()));
    double v = toNum(argv[0]);
    if (std::isnan(v)) {
        diagnostic::emit("abs() argument must be numeric");
        return Value::nil();
    }
    return Value::real(std::fabs(v));
}

Value nativeMin(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("min() requires at least 1 argument");
        return Value::nil();
    }

    // min(list)  or  min(a, b, c, ...)
    Value* data = argv;
    int n = argc;
    if (argc == 1 && argv[0].isList()) {
        ObjList* list = argv[0].asList();
        if (list->size() == 0) {
            diagnostic::emit("min() called on empty list");
            return Value::nil();
        }
        data = list->elements.data();
        n = static_cast<int>(list->size());
    }

    Value best = data[0];
    double best_d = toNum(best);
    for (int i = 1; i < n; i++) {
        double d = toNum(data[i]);
        if (d < best_d) {
            best = data[i];
            best_d = d;
        }
    }
    return best;
}

Value nativeMax(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("max() requires at least 1 argument");
        return Value::nil();
    }

    Value* data = argv;
    int n = argc;
    if (argc == 1 && argv[0].isList()) {
        ObjList* list = argv[0].asList();
        if (list->size() == 0) {
            diagnostic::emit("max() called on empty list");
            return Value::nil();
        }
        data = list->elements.data();
        n = static_cast<int>(list->size());
    }

    Value best = data[0];
    double best_d = toNum(best);
    for (int i = 1; i < n; i++) {
        double d = toNum(data[i]);
        if (d > best_d) {
            best = data[i];
            best_d = d;
        }
    }
    return best;
}

Value nativePow(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("pow() requires 2 arguments: pow(base, exponent)");
        return Value::nil();
    }
    double b = toNum(argv[0]);
    double e = toNum(argv[1]);
    if (std::isnan(b) || std::isnan(e)) {
        diagnostic::emit("pow() arguments must be numeric");
        return Value::nil();
    }
    double result = std::pow(b, e);
    // return integer when both args are integers and result is exact
    if (argv[0].isInteger() && argv[1].isInteger() && e >= 0.0
        && result == std::floor(result) && !std::isinf(result))
        return Value::integer(static_cast<int64_t>(result));
    return Value::real(result);
}

Value nativeSqrt(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("sqrt() requires 1 argument");
        return Value::nil();
    }
    double v = toNum(argv[0]);
    if (std::isnan(v)) {
        diagnostic::emit("sqrt() argument must be numeric");
        return Value::nil();
    }
    if (v < 0.0) {
        diagnostic::emit("sqrt() argument must be non-negative");
        return Value::nil();
    }
    return Value::real(std::sqrt(v));
}

// Control / meta

Value nativeAssert(int argc, Value* argv)
{
    // assert(condition [, message])
    if (argc < 1 || !argv) {
        diagnostic::emit("assert() requires at least 1 argument");
        return Value::nil();
    }

    if (!argv[0].isTruthy()) {
        if (argc >= 2 && argv[1].isString())
            diagnostic::emit("Assertion failed: " + std::string(argv[1].asString()->str.data(), argv[1].asString()->str.len()));
        else
            diagnostic::emit("Assertion failed");
    }
    return Value::nil();
}

Value nativeClock(int argc, Value* argv)
{
    return Value::real(static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
}

Value nativeTime(int argc, Value* argv)
{
    // Wall-clock seconds since Unix epoch (float)
    using namespace std::chrono;
    double secs = duration<double>(system_clock::now().time_since_epoch()).count();
    return Value::real(secs);
}

Value nativeError(int argc, Value* argv)
{
    // error(message)  — emit a runtime diagnostic
    if (argc >= 1 && argv[0].isString())
        diagnostic::emit(std::string(argv[0].asString()->str.data(), argv[0].asString()->str.len()));
    else
        diagnostic::emit("runtime error");
    return Value::nil();
}

} // namespace mylang::runtime
