/// stdlib.cc

#include "../include/vm.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace mylang::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;

static StringRef valueToString(Value v)
{
    if (IS_NIL(v))
        return "nil";
    if (IS_BOOL(v))
        return AS_BOOL(v) ? "true" : "false";
    if (IS_INTEGER(v))
        return StringRef(std::to_string(AS_INTEGER(v)).c_str());
    if (IS_DOUBLE(v)) {
        std::ostringstream oss;
        oss << std::setprecision(14) << std::noshowpoint << AS_DOUBLE(v);
        return StringRef(oss.str().c_str());
    }
    if (IS_STRING(v))
        return AS_STRING(v)->str;
    if (IS_LIST(v))
        return "<list>";
    if (IS_CLOSURE(v))
        return "<function>";
    if (IS_NATIVE(v))
        return "<native>";
    if (IS_FUNCTION(v))
        return "<function>";
    return "";
}

Value VM::nativeLen(int argc, Value* argv)
{
    if (argc == 0 || !argv)
        return NIL_VAL;

    if (argc == 1) {
        if (IS_STRING(argv[0]))
            return MAKE_INTEGER(AS_STRING(argv[0])->str.len());
        if (IS_LIST(argv[0]))
            return MAKE_INTEGER(AS_LIST(argv[0])->elements.size());
    }

    /// do not accept multiple args for len
    return NIL_VAL;
}

static void printValue(Value v, int depth = 0)
{
    if (IS_NIL(v)) {
        std::cout << "nil";
        return;
    }

    if (IS_BOOL(v)) {
        std::cout << (AS_BOOL(v) ? "true" : "false");
        return;
    }

    if (IS_INTEGER(v)) {
        std::cout << AS_INTEGER(v);
        return;
    }

    if (IS_OBJECT(v)) {
        ObjHeader* obj = AS_OBJECT(v);

        switch (obj->type) {
        case ObjType::STRING:
            std::cout << static_cast<ObjString*>(obj)->str;
            return;

        case ObjType::LIST: {
            ObjList* list = static_cast<ObjList*>(obj);
            std::cout << '[';
            for (uint32_t i = 0; i < list->elements.size(); ++i) {
                if (i > 0)
                    std::cout << ", ";
                Value elem = list->elements[i];
                if (IS_OBJECT(elem) && AS_OBJECT(elem)->type == ObjType::STRING) {
                    std::cout << '"';
                    std::cout << AS_STRING((elem))->str;
                    std::cout << '"';
                } else {
                    printValue(elem, depth + 1);
                }
            }
            std::cout << ']';
            return;
        }

        case ObjType::CLOSURE: {
            ObjClosure* cl = static_cast<ObjClosure*>(obj);
            std::cout << "<function ";
            if (cl->function && cl->function->name)
                std::cout << cl->function->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case ObjType::NATIVE: {
            ObjNative* nat = static_cast<ObjNative*>(obj);
            std::cout << "<native ";
            if (nat->name)
                std::cout << nat->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case ObjType::FUNCTION: {
            ObjFunction* fn = static_cast<ObjFunction*>(obj);
            std::cout << "<function ";
            if (fn->name)
                std::cout << fn->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case ObjType::UPVALUE:
            std::cout << "<upvalue>";
            return;
        }
    }

    double d = AS_DOUBLE(v);
    if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15) {
        std::cout << static_cast<int64_t>(d);
    } else {
        std::ostringstream oss;
        oss << std::setprecision(14) << std::noshowpoint << d;
        std::cout << oss.str();
    }
}

Value VM::nativePrint(int argc, Value* argv)
{
    if (argc == 0 || !argv) {
        std::cout << '\n';
        return NIL_VAL;
    }

    for (int i = 0; i < argc; ++i) {
        if (i > 0)
            std::cout << '\t';
        printValue(argv[i]);
    }
    std::cout << '\n';
    return NIL_VAL;
}

Value VM::nativeType(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    return MAKE_INTEGER(static_cast<int64_t>(valueTypeTag(argv[0])));
}

Value VM::nativeInt(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;
    if (IS_NUMBER(argv[0]))
        return MAKE_INTEGER(static_cast<int64_t>(AS_DOUBLE_ANY(argv[0])));
    return NIL_VAL;
}

Value VM::nativeFloat(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    if (IS_NUMBER(argv[0]))
        return MAKE_REAL(AS_DOUBLE_ANY(argv[0]));

    return NIL_VAL;
}

Value VM::nativeAppend(int argc, Value* argv)
{
    if (argc < 2 || !argv) {
        diagnostic::emit("append() : expected two arguments got : " + std::to_string(argc));
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    Value& list_v = argv[0];
    if (!IS_LIST(list_v)) {
        diagnostic::emit("append() called on a non list value");
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
        return NIL_VAL;
    }

    ObjList* list_obj = AS_LIST(list_v);

    for (int i = 1; i < argc; ++i)
        list_obj->elements.push(argv[i]);

    return NIL_VAL;
}

Value VM::nativePop(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("pop() called with no value attatched");
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
        return NIL_VAL;
    }

    Value& list_v = argv[0];
    if (!IS_LIST(list_v)) {
        diagnostic::emit("pop() called on a non list value");
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_CALL);
        return NIL_VAL;
    }

    AS_LIST(list_v)->elements.pop();
    return NIL_VAL;
}

Value VM::nativeSlice(int argc, Value* argv)
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

    ObjList* list_obj = AS_LIST(argv[0]);
    Value ret = MAKE_LIST();
    ObjList* ret_list = AS_LIST(ret);
    /// Expects indices to be ints
    uint32_t a = AS_INTEGER(argv[1]);
    uint32_t b = argc == 3 ? AS_INTEGER(argv[2]) : list_obj->size() - 1;

    for (int i = a; i <= b; ++i)
        ret_list->elements.push(list_obj->elements[i]);

    return ret;
}

