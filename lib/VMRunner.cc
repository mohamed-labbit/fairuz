//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Owns the one-shot file execution boundary and its temporary host state.
//===----------------------------------------------------------------------===//

#include "VMRunner.hpp"
#include "Compiler/compiler.hpp"
#include "Diag/diagnostic.hpp"
#include "Diag/runtime_error.hpp"
#include "File.hpp"
#include "Interpreter/vm.hpp"
#include "Parser/ast_printer.hpp"
#include "Parser/parser.hpp"
#include "Support/arena.hpp"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace fairuz::runtime {
namespace {

// Moving the engine must not allocate: restoring a caller's state must also
// work while unwinding an out-of-memory or diagnostic-rendering failure.
static_assert(std::is_nothrow_move_constructible_v<diagnostic::DiagnosticEngine>);
static_assert(std::is_nothrow_move_assignable_v<diagnostic::DiagnosticEngine>);

class RestoreHostState {
public:
    RestoreHostState() noexcept
        : m_previous_allocator(g_context)
        , m_previous_diagnostics(std::move(diagnostic::engine))
    {
        diagnostic::reset();
        // This boundary renders its own diagnostics. A caller's JSON mode
        // must neither suppress those messages nor be changed permanently.
        diagnostic::engine.set_json_output(false);
    }

    ~RestoreHostState() noexcept
    {
        set_context(m_previous_allocator);
        diagnostic::engine = std::move(m_previous_diagnostics);
    }

    RestoreHostState(RestoreHostState const&) = delete;
    RestoreHostState& operator=(RestoreHostState const&) = delete;

private:
    AllocatorContext* m_previous_allocator;
    diagnostic::DiagnosticEngine m_previous_diagnostics;
};

struct RunContext {
    // Order is intentional: destroy every arena-owned object while this
    // arena is still the active allocator, then restore the caller's state.
    // The first member also restores diagnostics if arena construction fails.
    RestoreHostState restore_host_state;
    AllocatorContext allocator;
    RunOptions const& options;
    std::string compile_diagnostics;

    explicit RunContext(RunOptions const& options)
        : options(options)
    {
        set_context(&allocator);
        diagnostic::engine.set_json_output(options.json_diagnostics);
    }

    void preserve_compile_diagnostics()
    {
        diagnostic::dump();
        if (options.json_diagnostics) {
            // VM setup clears the engine. Serialize the compile phase while
            // its source and annotations are still available, then combine
            // the two arrays' contents into one document at the boundary.
            auto json = diagnostic::engine.to_json();
            compile_diagnostics = json.substr(1, json.rfind(']') - 1);
        }
        // Clear even if VM construction fails before its own reset. This
        // prevents a preserved compile-phase warning from being emitted twice.
        diagnostic::reset();
    }

    void finish_diagnostics() const
    {
        if (!options.json_diagnostics) {
            diagnostic::dump();
            return;
        }
        auto json = diagnostic::engine.to_json();
        if (!compile_diagnostics.empty())
            json.insert(1, compile_diagnostics + (json[1] == ']' ? "" : ","));
        std::cerr << json;
    }

