//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements Virtual Machine runner.
//===----------------------------------------------------------------------===//

#include "VMRunner.hpp"
#include "Compiler/chunk.hpp"
#include "Compiler/compiler.hpp"
#include "Diag/diagnostic.hpp"
#include "Diag/runtime_error.hpp"
#include "File.hpp"
#include "Interpreter/vm.hpp"
#include "Parser/ast.hpp"
#include "Parser/parser.hpp"
#include "Support/arena.hpp"
#include "Support/array.hpp"

namespace fairuz::runtime {

ExitStatus run_protected(char const* filepath)
{
    // set allocator global context
    AllocatorContext alloc_ctx;
    set_context(&alloc_ctx);

    FileManager file_manager { filepath };
    diagnostic::set_source(&file_manager);

    // parse source code
    parser::Parser parser { &file_manager };
    Array<AST::StmtPtr> stmts = parser.parse_program();

    if (diagnostic::has_errors()) {
        diagnostic::dump();
        return ExitStatus::ERROR;
    }

    // compile source AST
    Compiler compiler(diagnostic::engine.source());
    ErrorOr<Chunk*, RuntimeError> compile_result = compiler.compile(stmts);
    if (diagnostic::has_errors()) {
        diagnostic::dump();
        return ExitStatus::ERROR;
    }

    if (compile_result.has_error()) {
        // handle compile error
    }

    Chunk* bytecode_chunk = compile_result.value();
    if (bytecode_chunk == nullptr) {
        std::cerr << "Compilation Failed, no bytecode was generated!\n";
        return ExitStatus::ERROR;
    }

    VM virtual_machine;
    auto result = virtual_machine.run(bytecode_chunk, &compiler);

    return ExitStatus::SUCCESS;
}

} // namespace fairuz::runtime
