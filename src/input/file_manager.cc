#include "../../include/input/file_manager.hpp"


std::pair<bool, std::string> FileManager::detect_encoding() const
{
    std::vector<char> buffer(4);
    stream_.seekg(0);
    stream_.read(buffer.data(), 4);
    const auto bytes_read = stream_.gcount();
    stream_.seekg(0);

    if (bytes_read >= 3)
        if (static_cast<unsigned char>(buffer[0]) == 0xEF && static_cast<unsigned char>(buffer[1]) == 0xBB
          && static_cast<unsigned char>(buffer[2]) == 0xBF)
            return {true, "UTF-8"};
    if (bytes_read >= 2)
    {
        if (static_cast<unsigned char>(buffer[0]) == 0xFE && static_cast<unsigned char>(buffer[1]) == 0xFF)
            return {true, "UTF-16BE"};
        if (static_cast<unsigned char>(buffer[0]) == 0xFF && static_cast<unsigned char>(buffer[1]) == 0xFE)
            return {true, "UTF-16LE"};
    }
    if (bytes_read >= 4)
    {
        if (static_cast<unsigned char>(buffer[0]) == 0x00 && static_cast<unsigned char>(buffer[1]) == 0x00
          && static_cast<unsigned char>(buffer[2]) == 0xFE && static_cast<unsigned char>(buffer[3]) == 0xFF)
            return {true, "UTF-32BE"};
        if (static_cast<unsigned char>(buffer[0]) == 0xFF && static_cast<unsigned char>(buffer[1]) == 0xFE
          && static_cast<unsigned char>(buffer[2]) == 0x00 && static_cast<unsigned char>(buffer[3]) == 0x00)
            return {true, "UTF-32LE"};
    }

    return {false, "UTF-8"};
}

std::size_t FileManager::validate_utf8_boundary(std::span<const char> buffer) const noexcept
{
    if (buffer.empty())
        return 0;

    const std::size_t size = buffer.size();
    // Search backwards up to 4 bytes for incomplete UTF-8 sequence
    for (std::size_t i = 0; i < std::min(MAX_UTF8_CHAR_BYTES, size); ++i)
    {
        const std::size_t pos = size - 1 - i;
        const auto byte = static_cast<unsigned char>(buffer[pos]);
        if ((byte & 0x80) == 0)
            return 0;
        if ((byte & 0xC0) == 0xC0)
        {
            const std::size_t expected = get_utf8_sequence_length(byte);
            const std::size_t available = size - pos;
            if (available < expected)
                return i + 1;
            // Validate the complete sequence
            if (!is_valid_utf8_sequence(buffer.subspan(pos, available)))
                return i + 1;
            return 0;
        }
    }

    return std::min(MAX_UTF8_CHAR_BYTES, size);
}

bool FileManager::is_valid_utf8_sequence(std::span<const char> seq) noexcept
{
    if (seq.empty())
        return false;
    const auto first = static_cast<unsigned char>(seq[0]);
    const std::size_t len = get_utf8_sequence_length(first);
    if (seq.size() < len)
        return false;
    // Validate continuation bytes
    for (std::size_t i = 1; i < len; ++i)
        if ((static_cast<unsigned char>(seq[i]) & 0xC0) != 0x80)
            return false;
    // Check for overlong encodings and invalid ranges
    if (len == 2)
    {
        const uint32_t cp = ((first & 0x1F) << 6) | (seq[1] & 0x3F);
        return cp >= 0x80;
    }
    if (len == 3)
    {
        const uint32_t cp = ((first & 0x0F) << 12) | ((seq[1] & 0x3F) << 6) | (seq[2] & 0x3F);
        return cp >= 0x800 && (cp < 0xD800 || cp > 0xDFFF);
    }
    if (len == 4)
    {
        const uint32_t cp = ((first & 0x07) << 18) | ((seq[1] & 0x3F) << 12) | ((seq[2] & 0x3F) << 6) | (seq[3] & 0x3F);
        return cp >= 0x10000 && cp <= 0x10FFFF;
    }
    return true;
}

