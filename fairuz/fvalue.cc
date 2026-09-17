#include "fvalue.hpp"
#include "fobject.hpp"

namespace fairuz::runtime {

#if FA_USE_NANBOX
ObjString* Value::as_string() const { return obj_cast<ObjString>(as_obj(), ObjType::STRING); }
ObjList* Value::as_list() const { return obj_cast<ObjList>(as_obj(), ObjType::LIST); }
ObjDict* Value::as_dict() const { return obj_cast<ObjDict>(as_obj(), ObjType::DICT); }
ObjFunction* Value::as_func() const { return obj_cast<ObjFunction>(as_obj(), ObjType::FUNCTION); }
ObjNative* Value::as_native() const { return obj_cast<ObjNative>(as_obj(), ObjType::NATIVE); }
ObjClass* Value::as_class() const { return obj_cast<ObjClass>(as_obj(), ObjType::CLASS); }
ObjInstance* Value::as_instance() const { return obj_cast<ObjInstance>(as_obj(), ObjType::INSTANCE); }
ObjFileHandle* Value::as_file_handle() const { return obj_cast<ObjFileHandle>(as_obj(), ObjType::FILE_HANDLE); }
ObjModule* Value::as_module() const { return obj_cast<ObjModule>(as_obj(), ObjType::MODULE); }

#else

ObjString* Value::as_string() const { return obj_cast<ObjString>(as_obj(), ObjType::STRING); }
ObjList* Value::as_list() const { return obj_cast<ObjList>(as_obj(), ObjType::LIST); }
ObjDict* Value::as_dict() const { return obj_cast<ObjDict>(as_obj(), ObjType::DICT); }
ObjFunction* Value::as_func() const { return obj_cast<ObjFunction>(as_obj(), ObjType::FUNCTION); }
ObjNative* Value::as_native() const { return obj_cast<ObjNative>(as_obj(), ObjType::NATIVE); }
ObjClass* Value::as_class() const { return obj_cast<ObjClass>(as_obj(), ObjType::CLASS); }
ObjInstance* Value::as_instance() const { return obj_cast<ObjInstance>(as_obj(), ObjType::INSTANCE); }
ObjFileHandle* Value::as_file_handle() const { return obj_cast<ObjFileHandle>(as_obj(), ObjType::FILE_HANDLE); }
ObjModule* Value::as_module() const { return obj_cast<ObjModule>(as_obj(), ObjType::MODULE); }

#endif

size_t ValueHash::operator()(Value const& v) const noexcept
{
    switch (value_type_tag(v)) {
    case TypeTag::NONE: return 0;
    case TypeTag::NIL: return 0;
    case TypeTag::BOOL: return std::hash<bool> { }(v.as_bool());
    case TypeTag::INT: return std::hash<f64> { }(static_cast<f64>(v.as_int()));
    case TypeTag::DOUBLE: return std::hash<f64> { }(v.as_double());
    case TypeTag::STRING: return v.as_string()->hash;
    default: return std::hash<void*> { }(v.as_obj());
    }
}

bool ValueEqual::operator()(Value const& lhs, Value const& rhs) const noexcept
{
    if (lhs.is_string() && rhs.is_string())
        return lhs.as_string()->str == rhs.as_string()->str;
    if (lhs.is_number() && rhs.is_number())
        return lhs.as_double_any() == rhs.as_double_any();
    return lhs == rhs;
}

} // namespace fairuz::runtime
