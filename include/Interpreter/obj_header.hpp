//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines common heap-object metadata.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_INTERPRETER_OBJECT_HEADER_HPP
#define FAIRUZ_INTERPRETER_OBJECT_HEADER_HPP

#include "Macros.hpp"

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

#endif // FAIRUZ_INTERPRETER_OBJECT_HEADER_HPP