constexpr std::size_t FileManager::get_utf8_sequence_length(unsigned char first_byte) noexcept
{
    if ((first_byte & 0x80) == 0)
        return 1;
    if ((first_byte & 0xE0) == 0xC0)
        return 2;
    if ((first_byte & 0xF0) == 0xE0)
        return 3;
    if ((first_byte & 0xF8) == 0xF0)
        return 4;
    return 1;
}

typename FileManager::BufferStrategy FileManager::determine_strategy(std::size_t file_size) const noexcept
{
    if (file_size > MMAP_THRESHOLD)
        return BufferStrategy::MemoryMapped;
    if (file_size < SMALL_FILE_THRESHOLD)
        return BufferStrategy::SmallFile;
    if (file_size < LARGE_FILE_THRESHOLD)
        return BufferStrategy::MediumFile;
    return BufferStrategy::LargeFile;
}

std::size_t FileManager::get_buffer_size() const noexcept
{
    switch (strategy_)
    {
    case BufferStrategy::SmallFile :
        return DEFAULT_BUFFER_SIZE * 4;
    case BufferStrategy::MediumFile :
        return DEFAULT_BUFFER_SIZE * 2;
    case BufferStrategy::LargeFile :
        return DEFAULT_BUFFER_SIZE;
    case BufferStrategy::Streaming :
        return DEFAULT_BUFFER_SIZE / 2;
    case BufferStrategy::MemoryMapped :
        return DEFAULT_BUFFER_SIZE * 8;
    default :
        return DEFAULT_BUFFER_SIZE;
    }
}

LineEnding FileManager::detect_line_endings() const
{
    stream_.seekg(0);
    std::vector<char> sample(std::min(stats_.total_bytes, std::size_t{8192}));
    stream_.read(sample.data(), sample.size());
    const auto bytes_read = stream_.gcount();
    stream_.seekg(0);
    std::size_t lf_count = 0;
    std::size_t crlf_count = 0;
    std::size_t cr_count = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(bytes_read); ++i)
    {
        if (sample[i] == '\r')
        {
            if (i + 1 < static_cast<std::size_t>(bytes_read) && sample[i + 1] == '\n')
            {
                ++crlf_count;
                ++i;
            }
            else
            {
                ++cr_count;
            }
        }
        else if (sample[i] == '\n')
        {
            ++lf_count;
        }
    }
    if (crlf_count > lf_count && crlf_count > cr_count)
        return LineEnding::CRLF;
    if (lf_count > crlf_count && lf_count > cr_count)
        return LineEnding::LF;
    if (cr_count > 0)
        return LineEnding::CR;
    if (crlf_count > 0 || lf_count > 0)
        return LineEnding::Mixed;
    return LineEnding::Unknown;
}

std::expected<void, FileManagerError> FileManager::build_line_index()
{
    if (line_index_built_)
        return {};
    std::unique_lock lock(mutex_);
    auto reset_result = reset();
    if (!reset_result)
        return reset_result;
    line_index_.clear();
    line_index_.reserve(stats_.total_bytes / 80);  // Estimate
    std::size_t byte_pos = 0;
    std::size_t char_pos = 0;
    std::size_t line_start_byte = 0;
    std::size_t line_start_char = 0;
    std::size_t max_line_len = 0;
    std::size_t current_line_len = 0;
    while (true)
    {
        auto chunk = read_window_internal(LINE_INDEX_CHUNK);
        if (!chunk)
            return std::unexpected(chunk.error());
        if (chunk->empty())
            break;
        for (char16_t c : *chunk)
        {
            ++current_line_len;

            if (c == u'\n' || c == u'\r')
            {
                max_line_len = std::max(max_line_len, current_line_len - 1);

                line_index_.push_back(LineIndex{line_start_byte, line_start_char, current_line_len - 1});
                // Handle CRLF
                if (c == u'\r' && char_pos + 1 < context_.char_offset)
                {
                    // Peek next char (already in chunk if available)
                    // This is simplified; in practice we'd need to handle this better
                }
                line_start_byte = context_.byte_offset;
                line_start_char = context_.char_offset;
                current_line_len = 0;
            }
            ++char_pos;
        }
    }
    // Add final line if file doesn't end with newline
    if (current_line_len > 0)
    {
        max_line_len = std::max(max_line_len, current_line_len);
        line_index_.push_back(LineIndex{line_start_byte, line_start_char, current_line_len});
    }
    stats_.total_lines = line_index_.size();
    stats_.max_line_length = max_line_len;
    if (stats_.total_lines > 0)
    {
        stats_.average_line_length = stats_.total_characters / stats_.total_lines;
    }
    line_index_built_ = true;
    auto error = reset();  // TODO : report error
    return {};
}

