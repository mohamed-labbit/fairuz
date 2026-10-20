//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements Fairuz runtime error handler
//===----------------------------------------------------------------------===//

#include "Diag/runtime_error.hpp"
#include "Interpreter/vm.hpp"
#include <cstdlib>

namespace fairuz::runtime {

void VM::halt()
{
    unwind();
    ::exit(1);
}

void VM::unwind()
{
    // A partially initialized module is useful only while breaking an active
    // import cycle. It must never masquerade as a successful import on retry.
    for (auto it = m_module_cache.begin(); it != m_module_cache.end();) {
        if (!it->second->initialized) {
            it->second->executing = false;
            it = m_module_cache.erase(it);
        } else ++it;
    }
    std::fill(m_stack, m_stack + std::clamp(m_stack_top, 0, STACK_SIZE), Value::nil());
    std::fill(m_frames, m_frames + std::clamp(m_frames_top, 0, MAX_FRAMES), CallFrame());
    m_frames_top = 0;
    m_stack_top = 0;
}

void VM::print(RuntimeError const& err)
{
    ErrorRenderer renderer;
    renderer.print_runtime_error(err);
}

} // fairuz::runtime
