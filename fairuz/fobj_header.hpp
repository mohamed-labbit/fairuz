#ifndef FA_FOBJ_HEADER_HPP
#define FA_FOBJ_HEADER_HPP

#include "fmacros.hpp"

namespace fairuz::runtime {

enum class ObjType : u8 {
    INT,
    STRING,
    LIST,
    DICT,
    FUNCTION,
    NATIVE,
    CLASS,
    INSTANCE,
    FILE_HANDLE,
    MODULE,
    _COUNT,
};

struct ObjHeader {
    ObjType type { ObjType::STRING };
    bool is_marked { false };
    ObjHeader* next { nullptr };

    ObjHeader() = default;
    explicit ObjHeader(ObjType t)
        : type(t)
    {
    }
};

} // namespace fairuz::runtime

#endif // FA_FOBJ_HEADER_HPP