std::expected<std::u16string, FileManagerError> FileManager::read_window_internal(std::size_t size)
{
    if (size == 0)
        return std::u16string{};
    if (!is_open())
        return std::unexpected(FileManagerError::FileNotOpen);
    const std::size_t byte_chunk_size = size * MAX_UTF8_CHAR_BYTES;
    std::vector<char> byte_buffer(byte_chunk_size);
    stream_.read(byte_buffer.data(), byte_chunk_size);
    std::streamsize bytes_read = stream_.gcount();
    if (bytes_read == 0)
    {
        if (stream_.eof())
            return std::u16string();
        else
            return std::unexpected(FileManagerError::ReadError);
    }
    if (bytes_read < 0)
        return std::unexpected(FileManagerError::ReadError);
    byte_buffer.resize(static_cast<std::size_t>(bytes_read));
    const std::size_t bytes_to_rewind = validate_utf8_boundary(byte_buffer);
    if (bytes_to_rewind > 0)
    {
        byte_buffer.resize(bytes_read - bytes_to_rewind);
        stream_.seekg(-static_cast<std::streamoff>(bytes_to_rewind), std::ios_base::cur);
        if (stream_.fail())
            return std::unexpected(FileManagerError::SeekOutOfBounds);
    }
    const std::size_t valid_bytes = byte_buffer.size();
    context_.byte_offset += valid_bytes;
    context_.bytes_read_total += valid_bytes;
    std::u16string result;

    try
    {
        result = utf8::utf8to16(std::string(byte_buffer.begin(), byte_buffer.end()));
    } catch (const utf8::invalid_utf8&)
    {
        return std::unexpected(FileManagerError::InvalidUtf8);
    } catch (...)
    {
        return std::unexpected(FileManagerError::SystemError);
    }

    if (result.size() > size)
    {
        const std::size_t chars_to_trim = result.size() - size;
        std::string trimmed_utf8;
        try
        {
            auto trim_start = result.begin() + static_cast<std::ptrdiff_t>(size);
            utf8::utf16to8(trim_start, result.end(), std::back_inserter(trimmed_utf8));

            stream_.seekg(-static_cast<std::streamoff>(trimmed_utf8.size()), std::ios_base::cur);
            context_.byte_offset -= trimmed_utf8.size();
        } catch (...)
        {
        }

        result.resize(size);
    }

    context_.char_offset += result.size();
    context_.chars_read_total += result.size();

    return result;
}

std::optional<std::u16string> FileManager::check_cache(std::size_t char_offset, std::size_t size) const
{
    if (!enable_caching_)
        return std::nullopt;
    std::shared_lock lock(mutex_);
    const std::size_t cache_key = char_offset / CACHE_CHUNK_SIZE;
    auto it = cache_.find(cache_key);
    if (it != cache_.end())
    {
        const auto& entry = it->second;
        if (char_offset >= entry.char_offset && char_offset + size <= entry.char_offset + entry.data.size())
        {
            const std::size_t offset_in_cache = char_offset - entry.char_offset;
            ++const_cast<std::size_t&>(cache_hits_);
            const_cast<CacheEntry&>(entry).last_access = std::chrono::steady_clock::now();
            ++const_cast<CacheEntry&>(entry).access_count;
            return entry.data.substr(offset_in_cache, size);
        }
    }
    ++const_cast<std::size_t&>(cache_misses_);
    return std::nullopt;
}

void FileManager::update_cache(std::size_t char_offset, const std::u16string& data)
{
    if (!enable_caching_)
        return;
    std::unique_lock lock(mutex_);
    if (cache_.size() >= max_cache_entries_)
    {
        // LRU eviction
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
            if (it->second.last_access < oldest->second.last_access)
                oldest = it;
        cache_.erase(oldest);
    }
    const std::size_t cache_key = char_offset / CACHE_CHUNK_SIZE;
    cache_[cache_key] = CacheEntry{data, context_.byte_offset, char_offset, std::chrono::steady_clock::now(), 1};
}

