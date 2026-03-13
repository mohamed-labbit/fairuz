#ifndef VALUE_HPP
#define VALUE_HPP

#include <bit>
#include <cassert>
#include <cstdint>
#include <cstring>

#include "../array.hpp"
#include "../string.hpp"

namespace mylang::runtime {

struct Chunk;
struct ObjHeader;
struct ObjString;
struct ObjList;
struct ObjFunction;
struct ObjUpValue;
struct ObjClosure;
struct ObjNative;

// nan-boxed values
class Value {
private:
    uint64_t Raw_;

public:
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000000
    static constexpr uint64_t NANBOX_QNAN = UINT64_C(0x7FF8000000000000);
    /// 10000000 0000000 00000000 00000000 00000000 00000000 00000000 00000000
    static constexpr uint64_t NANBOX_SIGN_BIT = UINT64_C(0x8000000000000000);
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000001
    static constexpr uint64_t TAG_NIL = UINT64_C(1); // 0001
    static constexpr uint64_t NIL_VAL = NANBOX_QNAN | TAG_NIL;
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000011
    static constexpr uint64_t TRUE_VAL = NANBOX_QNAN | UINT64_C(3);
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000010
    static constexpr uint64_t FALSE_VAL = NANBOX_QNAN | UINT64_C(2);
    /// 01111111 11111001 00000000 00000000 00000000 00000000 00000000 00000000
    static constexpr uint64_t TAG_INTEGER = NANBOX_QNAN | UINT64_C(0x0001000000000000);

    Value()
        : Raw_(NIL_VAL)
    {
    }

    explicit Value(uint64_t raw)
        : Raw_(raw)
    {
    }

    bool operator==(Value const& other) const;
    bool operator!=(Value const& other) const;

    uint64_t raw() const;

    static Value nan(); // loud nan
    static Value nil();
    static Value boolean(bool const b);
    static Value integer(int64_t const v);
    static Value real(double d);
    static Value object(ObjHeader* ptr);

    bool isNil() const;
    bool isBoolean() const;
    bool isInteger() const;
    bool isDouble() const;
    bool isObject() const;
    bool isNumber() const;

    // if the raw value does not comply with the requested type then it return nil
    // the caller should account for that
    bool asBoolean() const;
    int64_t asInteger() const;
    double asDouble() const;
    double asDoubleAny() const;
    ObjHeader* asObject() const;

    ObjString* asString() const;
    ObjList* asList() const;
    ObjFunction* asFunction() const;
    ObjClosure* asClosure() const;
    ObjNative* asNative() const;

    bool isString() const;
    bool isList() const;
    bool isFunction() const;
    bool isClosure() const;
    bool isNative() const;

    bool isTruthy() const;

    ~Value() = default;
};

enum class ObjType : uint8_t {
    STRING,
    LIST,
    FUNCTION,
    CLOSURE,
    NATIVE,
    UPVALUE
};

struct ObjHeader {
    ObjType type { ObjType::UPVALUE };
    bool isMarked { false };

    ObjHeader* next { nullptr };

    explicit ObjHeader(ObjType t)
        : type(t)
    {
    }
};

struct ObjString : ObjHeader {
    StringRef str;
    uint64_t hash;

    explicit ObjString(StringRef s)
        : ObjHeader(ObjType::STRING)
        , str(s)
    {
        hash = static_cast<uint64_t>(std::hash<StringRef> { }(str));
    }
};

struct ObjList : ObjHeader {
    Array<Value> elements;

    ObjList()
        : ObjHeader(ObjType::LIST)
    {
    }

    void reserve(uint32_t const cap)
    {
        elements.reserve(cap);
    }

    uint32_t size() const
    {
        return elements.size();
    }
};

struct ObjFunction : ObjHeader {
    unsigned int arity { 0 };
    unsigned int upvalueCount { 0 };

    Chunk* chunk { nullptr };
    ObjString* name { nullptr };