Value VM::nativeInput(int argc, Value* argv) { return NIL_VAL; }

Value VM::nativeStr(int argc, Value* argv)
{
    if (argc > 1) {
        diagnostic::emit("str() expects one argument , got : " + std::to_string(argc));
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    StringRef output = "";

    if (argc == 0 || !argv)
        return MAKE_OBJECT((MAKE_OBJ_STRING(output))); // return empty on no arg

    /// TODO : stringify a list

    if (IS_STRING(argv[0]))
        output = AS_STRING(argv[0])->str;
    else if (IS_BOOL(argv[0]))
        output = AS_BOOL(argv[0]) ? "true" : "false";
    else if (IS_DOUBLE(argv[0]))
        output = std::to_string(AS_DOUBLE(argv[0])).data();
    else if (IS_INTEGER(argv[0]))
        output = std::to_string(AS_INTEGER(argv[0])).data();

    return MAKE_STRING(output);
}

Value VM::nativeBool(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("bool() is called with no arguments");
        diagnostic::runtimeError(ErrorCode::WRONG_ARG_COUNT);
        return NIL_VAL;
    }

    return IS_TRUTHY(argv[0]) ? MAKE_BOOL(true) : MAKE_BOOL(false);
}

Value VM::nativeList(int argc, Value* argv)
{
    Value ret = MAKE_LIST();
    ObjList* list_obj = AS_LIST(ret);

    for (int i = 0; i < argc; ++i)
        list_obj->elements.push(argv[i]);

    return ret;
}

