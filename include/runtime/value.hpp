#ifndef VALUE_HPP
#define VALUE_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

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
    static constexpr uint64_t TAG_INT = UINT64_C(3);  // 0011
    static constexpr uint64_t TAG_BOOL = UINT64_C(2); // 0010
    static constexpr uint64_t TAG_NIL = UINT64_C(1);  // 0001
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000001
    static constexpr uint64_t NIL_VAL = NANBOX_QNAN | TAG_NIL;
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000110
    static constexpr uint64_t TRUE_VAL = NANBOX_QNAN | TAG_BOOL | (UINT64_C(1) << 2);
    /// 01111111 11111000 00000000 00000000 00000000 00000000 00000000 00000010
    static constexpr uint64_t FALSE_VAL = NANBOX_QNAN | TAG_BOOL;

    Value()
        : Raw_(NIL_VAL)
    {
    }

    explicit Value(uint64_t raw)
        : Raw_(raw)
    {
    }

    bool operator==(Value const& other) const { return Raw_ == other.Raw_; }
    bool operator!=(Value const& other) const { return Raw_ != other.Raw_; }

    uint64_t raw() const { return Raw_; }

    static Value nil() { return Value { NIL_VAL }; }
    static Value boolean(bool const b) { return Value { b ? TRUE_VAL : FALSE_VAL }; }
    static Value integer(int64_t const v)
    {
        /// store lower 48 bits, for larger values we use double, get sign on read
        if (v >= -(INT64_C(1) << 47) && v < (INT64_C(1) << 47)) {
            uint64_t payload = static_cast<uint64_t>(v) & UINT64_C(0xFFFFFFFFFFFF);
            return Value { NANBOX_QNAN | TAG_INT | (payload << 3) };
        }
        return real(static_cast<double>(v));
    }
    static Value real(double d)
    {
        uint64_t bits = UINT64_C(0);
        ::memcpy(&bits, &d, 8);
        // if nan, use canonical
        if ((bits & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (bits & UINT64_C(0x000FFFFFFFFFFFFF)) != 0)
            bits = NANBOX_QNAN;

        return Value { bits };
    }
    static Value object(ObjHeader* ptr)
    {
        uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
        return Value { NANBOX_SIGN_BIT | NANBOX_QNAN | (static_cast<uint64_t>(p) & UINT64_C(0xFFFFFFFFFFFF)) };
    }

    bool isNil() const { return Raw_ == NIL_VAL; }
    bool isBoolean() const { return Raw_ == TRUE_VAL || Raw_ == FALSE_VAL; }
    bool isInteger() const { return (Raw_ & (NANBOX_SIGN_BIT | NANBOX_QNAN | TAG_INT)) == (NANBOX_QNAN | TAG_INT); }
    bool isDouble() const { return (Raw_ & NANBOX_QNAN) != NANBOX_QNAN || Raw_ == NANBOX_QNAN; }
    bool isNumber() const { return isInteger() || isDouble(); }
    bool isObject() const { return (Raw_ & (NANBOX_SIGN_BIT | NANBOX_QNAN)) == (NANBOX_SIGN_BIT | NANBOX_QNAN); }

    // if the raw value does not comply with the requested type then it return nil
    // the caller should account for that
    bool asBoolean() const
    {
        assert(isBoolean());
        return (Raw_ >> 2) & 1;
    }
    int64_t asInteger() const
    {
        assert(isInteger());
        int64_t payload = static_cast<int64_t>((Raw_ >> 3) & UINT64_C(0xFFFFFFFFFFFF));
        if (payload & (INT64_C(1) << 47))
            payload |= ~UINT64_C(0xFFFFFFFFFFFF);
        return payload;
    }
    double asDouble() const
    {
        assert(isDouble());
        double d = 0.0;
        ::memcpy(&d, &Raw_, 8);
        return d;
    }
    // creates a double from both integer and double
    double asDoubleAny() const
    {
        assert(isNumber());
        if (isInteger())
            return static_cast<double>(asInteger());
        return asDouble();
    }
    ObjHeader* asObject() const
    {
        assert(isObject());
        return reinterpret_cast<ObjHeader*>(static_cast<uintptr_t>(Raw_ & UINT64_C(0xFFFFFFFFFFFF)));
    }

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

    bool isTruthy() const
    {
        if (isNil())
            return false;
        if (isBoolean())
            return asBoolean();
        if (isInteger())
            return asInteger() != 0;
        if (isDouble())
            return asDouble() != 0.0;
        return true;
    }

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
    std::vector<Value> elements;

    ObjList()
        : ObjHeader(ObjType::LIST)
    {
    }

    void reserve(size_t const s) { elements.reserve(s); }
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
    Value* location;
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
    ObjFunction* function;
    std::vector<ObjUpvalue*> upValues;

    explicit ObjClosure(ObjFunction* fn)
        : ObjHeader(ObjType::CLOSURE)
        , function(fn)
    {
        upValues.resize(fn->upvalueCount, nullptr);
    }
};

using NativeFn = Value (*)(int argc, Value* argv);

struct ObjNative : ObjHeader {
    NativeFn fn;
    ObjString* name;
    int arity;

    ObjNative(NativeFn f, ObjString* n, int a)
        : ObjHeader(ObjType::NATIVE)
        , fn(f)
        , name(n)
        , arity(a)
    {
    }
};

inline std::ostream& operator<<(std::ostream& os, Value v)
{
    using namespace mylang::runtime;

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
