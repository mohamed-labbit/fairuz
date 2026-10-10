#pragma once

#include "ffile.hpp"

#include <memory>
#include <vector>

namespace fairuz {

// A byte-only source snapshot. No allocator context, Unicode decoding, or
// diagnostic engine is needed. Share as const to keep its views stable.
class SourceManager {
public:
    SourceManager(std::string path, std::string text);
    explicit SourceManager(FileManager const& file)
        : SourceManager(file.get_path(), file.buffer())
    {
    }

    std::string_view line(size_t number) const noexcept;
    std::string_view source_slice(size_t start, size_t end) const noexcept;
    std::string_view buffer() const noexcept { return text; }

    std::string path;
    std::string text;
    std::vector<size_t> lines;
};

using SourcePtr = std::shared_ptr<SourceManager const>;

} // namespace fairuz
