#pragma once


#include "../../utfcpp/source/utf8.h"
#include "../macros.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>


namespace fs = std::filesystem;


/// @brief Error codes for FileManager operations
enum class FileManagerError {
    FILE_NOT_FOUND,
    FILE_NOT_OPEN,
    SEEK_OUT_OF_BOUND,
    READ_ERROR,
    INVALID_UTF8,
    INVALID_CHAR_OFFSET,
    PERMISSION_DENIED,
    UNEXPECTED_EOF,
    SYSTEM_ERROR,
    ENCODING_ERROR,
    CACHE_ERROR,
    INVALID_LINE_NUMBER,
    BUFFER_TOO_SMALL
};

/// @brief Convert error code to human-readable string
MYLANG_INLINE constexpr const char* to_string(FileManagerError error) MYLANG_NOEXCEPT
{
    switch (error)
    {
    case FileManagerError::FILE_NOT_FOUND : return "File not found";
    case FileManagerError::FILE_NOT_OPEN : return "File is not open";
    case FileManagerError::SEEK_OUT_OF_BOUND : return "Seek position out of bounds";
    case FileManagerError::READ_ERROR : return "Failed to read from file";
    case FileManagerError::INVALID_UTF8 : return "Invalid UTF-8 sequence encountered";
    case FileManagerError::INVALID_CHAR_OFFSET : return "Invalid character offset";
    case FileManagerError::PERMISSION_DENIED : return "Permission denied";
    case FileManagerError::UNEXPECTED_EOF : return "Unexpected end of file";
    case FileManagerError::SYSTEM_ERROR : return "System error occurred";
    case FileManagerError::ENCODING_ERROR : return "Encoding conversion error";
    case FileManagerError::CACHE_ERROR : return "Cache operation failed";
    case FileManagerError::INVALID_LINE_NUMBER : return "Invalid line number";
    case FileManagerError::BUFFER_TOO_SMALL : return "Buffer too small for operation";
    default : return "Unknown error";
    }
}

/// @brief Line ending types
enum class LineEnding {
    LF,  // Unix/Linux/macOS (\n)
    CRLF,  // Windows (\r\n)
    CR,  // Classic Mac (\r)
    MIXED,  // Mixed line endings
    UNKNOWN
};

/// @brief High-performance UTF-8 aware file reader with advanced features
///
/// Features:
/// - Thread-safe read operations with reader-writer locks
/// - UTF-8/UTF-16/UTF-32 support with BOM detection
/// - Character and line-based addressing with byte-level precision
/// - Intelligent caching with LRU eviction
/// - Memory-mapped file support for large files
/// - Line index building for O(1) line access
/// - Bidirectional seeking with position stack
/// - File change monitoring with auto-reload
/// - Streaming interface with iterator support
/// - SIMD-optimized validation where available
/// - Comprehensive error reporting with context
/// - Zero-copy optimizations
class FileManager
{
   public:
    /// @brief Statistics about file content
    struct FileStats
    {
        std::size_t total_bytes{0};
        std::size_t total_characters{0};
        std::size_t total_lines{0};
        std::size_t max_line_length{0};
        LineEnding predominant_line_ending{LineEnding::UNKNOWN};
        std::optional<std::size_t> average_line_length;
        fs::file_time_type last_modified;
        bool has_bom{false};
        std::string detected_encoding{"UTF-8"};
    };

    /// @brief Context tracking current position and state
    struct Context
    {
        std::size_t byte_offset{0};  ///< Current byte position in file
        std::size_t char_offset{0};  ///< Current character position
        std::size_t line_number{0};  ///< Current line number (0-indexed)
        std::size_t column_number{0};  ///< Current column in line
        std::size_t bytes_read_total{0};  ///< Total bytes read in session
        std::size_t chars_read_total{0};  ///< Total characters read in session

        void reset() MYLANG_NOEXCEPT
        {
            byte_offset = 0;
            char_offset = 0;
            line_number = 0;
            column_number = 0;
            bytes_read_total = 0;
            chars_read_total = 0;
        }
    };

    /// @brief Buffer management strategy
    enum class BufferStrategy {
        SMALL_FILE,  ///< < 1MB - aggressive buffering
        MEDIUM_FILE,  ///< 1MB - 100MB - balanced
        LARGE_FILE,  ///< > 100MB - conservative buffering
        STREAMING,  ///< Minimal buffering for streaming reads
        MEMORY_MAPPED  ///< Use memory mapping for very large files
    };

