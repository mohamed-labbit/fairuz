#include "../../../include/runtime/compiler/value.hpp"

namespace mylang {
namespace runtime {

Value Value::nil()
{
    return Value { NIL_VAL };
}

Value Value::boolean(bool b)
{
    return Value { b ? TRUE_VAL : FALSE_VAL };
}

Value Value::integer(int64_t v)
{
    // Store the lower 48 bits; sign-extend on read.
    // For values outside ±2^47 we fall back to double.
    if (v >= -(INT64_C(1) << 47) && v < (INT64_C(1) << 47)) {
        uint64_t payload = static_cast<uint64_t>(v) & UINT64_C(0xFFFFFFFFFFFF);

        return Value { NANBOX_QNAN | TAG_INT | (payload << 3) };
    }

    return Value::real(static_cast<double>(v));
}

Value Value::real(double d)
{
    uint64_t bits;
    ::memcpy(&bits, &d, 8);

    // If NaN → force canonical
    if ((bits & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (bits & UINT64_C(0x000FFFFFFFFFFFFF)) != 0)
        bits = CANONICAL_NAN;

    return Value { bits };
}

Value Value::object(ObjHeader* ptr)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);

    return Value {
        NANBOX_SIGN_BIT | NANBOX_QNAN | (static_cast<uint64_t>(p) & UINT64_C(0xFFFFFFFFFFFF))
    };
}

// ---- type queries ----
bool Value::isNil() const
{
    return raw == NIL_VAL;
}

bool Value::isBool() const
{
    return (raw & (NANBOX_QNAN | TAG_BOOL | TAG_INT)) == (NANBOX_QNAN | TAG_BOOL);
}

bool Value::isInt() const
{
    return (raw & (NANBOX_QNAN | TAG_INT)) == (NANBOX_QNAN | TAG_INT) && !isObj();
}

bool Value::isDouble() const
{
    return (raw & NANBOX_QNAN) != NANBOX_QNAN || raw == CANONICAL_NAN;
}

bool Value::isObj() const
{
    return (raw & (NANBOX_SIGN_BIT | NANBOX_QNAN)) == (NANBOX_SIGN_BIT | NANBOX_QNAN);
}

bool Value::isNumber() const
{
    return isInt() || isDouble();
}

// ---- extractors ----
bool Value::asBool() const
{
    assert(isBool());
    return (raw >> 2) & 1;
}

int64_t Value::asInt() const
{
    assert(isInt());
    // Extract 48-bit payload from bits 3..50, sign-extend.
    int64_t payload = static_cast<int64_t>((raw >> 3) & UINT64_C(0xFFFFFFFFFFFF));
    // Sign-extend from bit 47
    if (payload & (INT64_C(1) << 47))
        payload |= ~UINT64_C(0xFFFFFFFFFFFF);

    return payload;
}

double Value::asDouble() const
{
    assert(isDouble());
    double d;
    ::memcpy(&d, &raw, 8);
    return d;
}

double Value::asNumberDouble() const
{
    if (isInt())
        return static_cast<double>(asInt());

    return asDouble();
}

ObjHeader* Value::asObj() const
{
    assert(isObj());
    return reinterpret_cast<ObjHeader*>(static_cast<uintptr_t>(raw & UINT64_C(0xFFFFFFFFFFFF)));
}

// ---- truthiness (falsy: nil, false, 0, 0.0) ----
bool Value::isTruthy() const
{
    if (isNil())
        return false;
    if (isBool())
        return asBool();
    if (isInt())
        return asInt() != 0;
    if (isDouble())
        return asDouble() != 0.0;

    return true; // objects are truthy
}

// Defined in value.cpp
ObjString* Value::asString() const
{
    return static_cast<ObjString*>(asObj());
}

ObjList* Value::asList() const
{
    return static_cast<ObjList*>(asObj());
}

ObjFunction* Value::asFunction() const
{
    return static_cast<ObjFunction*>(asObj());
}

ObjClosure* Value::asClosure() const
{
    return static_cast<ObjClosure*>(asObj());
}

ObjNative* Value::asNative() const
{
    return static_cast<ObjNative*>(asObj());
}

bool Value::isString() const
{
    return isObj() && asObj()->type == ObjType::STRING;
}

bool Value::isList() const
{
    return isObj() && asObj()->type == ObjType::LIST;
}

bool Value::isFunction() const
{
    return isObj() && asObj()->type == ObjType::FUNCTION;
}

bool Value::isClosure() const
{
    return isObj() && asObj()->type == ObjType::CLOSURE;
}

bool Value::isNative() const
{
    return isObj() && asObj()->type == ObjType::NATIVE;
}

// Pretty-printer (defined in value.cpp)
inline std::ostream& operator<<(std::ostream& os, Value v)
{
    using namespace mylang::runtime;

    if (v.isNil()) {
        os << "nil";
        return os;
    }

    if (v.isBool()) {
        os << (v.asBool() ? "true" : "false");
        return os;
    }

    if (v.isInt()) {
        os << v.asInt();
        return os;
    }

    if (v.isDouble()) {
        os << v.asDouble();
        return os;
    }

    if (v.isObj()) {
        ObjHeader* obj = v.asObj();

        switch (obj->type) {
        case ObjType::STRING: {
            ObjString* s = static_cast<ObjString*>(obj);
            os << s->chars;
            break;
        }

        case ObjType::LIST: {
            ObjList* list = static_cast<ObjList*>(obj);
            os << "[";
            for (size_t i = 0; i < list->elements.size(); ++i) {
                os << list->elements[i];
                if (i + 1 < list->elements.size())
                    os << ", ";
            }
            os << "]";
            break;
        }

        case ObjType::FUNCTION: {
            ObjFunction* fn = static_cast<ObjFunction*>(obj);
            if (fn->name)
                os << "<fn " << fn->name->chars << ">";
            else
                os << "<fn anonymous>";
            break;
        }

        case ObjType::CLOSURE: {
            ObjClosure* closure = static_cast<ObjClosure*>(obj);
            if (closure->function->name)
                os << "<closure " << closure->function->name->chars << ">";
            else
                os << "<closure anonymous>";
            break;
        }

        case ObjType::NATIVE: {
            ObjNative* native = static_cast<ObjNative*>(obj);
            os << "<native fn " << native->name->chars << ">";
            break;
        }

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
}
