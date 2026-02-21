#pragma once

#include "../../IR/value.hpp"
#include "const_pool.hpp"
#include "opcode.hpp"
#include <vector>

namespace mylang {
namespace runtime {

struct OpCodeChunk {
private:
    std::vector<uint8_t> codes;
    ConstPool constantPool;

public:
    static constexpr OpCodeChunk createEmpty()
    {
        return std::vector<uint8_t>();
    }

    OpCodeChunk()
    {
        // reserve 32 bytes aas default
        codes.reserve(32);
    }

    OpCodeChunk(std::vector<uint8_t> const c)
        : codes(c)
    {
    }

    OpCodeChunk(OpCodeChunk const&) = default;
    OpCodeChunk(OpCodeChunk&&) = default;

    OpCodeChunk& operator=(OpCodeChunk const&) = default;

    uint8_t const& operator[](size_t const at) const
    {
        return codes[at];
    }

    uint8_t operator[](size_t const at)
    {
        return codes[at];
    }

    void push(uint8_t const code)
    {
        codes.push_back(code);
    }

    void pop()
    {
        codes.pop_back();
    }

    size_t addConstant(IR::Value const value)
    {
        if (value.isInt()) {
            int64_t v = *value.asInt();
            return constantPool.push(v);
        } else if (value.isFloat()) {
            double v = *value.asFloat();
            return constantPool.push(v);
        }
    }
};

}
}
