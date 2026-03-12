#include "../../include/runtime/value.hpp"
#include <cstdint>

namespace mylang ::runtime {

bool Value::operator==(Value const& other) const { return Raw_ == other.Raw_; }
bool Value::operator!=(Value const& other) const { return Raw_ != other.Raw_; }

uint64_t Value::raw() const { return Raw_; }

Value Value::nan() { return Value { UINT64_C(0x7FF0000000000000) }; }
Value Value::nil() { return Value { NIL_VAL }; }
Value Value::boolean(bool const b) { return Value { b ? TRUE_VAL : FALSE_VAL }; }
Value Value::integer(int64_t const v)
{
    if (v >= -(INT64_C(1) << 47) && v < (INT64_C(1) << 47)) {
        uint64_t payload = static_cast<uint64_t>(v) & UINT64_C(0xFFFFFFFFFFFF);
        return Value { TAG_INTEGER | payload };
    }
    return Value::real(static_cast<double>(v));
}
Value Value::real(double d)
{
    uint64_t bits = UINT64_C(0);
    ::memcpy(&bits, &d, 8);
    // if nan, use canonical
    if ((bits & UINT64_C(0x7FF0000000000000)) == UINT64_C(0x7FF0000000000000) && (bits & UINT64_C(0x000FFFFFFFFFFFFF)) != 0)
        bits = NANBOX_QNAN;

    return Value { bits };
}
Value Value::object(ObjHeader* ptr)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    return Value { NANBOX_SIGN_BIT | NANBOX_QNAN | (static_cast<uint64_t>(p) & UINT64_C(0xFFFFFFFFFFFF)) };
}

bool Value::asBoolean() const
{
    if (!isBoolean())
        diagnostic::emit("asBoolean() called from a non boolean value", diagnostic::Severity::FATAL);
    return Raw_ == TRUE_VAL ? true : false;
}
int64_t Value::asInteger() const
{
    if (!isInteger())
        diagnostic::emit("asInteger() called on a non-integer value", diagnostic::Severity::FATAL);
    int64_t payload = static_cast<int64_t>(Raw_ & UINT64_C(0xFFFFFFFFFFFF));
    // sign-extend from bit 47
    if (payload & (INT64_C(1) << 47))
        payload |= ~UINT64_C(0xFFFFFFFFFFFF);
    return payload;
}
double Value::asDouble() const
{
    if (!isDouble())
        diagnostic::emit("asDouble() called from a non floating-point value", diagnostic::Severity::FATAL);
    double d = 0.0;
    ::memcpy(&d, &Raw_, 8);
    return d;
}
double Value::asDoubleAny() const
{
    if (isDouble())
        return asDouble();
    if (isInteger())
        return static_cast<double>(asInteger());
    diagnostic::emit("asDoubleAny() called on non-numeric value", diagnostic::Severity::FATAL);
    return 0.0;
}
ObjHeader* Value::asObject() const
{
    if (!isObject())
        diagnostic::emit("asObject() called from a non object value", diagnostic::Severity::FATAL);
    return reinterpret_cast<ObjHeader*>(static_cast<uintptr_t>(Raw_ & UINT64_C(0xFFFFFFFFFFFF)));
}

bool Value::isNil() const { return Raw_ == NIL_VAL; }
bool Value::isBoolean() const { return (Raw_ | 1) == TRUE_VAL; }
bool Value::isDouble() const { return (Raw_ & NANBOX_QNAN) != NANBOX_QNAN || Raw_ == NANBOX_QNAN; }
bool Value::isInteger() const { return (Raw_ & ~UINT64_C(0xFFFFFFFFFFFF)) == TAG_INTEGER; }
bool Value::isObject() const { return (Raw_ & (NANBOX_SIGN_BIT | NANBOX_QNAN)) == (NANBOX_SIGN_BIT | NANBOX_QNAN); }
bool Value::isNumber() const { return isInteger() || isDouble(); }

ObjString* Value::asString() const { return static_cast<ObjString*>(asObject()); }
ObjList* Value::asList() const { return static_cast<ObjList*>(asObject()); }
ObjFunction* Value::asFunction() const { return static_cast<ObjFunction*>(asObject()); }
ObjClosure* Value::asClosure() const { return static_cast<ObjClosure*>(asObject()); }
ObjNative* Value::asNative() const { return static_cast<ObjNative*>(asObject()); }

bool Value::isString() const { return isObject() && asObject()->type == ObjType::STRING; }
bool Value::isList() const { return isObject() && asObject()->type == ObjType::LIST; }
bool Value::isFunction() const { return isObject() && asObject()->type == ObjType::FUNCTION; }
bool Value::isClosure() const { return isObject() && asObject()->type == ObjType::CLOSURE; }
bool Value::isNative() const { return isObject() && asObject()->type == ObjType::NATIVE; }
bool Value::isTruthy() const
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

} // namespace mylang::runtime