    /// @brief Cache entry for frequently accessed regions
    struct CacheEntry
    {
        std::u16string data;
        std::size_t byte_offset;
        std::size_t char_offset;
        std::chrono::steady_clock::time_point last_access;
        std::size_t access_count{0};
    };

    /// @brief Line index entry for fast line access
    struct LineIndex
    {
        std::size_t byte_offset;
        std::size_t char_offset;
        std::size_t line_length;
    };

    /// @brief Position bookmark for navigation
    struct Bookmark
    {
        std::string name;
        Context context;
        std::chrono::system_clock::time_point created;
    };

   private:
    fs::path filepath_;
    mutable std::ifstream stream_;
    mutable Context context_;
    FileStats stats_;
    BufferStrategy strategy_;
    fs::file_time_type last_known_write_time_;

    // Advanced features
    mutable std::shared_mutex mutex_;  ///< Thread-safety
    std::vector<LineIndex> line_index_;  ///< Fast line lookup
    std::unordered_map<std::size_t, CacheEntry> cache_;  ///< LRU cache
    std::vector<Context> position_stack_;  ///< Navigation history
    std::unordered_map<std::string, Bookmark> bookmarks_;  ///< Named positions
    bool line_index_built_{false};
    bool enable_caching_{true};
    std::size_t max_cache_entries_{100};
    std::size_t cache_hits_{0};
    std::size_t cache_misses_{0};

    // Configuration
    static constexpr std::size_t DEFAULT_BUFFER_SIZE = 8192;
    static constexpr std::size_t MAX_UTF8_CHAR_BYTES = 4;
    static constexpr std::size_t SMALL_FILE_THRESHOLD = 1'048'576;  // 1MB
    static constexpr std::size_t LARGE_FILE_THRESHOLD = 104'857'600;  // 100MB
    static constexpr std::size_t MMAP_THRESHOLD = 524'288'000;  // 500MB
    static constexpr std::size_t LINE_INDEX_CHUNK = 1000;
    static constexpr std::size_t CACHE_CHUNK_SIZE = 4096;

    /// @brief Detect BOM and encoding
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::pair<bool, std::string> detect_encoding() const;
    /// @brief Validate UTF-8 sequence and find truncation point
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::size_t validate_utf8_boundary(
      std::span<const char> buffer) const MYLANG_NOEXCEPT;
    /// @brief Validate a complete UTF-8 sequence
    MYLANG_COMPILER_ABI MYLANG_NODISCARD static bool is_valid_utf8_sequence(std::span<const char> seq) MYLANG_NOEXCEPT;
    /// @brief Get expected UTF-8 sequence length from first byte
    MYLANG_COMPILER_ABI MYLANG_NODISCARD static constexpr std::size_t get_utf8_sequence_length(
      unsigned char first_byte) MYLANG_NOEXCEPT;
    /// @brief Determine optimal buffer strategy based on file size
    MYLANG_COMPILER_ABI MYLANG_NODISCARD BufferStrategy determine_strategy(std::size_t file_size) const MYLANG_NOEXCEPT;
    /// @brief Get buffer size based on strategy
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::size_t get_buffer_size() const MYLANG_NOEXCEPT;
    /// @brief Detect predominant line ending type
    MYLANG_COMPILER_ABI MYLANG_NODISCARD LineEnding detect_line_endings() const;
    /// @brief Build line index for fast line-based access
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> build_line_index();
    /// @brief Internal read window without locking
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> read_window_internal(
      std::size_t size);
    /// @brief Check and update cache
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::optional<std::u16string> check_cache(
      std::size_t char_offset, std::size_t size) const;
    /// @brief Add entry to cache with LRU eviction
    MYLANG_COMPILER_ABI void update_cache(std::size_t char_offset, const std::u16string& data);
    /// @brief Initialize file statistics
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<FileStats, FileManagerError> compute_stats();

   public:
    /// @brief Construct FileManager for given file path
    explicit FileManager(const fs::path& filepath);