std::expected<typename FileManager::FileStats, FileManagerError> FileManager::compute_stats()
{
    try
    {
        FileStats stats;
        stats.total_bytes = fs::file_size(filepath_);
        stats.last_modified = fs::last_write_time(filepath_);
        auto [has_bom, encoding] = detect_encoding();
        stats.has_bom = has_bom;
        stats.detected_encoding = encoding;
        stats.predominant_line_ending = detect_line_endings();
        // Skip BOM if present
        if (has_bom)
        {
            if (encoding == "UTF-8")
                stream_.seekg(3);
            else if (encoding == "UTF-16LE" || encoding == "UTF-16BE")
                stream_.seekg(2);
            else if (encoding == "UTF-32LE" || encoding == "UTF-32BE")
                stream_.seekg(4);
        }
        return stats;
    } catch (const fs::filesystem_error&)
    {
        return std::unexpected(FileManagerError::SystemError);
    }
}

FileManager::FileManager(const fs::path& filepath) :
    filepath_(filepath),
    stream_(filepath, std::ios::binary),
    strategy_(BufferStrategy::MediumFile)
{
    if (!fs::exists(filepath_))
        throw std::runtime_error("File not found: " + filepath_.string());
    if (!stream_.is_open())
        throw std::runtime_error("Failed to open file: " + filepath_.string());
    auto stats_result = compute_stats();
    if (!stats_result)
        throw std::runtime_error("Failed to read file statistics");
    stats_ = *stats_result;
    last_known_write_time_ = stats_.last_modified;
    strategy_ = determine_strategy(stats_.total_bytes);
    const std::size_t buffer_size = get_buffer_size();
    stream_.rdbuf()->pubsetbuf(nullptr, buffer_size);
}

bool FileManager::is_changed_since_last_read() const noexcept
{
    try
    {
        auto current_write_time = fs::last_write_time(filepath_);
        return current_write_time != last_known_write_time_;
    } catch (...)
    {
        return false;
    }
}

/// @brief Check if file stream is open and ready
bool FileManager::is_open() const noexcept { return stream_.is_open() && stream_.good(); }

/// @brief Get current context (position tracking)
typename FileManager::Context FileManager::get_context() const noexcept
{
    std::shared_lock lock(mutex_);
    return context_;
}

typename FileManager::FileStats FileManager::get_stats() const noexcept
{
    std::shared_lock lock(mutex_);
    return stats_;
}

std::pair<std::size_t, std::size_t> FileManager::get_cache_stats() const noexcept
{
    std::shared_lock lock(mutex_);
    return {cache_hits_, cache_misses_};
}

void FileManager::set_caching(bool enabled) noexcept
{
    std::unique_lock lock(mutex_);
    enable_caching_ = enabled;
    if (!enabled)
        cache_.clear();
}

void FileManager::set_max_cache_entries(std::size_t max_entries) noexcept
{
    std::unique_lock lock(mutex_);
    max_cache_entries_ = max_entries;
}

std::expected<void, FileManagerError> FileManager::reset()
{
    std::unique_lock lock(mutex_);
    if (!is_open())
        return std::unexpected(FileManagerError::FileNotOpen);
    stream_.clear();
    stream_.seekg(0, std::ios::beg);
    // Skip BOM if present
    if (stats_.has_bom)
    {
        if (stats_.detected_encoding == "UTF-8")
            stream_.seekg(3);
        else if (stats_.detected_encoding == "UTF-16LE" || stats_.detected_encoding == "UTF-16BE")
            stream_.seekg(2);
    }
    if (stream_.fail())
        return std::unexpected(FileManagerError::SeekOutOfBounds);
    context_.reset();
    return {};
}

