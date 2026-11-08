//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements the CallRegisters class methods.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Macros.hpp"

namespace fairuz::runtime {

void CallRegisters::finish_callee() { m_allocator.rewind(m_base.value + 1); }

ErrorOr<RegType> CallRegisters::next_arg()
{
    // m_allocator.next_reg should point immediately to the
    // next slot reserved for the next argument since 'finish_arg'
    // will free scratch regs for us
    return m_allocator.alloc_register();
}

void CallRegisters::finish_arg(RegType arg) { m_allocator.rewind(arg.value + 1); }

void CallRegisters::finish_terminal() { m_allocator.rewind(m_base.value); }

RegType CallRegisters::finish_with_result()
{
    m_allocator.rewind(m_base.value + 1);
    return m_base;
}

ErrorOr<RegType> CallRegisters::prepare_auxiliary()
{
    assert(m_call_layout == CallLayout::INVOKE);
    assert(!m_auxiliary_prepared);
    assert(m_allocator.next_reg() == static_cast<u16>(m_base.value) + 1);

    auto auxiliary = m_allocator.alloc_register();
    if (auxiliary.has_error()) return auxiliary.error();

    m_auxiliary_prepared = true;
    return auxiliary.value();
}

} // namespace fairuz::runtime
