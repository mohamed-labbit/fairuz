//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Runs one source file through parsing, compilation, and VM execution.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_VM_RUNNER_HPP
#define FAIRUZ_VM_RUNNER_HPP

#include "Macros.hpp"

namespace fairuz::runtime {

// This API reports completion, not the script's final Value. Use an explicit
// comparison: conventional process success is zero, not a truthy value.
enum class ExitStatus : int {
    SUCCESS = 0,
    ERROR = 1,
};

struct RunOptions {
    bool dump_ast { false };
    bool dump_bytecode { false };
    bool print_time { false };
    bool check_only { false };
    bool json_diagnostics { false };
};

// Portable failure classification, not CLI-specific process exit numbers.
enum class RunFailure {
    NONE,
    PROGRAM,
    INPUT_NOT_FOUND,
    INTERNAL,
};

struct RunResult {
    ExitStatus status { ExitStatus::ERROR };
    RunFailure failure { RunFailure::INTERNAL };
};

// Execute a UTF-8 filesystem path using a fresh arena and VM. A null or empty
// path is an error; "-" is a filename, not an instruction to read stdin.
//
// Parser/compiler/runtime failures are rendered as text on stderr and return
// ERROR. Script output retains its normal stdout behavior. Function bodies
// remain lazily compiled: this executes a program rather than implementing
// the CLI's --check mode.
//
// The caller's allocator context and complete diagnostic state (including
// source, output mode, and prior diagnostics) are restored on every return.
// No AST, bytecode, VM Value, or runner diagnostic escapes the private arena.
// All C++ exceptions are contained, including failures while reporting errors;
// this does not intercept process termination, assertions, or hardware faults.
//
// The allocator and diagnostic engine are currently process-global. Callers
// must serialize this operation with other users of those globals.
FAIRUZ_NODISCARD ExitStatus run_protected(char const* filepath) noexcept;

// The shared program entry for CLI and integration tests. Options preserve
// the same isolation/lifetime guarantees as the convenience overload above.
// --check validates every deferred body without executing; AST dumping parses
// bodies eagerly; normal execution keeps unused bodies lazy. JSON mode emits
// one complete diagnostic array before restoring the caller's engine. Only
// status/classification escape; callers map failures to their own exit codes.
FAIRUZ_NODISCARD RunResult run_protected(char const* filepath, RunOptions const& options) noexcept;

} // namespace fairuz::runtime

#endif // FAIRUZ_VM_RUNNER_HPP
