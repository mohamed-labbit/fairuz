#include "../../include/runtime/stdlib.hpp"
#include "../../include/runtime/opcode.hpp"
#include <cmath>
#include <cstdint>
#include <string>

namespace mylang::runtime {

Value nativeLen(int argc, Value* argv)
{
    if (argc == 0 || !argv)
        return Value::nil();

    if (argc == 1) {
        if (argv[0].isString())
            return Value::integer(argv[0].asString()->str.len());
        if (argv[0].isList())
            return Value::integer(argv[0].asList()->elements.size());
    }

    /// do not accept multiple arguments for len
    return Value::nil();
}

Value nativePrint(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        std::cout << std::endl;
        return Value::nil();
    }

    StringRef output = nativeStr(1, &argv[0]).asString()->str;

    std::cout << output << std::endl;
    return Value::nil();
}

Value nativeType(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return Value::nil();

    return Value::integer(static_cast<int64_t>(valueTypeTag(argv[0])));
}

Value nativeInt(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return Value::nil();

    if (argv[0].isNumber())
        return Value::integer(static_cast<int64_t>(argv[0].asDoubleAny()));

    return Value::nil();
}

Value nativeFloat(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return Value::nil();

    if (argv[0].isNumber())
        return Value::real(argv[0].asDoubleAny());

    return Value::nil();
}

Value nativeAppend(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("append() : expected two arguments got : " + std::to_string(argc));
        return Value::nil();
    }

    Value& list_v = argv[0];
    if (!list_v.isList()) {
        diagnostic::emit("append() called on a non list value");
        return Value::nil();
    }

    ObjList* list_obj = list_v.asList();

    for (int i = 1; i < argc; ++i)
        list_obj->elements.push(argv[i]);

    return Value::nil();
}

Value nativePop(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("pop() called with no value attatched");
        return Value::nil();
    }

    Value& list_v = argv[0];
    if (!list_v.isList()) {
        diagnostic::emit("pop() called on a non list value");
        return Value::nil();
    }

    list_v.asList()->elements.pop();
    return Value::nil();
}

Value nativeSlice(int argc, Value* argv)
{
    /// cut a copy of a list, with inclusive indices
    /// accept [list, a, b]
    /// a, b are the indices
    /// if b is null then cut [a:]

    if (argc < 2) {
        diagnostic::emit("slice() expects at least 2 arguments, got : " + std::to_string(argc));
        return Value::nil();
    }

    ObjList* list_obj = argv[0].asList();
    Value ret = Value::object(makeObjectList());
    ObjList* ret_list = ret.asList();
    /// Expects indices to be ints
    uint32_t a = argv[1].asInteger();
    uint32_t b = argc == 3 ? argv[2].asInteger() : list_obj->size() - 1;

    for (int i = a; i <= b; ++i)
        ret_list->elements.push(list_obj->elements[i]);

    return ret;
}

Value nativeInput(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeStr(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("str() is called with no arguments");
        return Value::nil();
    }

    StringRef output = "";

    /// TODO : stringify a list

    if (argv[0].isString())
        output = argv[0].asString()->str;
    else if (argv[0].isBoolean())
        output = argv[0].asBoolean() ? "true" : "false";
    else if (argv[0].isDouble())
        output = std::to_string(argv[0].asDouble()).data();
    else if (argv[0].isInteger())
        output = std::to_string(argv[0].asInteger()).data();

    return Value::object(makeObjectString(output));
}

Value nativeBool(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("bool() is called with no arguments");
        return Value::nil();
    }

    return argv[0].isTruthy() ? Value::boolean(true) : Value::boolean(false);
}

Value nativeList(int argc, Value* argv)
{
    if (argc < 2) {
        diagnostic::emit("list() expects at least 2 arguments, got : " + std::to_string(argc));
        return Value::nil();
    }

    Value ret = Value::object(makeObjectList());
    ObjList* list_obj = ret.asList();

    for (int i = 0; i < argc;)
        list_obj->elements.push(argv[i++]);

    return ret;
}

Value nativeSplit(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeJoin(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeSubstr(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeContains(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeTrim(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeFloor(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("floor() expects 1 argument, got : " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].isNumber()) {
        diagnostic::emit("floor() is called with a non numeric value argument");
        return Value::nil();
    }

    if (argv[0].isInteger())
        return argv[0];

    return Value::real(std::floorf(argv[0].asDouble()));
}

Value nativeCeil(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("ceil() expects 1 argument, got : " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].isNumber()) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return Value::nil();
    }

    if (argv[0].isInteger())
        return argv[0];

    return Value::real(std::ceilf(argv[0].asDouble()));
}

Value nativeRound(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeAbs(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("abs() expects 1 argument, got : " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].isNumber()) {
        diagnostic::emit("abs() is called with a non numeric value argument");
        return Value::nil();
    }

    if (argv[0].isInteger())
        return Value::integer(std::abs(argv[0].asInteger()));

    return Value::real(std::fabs(argv[0].asDouble()));
}

Value nativeMin(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("ceil() expects 2 or more arguments, got : " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].isNumber()) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return Value::nil();
    }

    Value ret = Value::real(argv[0].asDoubleAny());
    bool all_ints = true;

    for (int i = 1; i < argc; ++i) {
        if (!argv[i].isInteger())
            all_ints = false;

        ret = Value::real(std::fmin(ret.asDouble(), argv[i].asDoubleAny()));
    }

    if (all_ints)
        return Value::integer(static_cast<int64_t>(ret.asDouble()));

    return ret;
}

Value nativeMax(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("ceil() expects 2 or more arguments, got : " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].isNumber()) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return Value::nil();
    }

    Value ret = Value::real(argv[0].asDoubleAny());
    bool all_ints = true;

    for (int i = 1; i < argc; ++i) {
        if (!argv[i].isInteger())
            all_ints = false;

        ret = Value::real(std::fmax(ret.asDouble(), argv[i].asDoubleAny()));
    }

    if (all_ints)
        return Value::integer(static_cast<int64_t>(ret.asDouble()));

    return ret;
}

Value nativePow(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeSqrt(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeAssert(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeClock(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeError(int argc, Value* argv)
{
    return Value::nil();
}

Value nativeTime(int argc, Value* argv)
{
    return Value::nil();
}

} // namespace mylang::runtime
