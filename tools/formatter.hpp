//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Formats Fairuz source while preserving its semantics.
//===----------------------------------------------------------------------===//

#ifndef FA_FORMAT_HPP
#define FA_FORMAT_HPP

#include "Support/string.hpp"

namespace fairuz {

class Formatter {
public:
    // Format validated source, preserving its tokens and comments. Throws if
    // the result does not parse or changes the significant token stream.
    [[nodiscard]] StringRef format(StringRef const& source);
};

} // namespace fairuz

#endif // FA_FORMAT_HPP
