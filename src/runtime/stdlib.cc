#include "../../include/runtime/stdlib.hpp"
#include "../../include/runtime/opcode.hpp"
#include <cmath>
#include <cstdint>
#include <string>

namespace mylang::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

Value nativeLen(int argc, Value* argv)
{
    if (argc == 0 || !argv)
        return NIL_VAL;

    if (argc == 1) {
        if (isString(argv[0]))
            return makeInteger(asString(argv[0])->str.len());
        if (isList(argv[0]))
            return makeInteger(asList(argv[0])->elements.size());
    }

    /// do not accept multiple arguments for len
    return NIL_VAL;
}

Value nativePrint(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        std::cout << std::endl;
        return NIL_VAL;
    }

    StringRef output = asString(nativeStr(1, &argv[0]))->str;

    std::cout << output << std::endl;
    return NIL_VAL;
}

Value nativeType(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    return makeInteger(static_cast<int64_t>(valueTypeTag(argv[0])));
}

Value nativeInt(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    if (isNumber(argv[0]))
        return makeInteger(asDouble(static_cast<int64_t>(argv[0])));

    return NIL_VAL;
}

Value nativeFloat(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    if (isNumber(argv[0]))
        return makeReal(asDouble(argv[0]));

    return NIL_VAL;
}

Value nativeAppend(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("append() : expected two arguments got : " + std::to_string(argc));
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    Value& list_v = argv[0];
    if (!isList(list_v)) {
        diagnostic::emit("append() called on a non list value");
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
        return NIL_VAL;
    }

    ObjList* list_obj = asList(list_v);

    for (int i = 1; i < argc; ++i)
        list_obj->elements.push(argv[i]);

    return NIL_VAL;
}

Value nativePop(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("pop() called with no value attatched");
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
        return NIL_VAL;
    }

    Value& list_v = argv[0];
    if (!isList(list_v)) {
        diagnostic::emit("pop() called on a non list value");
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
        return NIL_VAL;
    }

    asList(list_v)->elements.pop();
    return NIL_VAL;
}

Value nativeSlice(int argc, Value* argv)
{
    /// cut a copy of a list, with inclusive indices
    /// accept [list, a, b]
    /// a, b are the indices
    /// if b is null then cut [a:]

    if (argc < 2) {
        diagnostic::emit("slice() expects at least 2 arguments, got : " + std::to_string(argc));
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    ObjList* list_obj = asList(argv[0]);
    Value ret = makeObject(makeObjectList());
    ObjList* ret_list = asList(ret);
    /// Expects indices to be ints
    uint32_t a = asInteger(argv[1]);
    uint32_t b = argc == 3 ? asInteger(argv[2]) : list_obj->size() - 1;

    for (int i = a; i <= b; ++i)
        ret_list->elements.push(list_obj->elements[i]);

    return ret;
}

Value nativeInput(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeStr(int argc, Value* argv)
{
    if (argc > 1) {
        diagnostic::emit("str() is called with no arguments");
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    StringRef output = "";

    if (argc == 0 || !argv)
        return makeObject((makeObjectString(output)));

    /// TODO : stringify a list

    if (isString(argv[0]))
        output = asString(argv[0])->str;
    else if (IS_BOOL(argv[0]))
        output = asBool(argv[0]) ? "true" : "false";
    else if (isDouble(argv[0]))
        output = std::to_string(asDouble(argv[0])).data();
    else if (IS_INTEGER(argv[0]))
        output = std::to_string(asInteger(argv[0])).data();

    return makeObject(makeObjectString(output));
}

Value nativeBool(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("bool() is called with no arguments");
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    return isTruthy(argv[0]) ? makeBool(true) : makeBool(false);
}

Value nativeList(int argc, Value* argv)
{
    if (argc < 2) {
        diagnostic::emit("list() expects at least 2 arguments, got : " + std::to_string(argc));
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    Value ret = makeObject(makeObjectList());
    ObjList* list_obj = asList(ret);

    for (int i = 0; i < argc;)
        list_obj->elements.push(argv[i++]);

    return ret;
}

Value nativeSplit(int argc, Value* argv) { return NIL_VAL; }

Value nativeJoin(int argc, Value* argv) { return NIL_VAL; }

Value nativeSubstr(int argc, Value* argv) { return NIL_VAL; }

Value nativeContains(int argc, Value* argv) { return NIL_VAL; }

Value nativeTrim(int argc, Value* argv) { return NIL_VAL; }

Value nativeFloor(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("floor() expects 1 argument, got : " + std::to_string(argc));
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    if (!isNumber(argv[0])) {
        diagnostic::emit("floor() is called with a non numeric value argument");
        // diagnostic::runtimeError(ErrorCode::);
        /// diagnostic::runtimeError(ErrorCode::);
        return NIL_VAL;
    }

    if (IS_INTEGER(argv[0]))
        return argv[0];

    return makeReal(std::floorf(asDouble(argv[0])));
}

Value nativeCeil(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("ceil() expects 1 argument, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!isNumber(argv[0])) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return NIL_VAL;
    }

    if (IS_INTEGER(argv[0]))
        return argv[0];

    return makeReal(std::ceilf(asDouble(argv[0])));
}

Value nativeRound(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeAbs(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("abs() expects 1 argument, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!isNumber(argv[0])) {
        diagnostic::emit("abs() is called with a non numeric value argument");
        return NIL_VAL;
    }

    if (IS_INTEGER(argv[0]))
        return makeInteger(std::abs(asInteger(argv[0])));

    return makeReal(std::fabs(asDouble(argv[0])));
}

Value nativeMin(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("ceil() expects 2 or more arguments, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!isNumber(argv[0])) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return NIL_VAL;
    }

    Value ret = makeReal(asDouble(argv[0]));
    bool all_ints = true;

    for (int i = 1; i < argc; ++i) {
        if (!IS_INTEGER(argv[i]))
            all_ints = false;

        ret = makeReal(std::fmin(asDouble(ret), asDouble(argv[i])));
    }

    if (all_ints)
        return makeInteger(static_cast<int64_t>(asDouble(ret)));

    return ret;
}

Value nativeMax(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("ceil() expects 2 or more arguments, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!isNumber(argv[0])) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return NIL_VAL;
    }

    Value ret = makeReal(asDouble(argv[0]));
    bool all_ints = true;

    for (int i = 1; i < argc; ++i) {
        if (!IS_INTEGER(argv[i]))
            all_ints = false;

        ret = makeReal(std::fmax(asDouble(ret), asDouble(argv[i])));
    }

    if (all_ints)
        return makeInteger(static_cast<int64_t>(asDouble(ret)));

    return ret;
}

Value nativePow(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeSqrt(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeAssert(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeClock(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeError(int argc, Value* argv)
{
    return NIL_VAL;
}

Value nativeTime(int argc, Value* argv)
{
    return NIL_VAL;
}

} // namespace mylang::runtime