Value VM::nativeSplit(int argc, Value* argv)
{
    if (argc != 2 || !argv)
        return NIL_VAL;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1]))
        return NIL_VAL;

    StringRef src = AS_STRING(argv[0])->str;
    StringRef delim = AS_STRING(argv[1])->str;

    Value ret = MAKE_LIST();
    ObjList* list = AS_LIST(ret);

    if (delim.empty()) {
        list->elements.push(MAKE_STRING(src));
        return ret;
    }

    size_t start = 0;
    while (start <= src.len()) {
        size_t pos = start;
        bool found = false;
        while (pos + delim.len() <= src.len()) {
            if (::memcmp(src.data() + pos, delim.data(), delim.len()) == 0) {
                found = true;
                break;
            }
            ++pos;
        }

        if (!found) {
            list->elements.push(MAKE_STRING(src.substr(start, src.len())));
            break;
        }

        list->elements.push(MAKE_STRING(src.substr(start, pos)));
        start = pos + delim.len();
    }

    return ret;
}

Value VM::nativeJoin(int argc, Value* argv)
{
    if (argc != 2 || !argv)
        return NIL_VAL;
    if (!IS_LIST(argv[0]) || !IS_STRING(argv[1]))
        return NIL_VAL;

    ObjList* list = AS_LIST(argv[0]);
    StringRef delim = AS_STRING(argv[1])->str;
    StringRef out = "";

    for (uint32_t i = 0; i < list->elements.size(); ++i) {
        if (i > 0)
            out += delim;
        out += valueToString(list->elements[i]);
    }

    return MAKE_STRING(out);
}

