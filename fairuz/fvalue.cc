#include "fvalue.hpp"
#include "fobject.hpp"

namespace fairuz::runtime {

#if !FA_USE_NANBOX
size_t Fa_ValueHash::operator()(Fa_Value const& v) const
{
    switch (value_type_tag(v)) {
    case Fa_TypeTag::NONE: return 0;
    case Fa_TypeTag::NIL: return 0;
    case Fa_TypeTag::BOOL: return std::hash<bool> { }(v.as_bool());
    case Fa_TypeTag::INT: return std::hash<i64> { }(v.as_int());
    case Fa_TypeTag::DOUBLE: return std::hash<f64> { }(v.as_double());
    case Fa_TypeTag::STRING: return Fa_as_string(v)->hash;
    default: return std::hash<void*> { }(v.as_object());
    }
}
#endif

} // namespace fairuz::runtime
