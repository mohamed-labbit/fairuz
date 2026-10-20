#ifndef FAIRUZ_COMPILER_REGISTER_ALLOCATOR_HPP
#define FAIRUZ_COMPILER_REGISTER_ALLOCATOR_HPP

#include "Compiler/opcode.hpp"
#include "Macros.hpp"
#include "Support/error.hpp"

#include <cassert>

namespace fairuz::runtime {

class RegisterAllocator {
private:
    // These are counts/checkpoints, including the one-past-last position,
    // rather than register identities. Never do cursor arithmetic in u8.
    u16 m_next_reg { 0 };
    u16 m_max_reg { 0 };

public:
    RegisterAllocator() = default;

    u16 next_reg() const { return m_next_reg; }
    u16 max_reg() const { return m_max_reg; }

    ErrorOr<RegType> alloc_register()
    {
        if (m_next_reg >= MAX_REGS) return Error { ErrorCode::TOO_MANY_REGISTERS, { } };
        RegType reg { static_cast<u8>(m_next_reg) };
        ++m_next_reg;
        m_max_reg = m_next_reg > m_max_reg ? m_next_reg : m_max_reg;
        return reg;
    }

    void free_register()
    {
        if (m_next_reg > 0) m_next_reg--;
    }

    void rewind(u16 checkpoint)
    {
        assert(checkpoint <= m_next_reg);
        m_next_reg = checkpoint;
    }

    ErrorOr<void> reserve_register(RegType reg)
    {
        if (reg.value >= MAX_REGS) return Error { ErrorCode::TOO_MANY_REGISTERS, { } };

        if (reg.value >= m_next_reg) {
            m_next_reg = static_cast<u16>(reg.value) + 1;
            if (m_next_reg > m_max_reg) m_max_reg = m_next_reg;
        }

        return { };
    }
};

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_REGISTER_ALLOCATOR_HPP
