#include "../../../include/input/buffer/base.hpp"
#include "../../../utfcpp/source/utf8.h"

#include <iostream>
#include <vector>


namespace mylang {
namespace lex {
namespace buffer {

bool InputBufferBase::refresh_buffer(const std::uint32_t to_refresh)
{
    if (!file_manager_->is_open())
        return false;

    auto& bufs = this->buffers_;
    std::size_t max_chars = bufs[to_refresh].size() - 1;
    buffer_t buf = file_manager_->read_window(max_chars);

    if (buf.empty())
    {
        bufs[to_refresh].clear();
        bufs[to_refresh].push_back(BUFFER_END);
        return false;
    }

    bufs[to_refresh].assign(buf.begin(), buf.end());
    bufs[to_refresh].push_back(BUFFER_END);
    return true;
}

}
}  // lex
}  // mylang
