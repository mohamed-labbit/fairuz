#include "fsource_manager.hpp"

#include <algorithm>
#include <utility>

namespace fairuz {

SourceManager::SourceManager(std::string file_path, std::string contents)
    : path(file_path.empty() ? "<input>" : std::move(file_path))
    , text(std::move(contents))
    , lines { 0 }
{
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] == '\n')
            lines.push_back(i + 1);
}

std::string_view SourceManager::line(size_t number) const noexcept
{
    if (number == 0 || number > lines.size())
        return { };
    size_t from = lines[number - 1];
    size_t to = number < lines.size() ? lines[number] - 1 : text.size();
    if (to > from && text[to - 1] == '\r')
        --to;
    return std::string_view(text).substr(from, to - from);
}

std::string_view SourceManager::source_slice(size_t start, size_t end) const noexcept
{
    if (start >= text.size() || end <= start)
        return { };
    return std::string_view(text).substr(start, std::min(end, text.size()) - start);
}

} // namespace fairuz