    FileManager(FileManager&& other) MYLANG_NOEXCEPT = delete;
    FileManager& operator=(FileManager&& other) MYLANG_NOEXCEPT = delete;

    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    /// @brief Check if file has been modified since last read
    MYLANG_COMPILER_ABI MYLANG_NODISCARD bool is_changed_since_last_read() const MYLANG_NOEXCEPT;
    /// @brief Check if file stream is open and ready
    MYLANG_COMPILER_ABI MYLANG_NODISCARD bool is_open() const MYLANG_NOEXCEPT;
    /// @brief Get current context (position tracking)
    MYLANG_COMPILER_ABI MYLANG_NODISCARD Context get_context() const MYLANG_NOEXCEPT;
    /// @brief Get file statistics
    MYLANG_COMPILER_ABI MYLANG_NODISCARD FileStats get_stats() const MYLANG_NOEXCEPT;
    /// @brief Get cache statistics
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::pair<std::size_t, std::size_t> get_cache_stats() const MYLANG_NOEXCEPT;
    /// @brief Enable or disable caching
    MYLANG_COMPILER_ABI void set_caching(bool enabled) MYLANG_NOEXCEPT;
    /// @brief Set maximum cache entries
    MYLANG_COMPILER_ABI void set_max_cache_entries(std::size_t max_entries) MYLANG_NOEXCEPT;
    /// @brief Reset read position to beginning of file
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> reset();
    /// @brief Seek to specific character offset
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> seek_to_char(std::size_t char_offset);
    /// @brief Seek to specific line number
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> seek_to_line(std::size_t line_number);
    /// @brief Read a window of UTF-16 characters starting from current position
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> read_window(std::size_t size);
    /// @brief Read specific line by number
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> read_line(
      std::size_t line_number);
    /// @brief Read next line from current position
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> read_next_line();
    /// @brief Read range of lines
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::vector<std::u16string>, FileManagerError> read_lines(
      std::size_t start_line, std::size_t count);
    /// @brief Read entire file as UTF-16 string
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> read_all();
    /// @brief Search for pattern in file
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::vector<std::size_t>, FileManagerError> search(
      std::u16string_view pattern, std::size_t max_results = 1000);
    /// @brief Create bookmark at current position
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> create_bookmark(const std::string& name);
    /// @brief Jump to bookmark
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> goto_bookmark(const std::string& name);
    /// @brief Get all bookmark names
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::vector<std::string> get_bookmarks() const;
    /// @brief Push current position to navigation stack
    MYLANG_COMPILER_ABI void push_position();
    /// @brief Pop and restore previous position
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> pop_position();
    /// @brief Get number of positions in navigation stack
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::size_t get_position_stack_size() const MYLANG_NOEXCEPT;
    /// @brief Update file statistics and check for changes
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> refresh_stats();
    /// @brief Build line index for fast line access
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<void, FileManagerError> ensure_line_index();
    /// @brief Get total number of lines (builds index if needed)
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::size_t, FileManagerError> get_line_count();
    /// @brief Read file in reverse order (from end to beginning)
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> read_reverse(
      std::size_t char_count);
    /// @brief Get character at specific offset without moving position
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<char16_t, FileManagerError> peek_char(std::size_t char_offset);
    /// @brief Get substring without changing position
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::u16string, FileManagerError> peek_range(
      std::size_t start_offset, std::size_t length);
    /// @brief Count occurrences of character
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::expected<std::size_t, FileManagerError> count_char(char16_t target);
    /// @brief Get file path
    MYLANG_COMPILER_ABI MYLANG_NODISCARD const fs::path& get_path() const MYLANG_NOEXCEPT;
    /// @brief Get buffer strategy
    MYLANG_COMPILER_ABI MYLANG_NODISCARD BufferStrategy get_strategy() const MYLANG_NOEXCEPT;
    /// @brief Clear all caches and bookmarks
    MYLANG_COMPILER_ABI void clear_all_caches() MYLANG_NOEXCEPT;
    /// @brief Get memory usage estimate in bytes
    MYLANG_COMPILER_ABI MYLANG_NODISCARD std::size_t estimate_memory_usage() const MYLANG_NOEXCEPT;

    /// @brief Iterator for streaming file content
    class iterator
    {
       public:
        using iterator_category = std::input_iterator_tag;
        using value_type = char16_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const char16_t*;
        using reference = const char16_t&;

       private:
        FileManager* manager_{nullptr};
        std::optional<char16_t> current_;
        bool at_end_{false};

        void advance();

       public:
        iterator() = default;

        explicit iterator(FileManager* mgr, bool end = false);

        reference operator*() const;
        pointer operator->() const;

        iterator& operator++();
        iterator operator++(int);

        bool operator==(const iterator& other) const;
        bool operator!=(const iterator& other) const;
    };

    /// @brief Get iterator to beginning
    MYLANG_COMPILER_ABI MYLANG_NODISCARD iterator begin();
    /// @brief Get iterator to end
    MYLANG_COMPILER_ABI MYLANG_NODISCARD iterator end();
};