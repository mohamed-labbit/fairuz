#ifndef FA_FORMAT_HPP
#define FA_FORMAT_HPP

#include "fstring.hpp"

namespace fairuz {

class Fa_Formatter {
public:
    // Format validated source, preserving its tokens and comments. Throws if
    // the result does not parse or changes the significant token stream.
    [[nodiscard]] Fa_StringRef format(Fa_StringRef const& source);
};

} // namespace fairuz

#endif // FA_FORMAT_HPP