Value VM::nativeSubstr(int argc, Value* argv)
{
    if (argc != 3 || !argv) {
        diagnostic::emit("substr() expects 3 arguments, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    StringRef str = AS_STRING(argv[0])->str;
    Value a = AS_INTEGER(argv[1]);
    Value b = AS_INTEGER(argv[2]);
    /// TODO: check a, b types

    StringRef ret = str.substr(a, b);
    return MAKE_STRING(ret);
}

Value VM::nativeContains(int argc, Value* argv)
{
    if (argc != 2 || !argv)
        return NIL_VAL;
    if (!IS_STRING(argv[0]) || !IS_STRING(argv[1]))
        return NIL_VAL;

    return MAKE_BOOL(AS_STRING(argv[0])->str.find(AS_STRING(argv[1])->str));
}

Value VM::nativeTrim(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;
    if (!IS_STRING(argv[0]))
        return NIL_VAL;

    StringRef out = AS_STRING(argv[0])->str;
    out.trimWhitespace();
    return MAKE_STRING(out.substr(0, out.len()));
}

Value VM::nativeFloor(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("floor() expects 1 argument, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!IS_NUMBER(argv[0])) {
        diagnostic::emit("floor() is called with a non numeric value argument");
        return NIL_VAL;
    }

    if (IS_INTEGER(argv[0]))
        return argv[0];

    return MAKE_REAL(std::floor(AS_DOUBLE(argv[0])));
}

Value VM::nativeCeil(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("ceil() expects 1 argument, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!IS_NUMBER(argv[0])) {
        diagnostic::emit("ceil() is called with a non numeric value argument");
        return NIL_VAL;
    }

    if (IS_INTEGER(argv[0]))
        return argv[0];

    return MAKE_REAL(std::ceil(AS_DOUBLE_ANY(argv[0])));
}

Value VM::nativeRound(int argc, Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;
    if (!IS_NUMBER(argv[0]))
        return NIL_VAL;
    if (IS_INTEGER(argv[0]))
        return argv[0];

    return MAKE_REAL(std::round(AS_DOUBLE_ANY(argv[0])));
}

Value VM::nativeAbs(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("abs() expects 1 argument, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    if (!IS_NUMBER(argv[0])) {
        diagnostic::emit("abs() is called with a non numeric value argument");
        return NIL_VAL;
    }

    if (IS_INTEGER(argv[0])) {
        int64_t v = AS_INTEGER(argv[0]);
        if (v == INT64_MIN) {
            diagnostic::emit("abs() argument out of range");
            return NIL_VAL;
        }
        return MAKE_INTEGER(std::abs(AS_INTEGER(argv[0])));
    }
    return MAKE_REAL(std::fabs(AS_DOUBLE(argv[0])));
}

Value VM::nativeMin(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("min() expects 1 or more arguments, got: " + std::to_string(argc));
        return NIL_VAL;
    }

    // Determine mode from argv[0]
    bool all_ints = IS_INTEGER(argv[0]);
    bool all_strs = IS_STRING(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; ++i) {
        if (!IS_INTEGER(argv[i]))
            all_ints = false;
        if (!IS_STRING(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Value ret = argv[0];
        for (int i = 1; i < argc; ++i) {
            if (AS_STRING(argv[i])->str < AS_STRING(ret)->str)
                ret = argv[i];
        }
        return ret;
    }

    Value ret = MAKE_REAL(AS_DOUBLE_ANY(argv[0]));
    for (int i = 1; i < argc; ++i)
        ret = MAKE_REAL(std::fmin(AS_DOUBLE_ANY(ret), AS_DOUBLE_ANY(argv[i])));

    if (all_ints)
        return MAKE_INTEGER(static_cast<int64_t>(AS_DOUBLE_ANY(ret)));
    return ret;
}

Value VM::nativeMax(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit("max() expects 1 or more arguments, got: " + std::to_string(argc));
        return NIL_VAL;
    }

    // Determine mode from argv[0]
    bool all_ints = IS_INTEGER(argv[0]);
    bool all_strs = IS_STRING(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; ++i) {
        if (!IS_INTEGER(argv[i]))
            all_ints = false;
        if (!IS_STRING(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Value ret = argv[0];
        for (int i = 1; i < argc; ++i) {
            if (AS_STRING(argv[i])->str > AS_STRING(ret)->str)
                ret = argv[i];
        }
        return ret;
    }

    Value ret = MAKE_REAL(AS_DOUBLE_ANY(argv[0]));
    for (int i = 1; i < argc; ++i)
        ret = MAKE_REAL(std::fmax(AS_DOUBLE_ANY(ret), AS_DOUBLE_ANY(argv[i])));

    if (all_ints)
        return MAKE_INTEGER(static_cast<int64_t>(AS_DOUBLE_ANY(ret)));
    return ret;
}

Value VM::nativePow(int argc, Value* argv)
{
    if (argc != 2 || !argv) {
        diagnostic::emit("pow() expects 2 arguments, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    Value base = argv[0];
    Value exponent = argv[1];

    if (UNLIKELY(!IS_NUMBER(base) || !IS_NUMBER(exponent))) {
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        return NIL_VAL;
    }

    if (IS_INTEGER(base) && IS_INTEGER(exponent))
        // return an int even if the result may be larger than 48 bit range
        return MAKE_INTEGER(std::pow(AS_INTEGER(base), AS_INTEGER(exponent)));
    else
        return MAKE_REAL(std::pow(AS_DOUBLE_ANY(base), AS_DOUBLE_ANY(exponent)));

    return NIL_VAL;
}

Value VM::nativeSqrt(int argc, Value* argv)
{
    if (argc != 1 || !argv) {
        diagnostic::emit("sqrt() expects 1 argument, got : " + std::to_string(argc));
        return NIL_VAL;
    }

    Value n = argv[0];

    if (UNLIKELY(!IS_NUMBER(n))) {
        diagnostic::runtimeError(ErrorCode::TYPE_ERROR_ARITH);
        return NIL_VAL;
    }

    double val = AS_DOUBLE_ANY(n);
    if (val < 0.0)
        return NIL_VAL;

    return MAKE_REAL(std::sqrt(val));
}

Value VM::nativeAssert(int argc, Value* argv) { return NIL_VAL; }
Value VM::nativeClock(int argc, Value* argv) { return NIL_VAL; }
Value VM::nativeError(int argc, Value* argv) { return NIL_VAL; }
Value VM::nativeTime(int argc, Value* argv) { return NIL_VAL; }

} // namespace mylang::runtime
