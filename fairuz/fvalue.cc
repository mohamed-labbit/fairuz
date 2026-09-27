#include "fvalue.hpp"
#include "fgc.hpp"
#include "finteger.hpp"
#include "fobject.hpp"

namespace fairuz::runtime {

#if FA_USE_NANBOX

Value Value::from_int(i64 const v, GarbageCollector& gc)
{
    if (fits_in_int48(v))
        return from_int(v);
    return gc.make_int(v);
}

#endif

i64 Value::as_int() const
{
    if (is_big_int()) {
        i64 result;
        if (!integer::to_i64(*this, result))
            diagnostic::fatal_error(ErrorCode::NUMERIC_OUT_OF_RANGE, "integer does not fit the native API's signed 64-bit parameter");
        return result;
    }
#if FA_USE_NANBOX
    i64 payload = static_cast<i64>(m_value & PAYLOAD_MASK);
    if (payload & (INT64_C(1) << 47))
        return payload - (INT64_C(1) << 48);
    return payload;
#else
    return as.i;
#endif
}

f64 Value::as_double_any() const { return is_int() ? integer::to_double(*this) : as_double(); }

bool Value::is_truthy() const
{
    if (is_nil())
        return false;
    if (is_bool())
        return as_bool();
    if (is_big_int())
        return !as_big_int()->limbs.empty();
    if (is_int())
        return as_int() != 0;
    if (is_obj())
        return true;
    return as_double() != 0;
}

ObjString* Value::as_string() const { return reinterpret_cast<ObjString*>(as_obj()); }
ObjList* Value::as_list() const { return reinterpret_cast<ObjList*>(as_obj()); }
ObjDict* Value::as_dict() const { return reinterpret_cast<ObjDict*>(as_obj()); }
ObjFunction* Value::as_func() const { return reinterpret_cast<ObjFunction*>(as_obj()); }
ObjNative* Value::as_native() const { return reinterpret_cast<ObjNative*>(as_obj()); }
ObjClass* Value::as_class() const { return reinterpret_cast<ObjClass*>(as_obj()); }
ObjInstance* Value::as_instance() const { return reinterpret_cast<ObjInstance*>(as_obj()); }
ObjFileHandle* Value::as_file_handle() const { return reinterpret_cast<ObjFileHandle*>(as_obj()); }
ObjModule* Value::as_module() const { return reinterpret_cast<ObjModule*>(as_obj()); }
ObjBigInt* Value::as_big_int() const { return reinterpret_cast<ObjBigInt*>(as_obj()); }

size_t ValueHash::operator()(Value const& v) const
{
    switch (value_type_tag(v)) {
    case TypeTag::NONE: return 0;
    case TypeTag::NIL: return 0;
    case TypeTag::BOOL: return std::hash<bool> { }(v.as_bool());
    case TypeTag::INT: return std::hash<f64> { }(v.as_double_any());
    case TypeTag::DOUBLE: return std::hash<f64> { }(v.as_double());
    case TypeTag::STRING: return v.as_string()->hash;
    default: return std::hash<void*> { }(v.as_obj());
    }
}

bool ValueEqual::operator()(Value const& lhs, Value const& rhs) const
{
    if (lhs.is_nil() && rhs.is_nil())
        return true;
    if (lhs.is_string() && rhs.is_string())
        return lhs.as_string()->str == rhs.as_string()->str;
    if (lhs.is_double() && rhs.is_double())
        return lhs.as_double() == rhs.as_double();
    if (lhs.is_number() && rhs.is_number())
        return integer::compare_numbers(lhs, rhs) == 0;
    if (lhs.is_bool() && rhs.is_bool())
        return lhs.as_bool() == rhs.as_bool();
    if (lhs.is_obj() && rhs.is_obj())
        return lhs.as_obj() == rhs.as_obj();
    return false;
}

} // namespace fairuz::runtime
