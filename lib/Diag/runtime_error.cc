//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Fairuz runtime error implementation.
//===----------------------------------------------------------------------===//

#include "Diag/runtime_error.hpp"
#include "Compiler/chunk.hpp"
#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include "Macros.hpp"
#include <cassert>
#include <string>

namespace fairuz::runtime {

/// Constructors

FrameTrace::FrameTrace(CallFrame const& frame)
{
    if (frame.func != nullptr) function_name = std::string(frame.func->name().data(), frame.func->name().len());
    else if (frame.chunk != nullptr) function_name = std::string(frame.chunk->name.data(), frame.chunk->name.len());
    if (frame.module != nullptr) module_name = std::string(frame.module->name.data(), frame.module->name.len());
    if (frame.chunk != nullptr) {
        source = frame.chunk->source;
        size_t off = frame.ip.value > 0 ? frame.ip.value - 1 : 0;
        if (off < frame.chunk->locations.size()) loc = frame.chunk->locations[off];
    }
}

RuntimeError::RuntimeError(ErrorCode errc, SourcePtr source, SourceLocation loc, std::string detail)
    : kind(error_category_for(errc))
    , errc(errc)
    , source(std::move(source))
    , loc(loc)
    , detail(detail)
{
    // Native host calls and setup failures need not have a source location.
}

void RuntimeError::add_trace(CallFrame const& frame) { traceback.push(frame); }

std::string RuntimeError::str() const
{
    ErrorRenderer renderer;
    return renderer.error_string(*this);
}

}; // fairuz::runtime
