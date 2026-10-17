#ifndef FAIRUZ_COMPILER_REGISTER_ALLOCATOR_HPP
#define FAIRUZ_COMPILER_REGISTER_ALLOCATOR_HPP

#include "Compiler/opcode.hpp"
#include "Macros.hpp"
#include "Support/error.hpp"

namespace fairuz::runtime {

using RegType = u8;

class RegisterAllocator {
private:
    u8 m_next_reg { 0 };
    u8 m_max_reg { 0 };

public:
    RegisterAllocator() = default;

    u8 next_reg() const { return m_next_reg; }
    u8 max_reg() const { return m_max_reg; }

    ErrorOr<RegType> alloc_register()
    {
        if (m_next_reg >= MAX_REGS)
            return Error { ErrorCode::TOO_MANY_REGISTERS, { } };
        RegType reg = m_next_reg++;
        m_max_reg = m_next_reg > m_max_reg ? m_next_reg : m_max_reg;
        return reg;
    }

    void free_register()
    {
        if (m_next_reg > 0)
            m_next_reg--;
    }

    void rewind(RegType reg)
    {
        assert(reg <= m_next_reg);
        m_next_reg = reg;
    }

    ErrorOr<void> reserve_register(RegType reg)
    {
        if (reg >= MAX_REGS)
            return Error { ErrorCode::TOO_MANY_REGISTERS, { } };

        if (reg >= m_next_reg) {
            m_next_reg = reg + 1;
            if (m_next_reg > m_max_reg)
                m_max_reg = m_next_reg;
        }

        return { };
    }
};

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_REGISTER_ALLOCATOR_HPP
