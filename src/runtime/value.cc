#include "../../include/runtime/value.hpp"

namespace mylang ::runtime {

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

} // namespace mylang::runtime
