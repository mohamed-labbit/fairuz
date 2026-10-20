//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Represents runtime failures with source tracebacks.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_DIAG_RUNTIME_ERROR_HPP
#define FAIRUZ_DIAG_RUNTIME_ERROR_HPP

#include "Diag/diagnostic.hpp"
#include "Diag/errc.hpp"
#include "Interpreter/value.hpp"
#include "Macros.hpp"
#include "SourceManager.hpp"
#include "Support/error.hpp"

#include <cassert>
#include <iostream>
#include <sstream>
#include <stack>
#include <string>

namespace fairuz::runtime {

struct CallFrame; // forward

struct FrameTrace {
    std::string function_name;
    std::string module_name;
    SourcePtr source;
    SourceLocation loc;

    FrameTrace() = delete;
    /// extract a FrameTrace from CallFrame
    FrameTrace(CallFrame const& frame);

    bool is_main() { return module_name == "__main__"; }
};

struct RuntimeError {
    ErrorCategory kind;
    ErrorCode errc;
    SourcePtr source;
    SourceLocation loc;
    std::stack<FrameTrace> traceback;
    std::string detail;
    RuntimeError const* cause { nullptr };

    RuntimeError() = delete;
    RuntimeError(ErrorCode errc, SourcePtr source, SourceLocation loc, std::string detail = "");

    void add_trace(CallFrame const& frame);
    ErrorCode get_code() const { return errc; }
    std::string str() const;
};

struct RuntimeRaised {
    RuntimeError const* error { nullptr };

    RuntimeRaised(RuntimeError* err)
        : error(err)
    {
        assert(error != nullptr);
    }
};

using ExecutionResult = ErrorOr<Value, RuntimeError>;

struct ImportCheckpoint {
    SourceLocation loc;
    ObjModule* module;
};

struct DiagnosticCheckpoint {
    std::stack<diagnostic::DiagnosticEngine::DiagnosticId> ids;
};

struct ExecutionCheckpoint {
    int frame_depth;
    int stack_top;
    ImportCheckpoint imports;
    DiagnosticCheckpoint diagnostics;
};

class ErrorRenderer {
public:
    std::string error_string(RuntimeError err) const
    {
        auto srcloc_to_string = [](SourceLocation const& srcloc) -> std::string {
            /// Prints source location in the form of
            /// Line: line_number | Column: column_number
            std::ostringstream oss;
            oss << "Line: " << std::to_string(srcloc.line);
            oss << " | ";
            oss << "Column: " << std::to_string(srcloc.column);
            return oss.str();
        };

        auto frame_to_string = [&](FrameTrace const& f) -> std::string {
            std::ostringstream oss;
            oss << "  File: " << (f.source ? f.source->path : "<host>");
            oss << ", Location: " << srcloc_to_string(f.loc);
            return oss.str();
        };

        std::ostringstream oss;

        /// print traceback if there's any
        /// most recent call is the last (Python style)
        if (!err.traceback.empty()) {
            oss << "Traceback (most recent call last):\n";
            while (!err.traceback.empty()) {
                FrameTrace const& frame = err.traceback.top();
                oss << frame_to_string(frame) + "\n";
                if (frame.source) oss << std::string(frame.source->line(frame.loc.line)) + "\n";
                err.traceback.pop();
            }
        } else if (err.source) {
            oss << "  File: " << err.source->path << ", Location: " << srcloc_to_string(err.loc) << "\n";
            oss << err.source->line(err.loc.line) << "\n";
        }

        /// print generated error details
        /// Print error category and error message
        oss << Color::RED + Color::BOLD << error_category_name(err.kind) << ": " << Color::RESET;
        oss << error_message_for(err.errc);
        if (!err.detail.empty()) oss << ": " << err.detail;
        oss << "\n";

        return oss.str();
    }

    void print_runtime_error(RuntimeError err) const { std::cerr << error_string(err) << std::endl; }

    // Reporting is a host decision; constructing/returning an error stays silent.
    void report(RuntimeError const& err) const
    {
        diagnostic::SourceScope scope(err.source);
        auto id = diagnostic::engine.report_deferred(diagnostic::Severity::ERROR, err.loc, err.errc, err.detail);
        auto frames = err.traceback;
        while (!frames.empty()) {
            auto const& frame = frames.top();
            diagnostic::engine.add_frame(id, frame.source, frame.loc, frame.function_name);
            frames.pop();
        }
    }
};

} // namespace fairuz::runtime

#endif // FAIRUZ_DIAG_RUNTIME_ERROR_HPP