std::expected<void, FileManagerError> FileManager::seek_to_char(std::size_t char_offset)
{
    std::unique_lock lock(mutex_);
    if (char_offset == context_.char_offset)
        return {};
    // Check cache first
    if (auto cached = check_cache(char_offset, 1))
    {
        context_.char_offset = char_offset;
        return {};
    }
    if (char_offset < context_.char_offset || char_offset - context_.char_offset > 10000)
    {
        auto reset_result = reset();
        if (!reset_result)
            return reset_result;
    }

    while (context_.char_offset < char_offset)
    {
        const std::size_t chars_to_read = char_offset - context_.char_offset;
        auto result = read_window_internal(std::min(chars_to_read, std::size_t{1024}));
        if (!result)
            return std::unexpected(result.error());
        if (result->empty())
            return std::unexpected(FileManagerError::UnexpectedEOF);
    }

    return {};
}

std::expected<void, FileManagerError> FileManager::seek_to_line(std::size_t line_number)
{
    if (!line_index_built_)
    {
        auto build_result = build_line_index();
        if (!build_result)
            return build_result;
    }

    std::unique_lock lock(mutex_);
    if (line_number >= line_index_.size())
        return std::unexpected(FileManagerError::InvalidLineNumber);
    const auto& line_info = line_index_[line_number];
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(line_info.byte_offset));
    if (stream_.fail())
        return std::unexpected(FileManagerError::SeekOutOfBounds);
    context_.byte_offset = line_info.byte_offset;
    context_.char_offset = line_info.char_offset;
    context_.line_number = line_number;
    context_.column_number = 0;
    return {};
}

std::expected<std::u16string, FileManagerError> FileManager::read_window(std::size_t size)
{
    std::unique_lock lock(mutex_);
    // Check cache
    if (auto cached = check_cache(context_.char_offset, size))
        return *cached;
    auto result = read_window_internal(size);
    if (result && enable_caching_)
        update_cache(context_.char_offset - result->size(), *result);
    return result;
}

std::expected<std::u16string, FileManagerError> FileManager::read_line(std::size_t line_number)
{
    auto seek_result = seek_to_line(line_number);
    if (!seek_result)
        return std::unexpected(seek_result.error());
    std::unique_lock lock(mutex_);
    const std::size_t line_length = line_index_[line_number].line_length;
    return read_window_internal(line_length);
}

std::expected<std::u16string, FileManagerError> FileManager::read_next_line()
{
    std::unique_lock lock(mutex_);
    std::u16string line;
    line.reserve(80);  // Average line length

    while (true)
    {
        auto chunk = read_window_internal(1);
        if (!chunk)
            return std::unexpected(chunk.error());
        if (chunk->empty())
            break;
        const char16_t c = (*chunk)[0];
        if (c == u'\n')
        {
            ++context_.line_number;
            context_.column_number = 0;
            break;
        }
        if (c == u'\r')
        {
            // Check for CRLF
            auto peek = read_window_internal(1);
            if (peek && !peek->empty() && (*peek)[0] == u'\n')
            {
                // CRLF - consume the \n
            }
            else if (peek)
            {
                // Just CR - rewind the peek
                stream_.seekg(-1, std::ios_base::cur);
                --context_.byte_offset;
                --context_.char_offset;
            }
            ++context_.line_number;
            context_.column_number = 0;
            break;
        }
        line += c;
        ++context_.column_number;
    }

    return line;
}

std::expected<std::vector<std::u16string>, FileManagerError> FileManager::read_lines(
  std::size_t start_line, std::size_t count)
{
    if (!line_index_built_)
    {
        auto build_result = build_line_index();
        if (!build_result)
            return std::unexpected(build_result.error());
    }
    if (start_line >= line_index_.size())
    {
        return std::unexpected(FileManagerError::InvalidLineNumber);
    }
    const std::size_t end_line = std::min(start_line + count, line_index_.size());
    std::vector<std::u16string> lines;
    lines.reserve(end_line - start_line);
    for (std::size_t i = start_line; i < end_line; ++i)
    {
        auto line = read_line(i);
        if (!line)
            return std::unexpected(line.error());
        lines.push_back(std::move(*line));
    }
    return lines;
}

std::expected<std::u16string, FileManagerError> FileManager::read_all()
{
    auto reset_result = reset();
    if (!reset_result)
        return std::unexpected(reset_result.error());
    std::u16string result;
    result.reserve(stats_.total_bytes / 2);
    while (true)
    {
        auto chunk = read_window(4096);
        if (!chunk)
            return std::unexpected(chunk.error());
        if (chunk->empty())
            break;
        result.append(*chunk);
    }
    return result;
}

