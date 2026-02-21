#pragma once

#include <cstdint>

namespace mylang {
namespace runtime {

enum class OpCode : uint8_t {
    OP_CONST,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV
};

}
}