    explicit ObjFunction(Chunk* ch = nullptr)
        : ObjHeader(ObjType::FUNCTION)
        , chunk(ch)
    {
    }
};

struct ObjUpvalue : ObjHeader {
    Value* location { nullptr };
    Value closed;
    ObjUpvalue* nextOpen;

    explicit ObjUpvalue(Value* slot)
        : ObjHeader(ObjType::UPVALUE)
        , location(slot)
        , nextOpen(nullptr)
    {
    }
};

struct ObjClosure : ObjHeader {
    ObjFunction* function { nullptr };
    Array<ObjUpvalue*> upValues;

    explicit ObjClosure(ObjFunction* fn)
        : ObjHeader(ObjType::CLOSURE)
        , function(fn)
    {
        upValues = Array<ObjUpvalue*>(fn->upvalueCount, nullptr);
    }
};

using NativeFn = Value (*)(int argc, Value* argv);

struct ObjNative : ObjHeader {
    NativeFn fn;
    ObjString* name { nullptr };
    int arity;

    ObjNative(NativeFn f, ObjString* n, int a)
        : ObjHeader(ObjType::NATIVE)
        , fn(f)
        , name(n)
        , arity(a)
    {
    }
};

static ObjHeader* makeObject(ObjType t)
{
    return getRuntimeAllocator().allocateObject<ObjHeader>(t);
}

static ObjString* makeObjectString(StringRef s)
{
    return getRuntimeAllocator().allocateObject<ObjString>(s);
}

static ObjList* makeObjectList()
{
    return getRuntimeAllocator().allocateObject<ObjList>();
}

static ObjFunction* makeObjectFunction(Chunk* ch = nullptr)
{
    return getRuntimeAllocator().allocateObject<ObjFunction>(ch);
}

static ObjUpvalue* makeObjectUpvalue(Value* slot)
{
    return getRuntimeAllocator().allocateObject<ObjUpvalue>(slot);
}

static ObjClosure* makeObjectClosure(ObjFunction* fn)
{
    return getRuntimeAllocator().allocateObject<ObjClosure>(fn);
}

static ObjNative* makeObjectNative(NativeFn f, ObjString* n, int a)
{
    return getRuntimeAllocator().allocateObject<ObjNative>(f, n, a);
}

inline std::ostream& operator<<(std::ostream& os, Value v)
{
    if (v.isNil())
        return { os << "nil" };
    if (v.isBoolean())
        return { os << (v.asBoolean() ? "true" : "false") };
    if (v.isInteger())
        return { os << v.asInteger() };
    if (v.isDouble())
        return { os << v.asDouble() };

    if (v.isObject()) {
        ObjHeader* obj = v.asObject();

        switch (obj->type) {
        case ObjType::STRING: {
            auto* s = static_cast<ObjString*>(obj);
            os << s->str;
        } break;

        case ObjType::LIST: {
            auto* list = static_cast<ObjList*>(obj);
            os << "[";
            for (size_t i = 0; i < list->elements.size(); ++i) {
                os << list->elements[i];
                if (i + 1 < list->elements.size())
                    os << ", ";
            }
            os << "]";
        } break;

        case ObjType::FUNCTION: {
            auto* fn = static_cast<ObjFunction*>(obj);
            if (fn->name)
                os << "<fn " << fn->name->str << ">";
            else
                os << "<fn anonymous>";
        } break;

        case ObjType::CLOSURE: {
            auto* closure = static_cast<ObjClosure*>(obj);
            if (closure->function->name)
                os << "<closure " << closure->function->name->str << ">";
            else
                os << "<closure anonymous>";
        } break;

        case ObjType::NATIVE: {
            auto* native = static_cast<ObjNative*>(obj);
            os << "<native fn " << native->name->str << ">";
        } break;

        case ObjType::UPVALUE:
            os << "<upvalue>";
            break;

        default:
            os << "<unknown object>";
            break;
        }

        return os;
    }

    // Should never happen
    os << "<invalid>";
    return os;
}

}

#endif // VALUE_HPP