std::expected<std::vector<std::size_t>, FileManagerError> FileManager::search(
  std::u16string_view pattern, std::size_t max_results)
{
    auto reset_result = reset();
    if (!reset_result)
        return std::unexpected(reset_result.error());
    std::vector<std::size_t> matches;
    matches.reserve(100);
    std::u16string buffer;
    buffer.reserve(pattern.size() * 2);
    std::size_t char_position = 0;

    while (matches.size() < max_results)
    {
        auto chunk = read_window(4096);
        if (!chunk)
            return std::unexpected(chunk.error());
        if (chunk->empty())
            break;

        buffer += *chunk;
        std::size_t pos = 0;

        while ((pos = buffer.find(pattern, pos)) != std::u16string::npos)
        {
            matches.push_back(char_position + pos);
            pos += pattern.size();
            if (matches.size() >= max_results)
                break;
        }

        // Keep last pattern.size()-1 chars for overlap detection
        if (buffer.size() >= pattern.size())
        {
            char_position += buffer.size() - (pattern.size() - 1);
            buffer = buffer.substr(buffer.size() - (pattern.size() - 1));
        }
        else
        {
            char_position += buffer.size();
        }
    }
    return matches;
}

std::expected<void, FileManagerError> FileManager::create_bookmark(const std::string& name)
{
    std::unique_lock lock(mutex_);
    bookmarks_[name] = Bookmark{name, context_, std::chrono::system_clock::now()};
    return {};
}

std::expected<void, FileManagerError> FileManager::goto_bookmark(const std::string& name)
{
    std::shared_lock lock(mutex_);
    auto it = bookmarks_.find(name);
    if (it == bookmarks_.end())
        return std::unexpected(FileManagerError::InvalidCharacterOffset);
    lock.unlock();
    return seek_to_char(it->second.context.char_offset);
}

std::vector<std::string> FileManager::get_bookmarks() const
{
    std::shared_lock lock(mutex_);
    std::vector<std::string> names;
    names.reserve(bookmarks_.size());
    for (const auto& [name, _] : bookmarks_)
        names.push_back(name);
    return names;
}

void FileManager::push_position()
{
    std::unique_lock lock(mutex_);
    position_stack_.push_back(context_);
    // Limit stack size
    if (position_stack_.size() > 100)
        position_stack_.erase(position_stack_.begin());
}

std::expected<void, FileManagerError> FileManager::pop_position()
{
    std::unique_lock lock(mutex_);
    if (position_stack_.empty())
        return std::unexpected(FileManagerError::InvalidCharacterOffset);
    const auto prev_context = position_stack_.back();
    position_stack_.pop_back();
    lock.unlock();
    return seek_to_char(prev_context.char_offset);
}

std::size_t FileManager::get_position_stack_size() const noexcept
{
    std::shared_lock lock(mutex_);
    return position_stack_.size();
}

std::expected<void, FileManagerError> FileManager::refresh_stats()
{
    auto new_stats = compute_stats();
    if (!new_stats)
        return std::unexpected(new_stats.error());
    std::unique_lock lock(mutex_);
    stats_ = *new_stats;
    last_known_write_time_ = stats_.last_modified;
    strategy_ = determine_strategy(stats_.total_bytes);
    // Invalidate line index if file changed
    if (is_changed_since_last_read())
    {
        line_index_.clear();
        line_index_built_ = false;
        cache_.clear();
    }
    return {};
}

std::expected<void, FileManagerError> FileManager::ensure_line_index()
{
    if (line_index_built_)
        return {};
    return build_line_index();
}

std::expected<std::size_t, FileManagerError> FileManager::get_line_count()
{
    if (!line_index_built_)
    {
        auto build_result = build_line_index();
        if (!build_result)
            return std::unexpected(build_result.error());
    }

    std::shared_lock lock(mutex_);
    return stats_.total_lines;
}

