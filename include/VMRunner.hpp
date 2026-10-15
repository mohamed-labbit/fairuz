//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Virtual Machine runner.
//===----------------------------------------------------------------------===//

namespace fairuz::runtime {

enum class ExitStatus : int {
    ERROR,
    SUCCESS,
};

ExitStatus run_protected(char const* filepath);

} // namespace fairuz::runtime
