#include "fvalue.hpp"
#include "fobject.hpp"

namespace fairuz::runtime {

#if FA_USE_NANBOX
Fa_ObjString* Fa_Value::as_string() const { return Fa_obj_cast<Fa_ObjString>(as_obj(), Fa_ObjType::STRING); }
Fa_ObjList* Fa_Value::as_list() const { return Fa_obj_cast<Fa_ObjList>(as_obj(), Fa_ObjType::LIST); }
Fa_ObjDict* Fa_Value::as_dict() const { return Fa_obj_cast<Fa_ObjDict>(as_obj(), Fa_ObjType::DICT); }
Fa_ObjFunction* Fa_Value::as_func() const { return Fa_obj_cast<Fa_ObjFunction>(as_obj(), Fa_ObjType::FUNCTION); }
Fa_ObjNative* Fa_Value::as_native() const { return Fa_obj_cast<Fa_ObjNative>(as_obj(), Fa_ObjType::NATIVE); }
Fa_ObjClass* Fa_Value::as_class() const { return Fa_obj_cast<Fa_ObjClass>(as_obj(), Fa_ObjType::CLASS); }
Fa_ObjInstance* Fa_Value::as_instance() const { return Fa_obj_cast<Fa_ObjInstance>(as_obj(), Fa_ObjType::INSTANCE); }
Fa_ObjFileHandle* Fa_Value::as_file_handle() const { return Fa_obj_cast<Fa_ObjFileHandle>(as_obj(), Fa_ObjType::FILE_HANDLE); }

#else

Fa_ObjString* Fa_Value::as_string() const { return Fa_obj_cast<Fa_ObjString>(as_obj(), Fa_ObjType::STRING); }
Fa_ObjList* Fa_Value::as_list() const { return Fa_obj_cast<Fa_ObjList>(as_obj(), Fa_ObjType::LIST); }
Fa_ObjDict* Fa_Value::as_dict() const { return Fa_obj_cast<Fa_ObjDict>(as_obj(), Fa_ObjType::DICT); }
Fa_ObjFunction* Fa_Value::as_func() const { return Fa_obj_cast<Fa_ObjFunction>(as_obj(), Fa_ObjType::FUNCTION); }
Fa_ObjNative* Fa_Value::as_native() const { return Fa_obj_cast<Fa_ObjNative>(as_obj(), Fa_ObjType::NATIVE); }
Fa_ObjClass* Fa_Value::as_class() const { return Fa_obj_cast<Fa_ObjClass>(as_obj(), Fa_ObjType::CLASS); }
Fa_ObjInstance* Fa_Value::as_instance() const { return Fa_obj_cast<Fa_ObjInstance>(as_obj(), Fa_ObjType::INSTANCE); }
Fa_ObjFileHandle* Fa_Value::as_file_handle() const { return Fa_obj_cast<Fa_ObjFileHandle>(as_obj(), Fa_ObjType::FILE_HANDLE); }

size_t Fa_ValueHash::operator()(Fa_Value const& v) const
{
    switch (value_type_tag(v)) {
    case Fa_TypeTag::NONE: return 0;
    case Fa_TypeTag::NIL: return 0;
    case Fa_TypeTag::BOOL: return std::hash<bool> { }(v.as_bool());
    case Fa_TypeTag::INT: return std::hash<i64> { }(v.as_int());
    case Fa_TypeTag::DOUBLE: return std::hash<f64> { }(v.as_double());
    case Fa_TypeTag::STRING: return v.as_string()->hash;
    default: return std::hash<void*> { }(v.as_obj());
    }
}

#endif

} // namespace fairuz::runtime
