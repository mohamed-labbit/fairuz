#ifndef FA_FOBJ_HEADER_HPP
#define FA_FOBJ_HEADER_HPP

#include "fmacros.hpp"

namespace fairuz::runtime {

enum class Fa_ObjType : u8 {
    INT,
    STRING,
    LIST,
    DICT,
    FUNCTION,
    NATIVE,
    CLASS,
    INSTANCE,
    FILE_HANDLE,
    _COUNT,
};

struct Fa_ObjHeader {
    Fa_ObjType type { Fa_ObjType::STRING };
    bool is_marked { false };
    Fa_ObjHeader* next { nullptr };

    Fa_ObjHeader() = default;
    explicit Fa_ObjHeader(Fa_ObjType t)
        : type(t)
    {
    }
};

} // namespace fairuz::runtime

#endif // FA_FOBJ_HEADER_HPP
