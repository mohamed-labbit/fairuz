#include "../../../include/runtime/vm/vm.hpp"

namespace mylang {
namespace runtime {

void VM::open_stdlib()
{
    // print(...) — print all args space-separated with a newline
    register_native("print", [](int argc, Value* argv) -> Value {
        for (int i = 0; i < argc; ++i) {
            if (i > 0)  std::cout << ' ';
            Value v = argv[i];
            if (v.isNil())          std::cout << "nil";
            else if (v.isBool())    std::cout << (v.asBool() ? "true" : "false");
            else if (v.isInt())     std::cout << v.asInt();
            else if (v.isDouble())  std::cout << v.asDouble();
            else if (v.isString())  std::cout << v.asString()->chars;
            else if (v.isList()) {
                std::cout << "[";
                auto& elems = v.asList()->elements;
                for (size_t j = 0; j < elems.size(); ++j) {
                    if (j > 0)  std::cout << ", ";
                    Value e = elems[j];
                    if (e.isNil())         std::cout << "nil";
                    else if (e.isBool())   std::cout << (e.asBool() ? "true" : "false");
                    else if (e.isInt())    std::cout << e.asInt();
                    else if (e.isDouble()) std::cout << e.asDouble();
                    else if (e.isString()) std::cout << e.asString()->chars;
                    else                   std::cout << "<obj>";
                }
                std::cout << "]";
            } else std::cout << "<obj>";
        }
        std::cout << '\n';
        return Value::nil(); }, /*arity=*/-1);

    // type(v) → string name of the type
    register_native("type", [](int argc, Value* argv) -> Value {
        if (argc < 1)  return Value::nil();
        Value v = argv[0];
        // We can't call intern() here (no VM*), so we return a raw ObjString.
        // Callers that need the string object should use the longer form.
        // For stdlib simplicity we return a string by re-registering after intern.
        // We store the name as a compile-time constant string in the closure instead.
        // (See below — registered as a lambda that uses a static map.)
        if (v.isNil())     { static ObjString s("nil");      return Value::object(&s); }
        if (v.isBool())    { static ObjString s("bool");     return Value::object(&s); }
        if (v.isInt())     { static ObjString s("int");      return Value::object(&s); }
        if (v.isDouble())  { static ObjString s("float");    return Value::object(&s); }
        if (v.isString())  { static ObjString s("string");   return Value::object(&s); }
        if (v.isList())    { static ObjString s("list");     return Value::object(&s); }
        if (v.isClosure()) { static ObjString s("function"); return Value::object(&s); }
        if (v.isNative())  { static ObjString s("function"); return Value::object(&s); }
        static ObjString s("unknown"); return Value::object(&s); }, 1);

    // len(list) → integer length
    register_native("len", [](int argc, Value* argv) -> Value {
        if (argc < 1)           { return Value::integer(0); }
        if (argv[0].isList())   { return Value::integer(static_cast<int64_t>(argv[0].asList()->elements.size())); }
        if (argv[0].isString()) { return Value::integer(static_cast<int64_t>(argv[0].asString()->chars.len())); }
        throw VMError("len() argument must be a list or string"); }, 1);

    // append(list, value) — mutates and returns list
    register_native("append", [](int argc, Value* argv) -> Value {
        if (argc < 2 || !argv[0].isList()) throw VMError("append(list, value) — first argument must be a list");
        argv[0].asList()->elements.push_back(argv[1]);
        return argv[0]; }, 2);

    // pop(list) — removes and returns last element
    register_native("pop", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].isList()) throw VMError("pop(list) — argument must be a list");
        auto& elems = argv[0].asList()->elements;
        if (elems.empty()) throw VMError("pop() on empty list");
        Value v = elems.back();
        elems.pop_back();
        return v; }, 1);

    // str(v) → string representation (returns ObjString*)
    register_native("str", [](int argc, Value* argv) -> Value {
        if (argc < 1) {  static ObjString s(""); return Value::object(&s); }
        Value v = argv[0];
        StringRef result;
        if (v.isNil())         result = "nil";
        else if (v.isBool())   result = v.asBool() ? "true" : "false";
        else if (v.isInt())    result = std::to_string(v.asInt()).data();
        else if (v.isDouble()) result = std::to_string(v.asDouble()).data();
        else if (v.isString()) return v; // already a string
        else                   result = "<obj>";
        // Allocate on heap — GC will track it
        ObjString* s = new ObjString(result);
        return Value::object(s); }, 1);

    // int(v) → integer truncation
    register_native("int", [](int argc, Value* argv) -> Value {
        if (argc < 1) return Value::integer(0);
        Value v = argv[0];
        if (v.isInt())    return v;
        if (v.isDouble()) return Value::integer(static_cast<int64_t>(v.asDouble()));
        if (v.isBool())   return Value::integer(v.asBool() ? 1 : 0);
        if (v.isString()) {
            try { return Value::integer(v.asString()->chars.toDouble()); }
            catch (...) { throw VMError("int() cannot convert string to integer"); }
        }
        throw VMError("int() cannot convert this type"); }, 1);

    // float(v) → double conversion
    register_native("float", [](int argc, Value* argv) -> Value {
        if (argc < 1) return Value::real(0.0);
        Value v = argv[0];
        if (v.isDouble()) return v;
        if (v.isInt())    return Value::real(static_cast<double>(v.asInt()));
        if (v.isBool())   return Value::real(v.asBool() ? 1.0 : 0.0);
        if (v.isString()) {
            try { return Value::real(v.asString()->chars.toDouble()); }
            catch (...) { throw VMError("float() cannot convert string"); }
        }
        throw VMError("float() cannot convert this type"); }, 1);

    // assert(cond, msg?) — throw if cond is falsy
    register_native("assert", [](int argc, Value* argv) -> Value {
        if (argc < 1 || !argv[0].isTruthy()) {
            std::string msg = "assertion failed";
            if (argc >= 2 && argv[1].isString()) msg = argv[1].asString()->chars.data();
            throw VMError(msg);
        }
        return argv[0]; }, -1);

    // clock() → elapsed seconds as float (uses std::clock)
    register_native("clock", [](int /*argc*/, Value* /*argv*/) -> Value { return Value::real(static_cast<double>(::clock()) / CLOCKS_PER_SEC); }, 0);
}

}
}