    RunContext(RunContext const&) = delete;
    RunContext& operator=(RunContext const&) = delete;
};

constexpr RunResult failed(RunFailure failure = RunFailure::PROGRAM)
{
    return { ExitStatus::ERROR, failure };
}

RunResult diagnostic_failure()
{
    auto* error = diagnostic::engine.first_error();
    return failed(error != nullptr && error_category_for(error->err_code) == ErrorCategory::INTERNAL
            ? RunFailure::INTERNAL
            : RunFailure::PROGRAM);
}

RunResult report_host_error(ErrorCode code, std::string const& detail = "")
{
    // File I/O, host exceptions, and setup failures do not have a script
    // location. Never borrow an unrelated source left by deferred compilation.
    diagnostic::SourceScope source_scope(SourcePtr { });
    diagnostic::report(diagnostic::Severity::ERROR, { }, code, detail);
    diagnostic::dump();
    return failed(code == ErrorCode::FILE_NOT_FOUND ? RunFailure::INPUT_NOT_FOUND
            : code == ErrorCode::INTERNAL_ERROR     ? RunFailure::INTERNAL
                                                    : RunFailure::PROGRAM);
}

RunResult report_execution_error(RuntimeError const& error)
{
    // Deferred parsing/imports can return an error whose original diagnostic
    // is already recorded. Report returned-only errors, but never duplicate
    // an existing parser diagnostic or its source information.
    if (!diagnostic::has_errors())
        ErrorRenderer().report(error);
    diagnostic::dump();
    return failed(error.kind == ErrorCategory::INTERNAL ? RunFailure::INTERNAL : RunFailure::PROGRAM);
}

RunResult run_file(char const* filepath, RunContext& context)
{
    if (filepath == nullptr || *filepath == '\0')
        return report_host_error(ErrorCode::FILE_NOT_OPEN, "input path is empty");

    FileManager file(filepath);
    diagnostic::SourceScope source_scope(&file);

    // Keep the original source snapshot alive independently of the engine:
    // VM construction/run reset diagnostics, and deferred bodies need their
    // definition's source even after the top-level parser has finished.
    SourcePtr source = diagnostic::engine.source();
    auto const& options = context.options;
    parser::Parser parser(&file, options.dump_ast ? parser::BodyParsing::Eager : parser::BodyParsing::Lazy);
    auto statements = parser.parse_program();
    if (diagnostic::has_errors()) {
        diagnostic::dump(); // Printing is incremental; parse_program may have printed already.
        return diagnostic_failure();
    }

    if (options.dump_ast) {
        AST::ASTPrinter printer(true);
        for (auto* statement : statements)
            printer.print(statement);
    }

    Compiler compiler(std::move(source));
    auto compiled = compiler.compile(statements);
    if (compiled.has_error())
        return report_execution_error(compiled.error());
    if (diagnostic::has_errors()) {
        diagnostic::dump();
        return diagnostic_failure();
    }

    auto* chunk = compiled.value();
    if (chunk == nullptr)
        return report_host_error(ErrorCode::INTERNAL_ERROR, "compilation produced no bytecode");

    if (options.check_only) {
        auto validation = Compiler::compile_all(chunk);
        if (validation.has_error())
            return report_execution_error(validation.error());
        if (diagnostic::has_errors())
            return diagnostic_failure();
    }
    if (options.dump_bytecode)
        chunk->disassemble();
    if (options.check_only)
        return { ExitStatus::SUCCESS, RunFailure::NONE };

    context.preserve_compile_diagnostics();

    // The VM dies before the arena: GC objects can reference arena-backed
    // chunks, strings, and deferred compiler metadata throughout execution.
    VM virtual_machine;
    auto start = std::chrono::steady_clock::now();
    auto executed = virtual_machine.run(chunk);
    auto end = std::chrono::steady_clock::now();
    if (executed.has_error())
        return report_execution_error(executed.error());
    if (diagnostic::has_errors()) {
        diagnostic::dump();
        return diagnostic_failure();
    }
    if (options.print_time) {
        std::chrono::duration<double> elapsed = end - start;
        std::cerr << "time: " << elapsed.count() << "s\n";
    }
    diagnostic::dump();
    return { ExitStatus::SUCCESS, RunFailure::NONE };
}

RunResult run_with_error_translation(char const* filepath, RunContext& context)
{
    // RunContext is owned by the caller, so both the arena and isolated
    // diagnostics remain alive inside every handler below.
    try {
        return run_file(filepath, context);
    } catch (diagnostic::DiagnosticAbort const&) {
        // Lexing and explicit panic paths normally rendered their diagnostic
        // before throwing. dump() prints only what has not been printed yet.
        if (!diagnostic::has_errors())
            return report_host_error(ErrorCode::INTERNAL_ERROR, "diagnostic abort without an error");
        diagnostic::dump();
        return diagnostic_failure();
    } catch (RuntimeRaised const& raised) {
        if (raised.error == nullptr)
            return report_host_error(ErrorCode::INTERNAL_ERROR, "runtime raised an empty error");
        return report_execution_error(*raised.error);
    } catch (RuntimeHalt const&) {
        // Legacy payload-free transport must never turn into successful
        // execution. Do not fabricate a source or silently lose the failure.
        if (!diagnostic::has_errors())
            return report_host_error(ErrorCode::INTERNAL_ERROR, "runtime halted without a diagnostic");
        diagnostic::dump();
        return diagnostic_failure();
    } catch (FileError const& error) {
        ErrorCode code = error.kind() == FileError::Kind::NotFound ? ErrorCode::FILE_NOT_FOUND
            : error.kind() == FileError::Kind::Read                ? ErrorCode::READ_ERROR
                                                                   : ErrorCode::FILE_NOT_OPEN;
        return report_host_error(code, error.path());
    } catch (std::bad_alloc const&) {
        return report_host_error(ErrorCode::ALLOC_FAILED);
    } catch (std::length_error const&) {
        return report_host_error(ErrorCode::ALLOC_FAILED);
    } catch (std::exception const& error) {
        return report_host_error(ErrorCode::INTERNAL_ERROR, error.what());
    } catch (...) {
        return report_host_error(ErrorCode::INTERNAL_ERROR, "unknown host exception");
    }
}

} // namespace

ExitStatus run_protected(char const* filepath) noexcept
{
    return run_protected(filepath, RunOptions { }).status;
}

RunResult run_protected(char const* filepath, RunOptions const& options) noexcept
{
    try {
        RunContext context(options);
        auto result = run_with_error_translation(filepath, context);
        context.finish_diagnostics();
        return result;
    } catch (std::bad_alloc const&) {
        // Error construction/rendering may itself allocate. Use a fixed C
        // stdio message here: no allocation, iostream exceptions, or access
        // to an arena/diagnostic engine that may already have been restored.
        std::fputs("fairuz: memory allocation failed while running or reporting an error\n", stderr);
        return failed();
    } catch (...) {
        std::fputs("fairuz: unable to run the program or report its failure\n", stderr);
    }
    return failed(RunFailure::INTERNAL);
}

} // namespace fairuz::runtime