std::expected<std::u16string, FileManagerError> FileManager::read_reverse(std::size_t char_count)
{
    std::unique_lock lock(mutex_);
    if (context_.char_offset < char_count)
        char_count = context_.char_offset;
    if (char_count == 0)
        return std::u16string{};
    const std::size_t target_offset = context_.char_offset - char_count;
    lock.unlock();
    auto seek_result = seek_to_char(target_offset);
    if (!seek_result)
        return std::unexpected(seek_result.error());
    return read_window(char_count);
}

std::expected<char16_t, FileManagerError> FileManager::peek_char(std::size_t char_offset)
{
    push_position();
    auto seek_result = seek_to_char(char_offset);
    if (!seek_result)
    {
        auto error = pop_position();  // TODO : report error
        return std::unexpected(seek_result.error());
    }
    auto result = read_window(1);
    auto error = pop_position();  // TODO : report error
    if (!result)
        return std::unexpected(result.error());
    if (result->empty())
        return std::unexpected(FileManagerError::UnexpectedEOF);
    return (*result)[0];
}

std::expected<std::u16string, FileManagerError> FileManager::peek_range(std::size_t start_offset, std::size_t length)
{
    push_position();
    auto seek_result = seek_to_char(start_offset);
    if (!seek_result)
    {
        auto error = pop_position();  // TODO : report error
        return std::unexpected(seek_result.error());
    }
    auto result = read_window(length);
    auto error = pop_position();  // TODO : report error
    return result;
}

std::expected<std::size_t, FileManagerError> FileManager::count_char(char16_t target)
{
    auto reset_result = reset();
    if (!reset_result)
        return std::unexpected(reset_result.error());
    std::size_t count = 0;

    while (true)
    {
        auto chunk = read_window(4096);
        if (!chunk)
            return std::unexpected(chunk.error());
        if (chunk->empty())
            break;
        count += std::count(chunk->begin(), chunk->end(), target);
    }

    auto error = reset();  // TODO : report error
    return count;
}

const fs::path& FileManager::get_path() const noexcept { return filepath_; }

typename FileManager::BufferStrategy FileManager::get_strategy() const noexcept
{
    std::shared_lock lock(mutex_);
    return strategy_;
}

void FileManager::clear_all_caches() noexcept
{
    std::unique_lock lock(mutex_);
    cache_.clear();
    line_index_.clear();
    position_stack_.clear();
    bookmarks_.clear();
    line_index_built_ = false;
    cache_hits_ = 0;
    cache_misses_ = 0;
}

std::size_t FileManager::estimate_memory_usage() const noexcept
{
    std::shared_lock lock(mutex_);
    std::size_t total = 0;
    // Cache
    for (const auto& [_, entry] : cache_)
        total += entry.data.size() * sizeof(char16_t);
    // Line index
    total += line_index_.size() * sizeof(LineIndex);
    // Position stack
    total += position_stack_.size() * sizeof(Context);
    // Bookmarks
    total += bookmarks_.size() * (sizeof(Bookmark) + 32);  // Rough estimate
    return total;
}


void FileManager::iterator::advance()
{
    if (at_end_)
        return;
    auto result = manager_->read_window(1);
    if (!result || result->empty())
    {
        at_end_ = true;
        current_ = std::nullopt;
    }
    else
    {
        current_ = (*result)[0];
    }
}

FileManager::iterator::iterator(FileManager* mgr, bool end) :
    manager_(mgr),
    at_end_(end)
{
    if (!at_end_ && manager_)
        advance();
}

typename FileManager::iterator::reference FileManager::iterator::operator*() const { return *current_; }

typename FileManager::iterator::pointer FileManager::iterator::operator->() const { return &(*current_); }

FileManager::iterator& FileManager::iterator::operator++()
{
    advance();
    return *this;
}

FileManager::iterator FileManager::iterator::operator++(int)
{
    iterator tmp = *this;
    advance();
    return tmp;
}

bool FileManager::iterator::operator==(const iterator& other) const
{
    if (at_end_ && other.at_end_)
        return true;
    if (at_end_ != other.at_end_)
        return false;
    return manager_ == other.manager_ && current_ == other.current_;
}

bool FileManager::iterator::operator!=(const iterator& other) const { return !(*this == other); }

FileManager::iterator FileManager::begin()
{
    auto error = reset();  // TODO : report error
    return iterator(this, false);
}

FileManager::iterator FileManager::end() { return iterator(this, true); }