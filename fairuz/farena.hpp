#ifndef FA_ARENA_HPP
#define FA_ARENA_HPP

#include "fmacros.hpp"

#include <functional>
#include <optional>
#include <sys/mman.h>
#include <type_traits>
#include <vector>

namespace fairuz {

class Fa_ArenaBlock {
private:
    size_t m_size { DEFAULT_BLOCK_SIZE };
    unsigned char* m_begin { nullptr };
    unsigned char* m_next { nullptr };
    unsigned char* m_end { nullptr };

public:
    explicit Fa_ArenaBlock(size_t const size = DEFAULT_BLOCK_SIZE, size_t const alignment = alignof(std::max_align_t));

    ~Fa_ArenaBlock()
    {
        if (m_begin != nullptr) {
            munmap(m_begin, m_size);
            m_begin = nullptr;
            m_next = nullptr;
            m_end = nullptr;
        }
    }

    // Non-copyable
    Fa_ArenaBlock(Fa_ArenaBlock const&) = delete;
    Fa_ArenaBlock& operator=(Fa_ArenaBlock const&) = delete;

    Fa_ArenaBlock(Fa_ArenaBlock&& other) noexcept
        : m_size(other.m_size)
        , m_begin(other.m_begin)
        , m_next(other.m_next)
        , m_end(other.m_end)
    {
        other.m_size = 0;
        other.m_begin = nullptr;
        other.m_next = nullptr;
        other.m_end = nullptr;
    }

    Fa_ArenaBlock& operator=(Fa_ArenaBlock&& other) noexcept;

    [[nodiscard]] unsigned char* begin() const { return m_begin; }
    [[nodiscard]] unsigned char* end() const { return m_end; }
    [[nodiscard]] unsigned char* next() const { return m_next; }
    [[nodiscard]] size_t size() const { return m_size; }

    [[nodiscard]] size_t used() const
    {
        return (m_begin == nullptr || m_next < m_begin) ? 0 : static_cast<size_t>(m_next - m_begin);
    }

    [[nodiscard]] size_t remaining() const
    {
        return (m_begin == nullptr) ? 0 : static_cast<size_t>(m_end - m_next);
    }

    bool pop(size_t bytes)
    {
        if (m_begin == nullptr || m_next < m_begin + bytes)
            return false;

        m_next -= bytes;
        return true;
    }

    [[nodiscard]] unsigned char* allocate(size_t bytes, std::optional<size_t> alignment = std::nullopt);

    unsigned char* reserve(size_t const bytes)
    {
        if ((m_begin == nullptr || bytes == 0) || static_cast<size_t>(m_end - m_next) < bytes)
            return nullptr;

        m_next += bytes;
        return m_next;
    }
}; // class Fa_ArenaBlock

class Fa_ArenaAllocator {
public:
    enum class GrowthStrategy : i32 {
        LINEAR
    }; // enum GrowthStrategy

    using OutOfMemoryHandler = std::function<bool(size_t requested)>;

private:
    struct DestructorRecord {
        void* object { nullptr };
        void (*destroy)(void*) { nullptr };
    };

    std::vector<Fa_ArenaBlock> m_blocks { };
    std::vector<DestructorRecord> m_destructors { };
    size_t m_block_size { DEFAULT_BLOCK_SIZE };
    size_t m_next_block_size { DEFAULT_BLOCK_SIZE };
    std::string m_name { "arena" };
    OutOfMemoryHandler m_oom_handler { nullptr };
    size_t m_max_block_size { MAX_BLOCK_SIZE };
    void* m_last_ptr { nullptr };
    size_t m_last_size { 0 };
    size_t m_last_consumed { 0 };
    unsigned char* m_next { nullptr };
    unsigned char* m_end { nullptr };

    static constexpr size_t ALIGNMENT = alignof(std::max_align_t);

public:
    explicit Fa_ArenaAllocator(OutOfMemoryHandler oom_handler = nullptr)
        : m_oom_handler(oom_handler)
    {
    }

    ~Fa_ArenaAllocator()
    {
        destroy_objects();
        m_blocks.clear();
    }

    Fa_ArenaAllocator(Fa_ArenaAllocator const&) = delete;
    Fa_ArenaAllocator& operator=(Fa_ArenaAllocator const&) = delete;

    Fa_ArenaAllocator(Fa_ArenaAllocator&&) noexcept = delete;
    Fa_ArenaAllocator& operator=(Fa_ArenaAllocator&&) noexcept = delete;

    void set_name(std::string const& name) { m_name = name; }

    void reset()
    {
        destroy_objects();
        m_blocks.clear();
        m_last_ptr = nullptr;
        m_last_size = 0;
        m_last_consumed = 0;
        m_next = nullptr;
        m_end = nullptr;
        m_next_block_size = m_block_size;

        allocate_block(m_next_block_size, alignof(std::max_align_t));
    }

    unsigned char* allocate_block(size_t requested, size_t alignment = alignof(std::max_align_t), bool retry_on_oom = true);

    [[nodiscard]] void* allocate(size_t const size, size_t const alignment = alignof(std::max_align_t));

    void deallocate(void* ptr, size_t const size);

    template<typename T>
    [[nodiscard]] T* allocate_array(size_t const count)
    {
        return static_cast<T*>(allocate(count * sizeof(T)));
    }

    template<typename T>
    void deallocate_array(T* ptr, size_t const count) { deallocate(static_cast<void*>(ptr), count * sizeof(T)); }

    template<typename T, typename... Args>
    [[nodiscard]] T* allocate_object(Args&&... args)
    {
        static_assert(std::is_constructible_v<T, Args...>, "T must be constructible with Args...");
        T* object = ::new (allocate(sizeof(T))) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            try {
                m_destructors.push_back({ object, [](void* pointer) {
                    static_cast<T*>(pointer)->~T();
                } });
            } catch (...) {
                object->~T();
                throw;
            }
        }
        return object;
    }

    template<typename T>
    void deallocate_object(T* obj)
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
            unregister_destructor(obj);
        deallocate(static_cast<void*>(obj), sizeof(T));
    }

private:
    void destroy_objects()
    {
        // Pop before invoking so a destructor may safely release another
        // arena object and unregister its pending callback.
        while (!m_destructors.empty()) {
            DestructorRecord record = m_destructors.back();
            m_destructors.pop_back();
            record.destroy(record.object);
        }
    }

    void unregister_destructor(void* object)
    {
        for (size_t i = m_destructors.size(); i > 0; --i) {
            if (m_destructors[i - 1].object == object) {
                m_destructors.erase(m_destructors.begin() + static_cast<ptrdiff_t>(i - 1));
                return;
            }
        }
    }

    void* allocate_slow(size_t size, size_t alignment);

    [[nodiscard]] unsigned char* allocate_from_blocks(size_t alloc_size, size_t align = alignof(std::max_align_t));

    [[nodiscard]] static constexpr size_t get_aligned(size_t n, size_t const alignment = alignof(std::max_align_t)) noexcept
    {
        return (n + alignment - 1) & ~(alignment - 1);
    }
}; // class Fa_ArenaAllocator

struct Fa_AllocatorContext {
    Fa_ArenaAllocator allocator { nullptr };
}; // struct Fa_AllocatorContext

inline Fa_AllocatorContext* g_context = nullptr;

inline void set_context(Fa_AllocatorContext* ctx) { g_context = ctx; }

inline Fa_AllocatorContext& get_context()
{
    if (UNLIKELY(!g_context)) {
        static Fa_AllocatorContext default_ctx;
        g_context = &default_ctx;
    }
    return *g_context;
}

inline Fa_ArenaAllocator& get_allocator() { return get_context().allocator; }
inline Fa_ArenaAllocator* get_allocator_ptr() { return &get_context().allocator; }

struct Fa_AllocatorContextScope {
    explicit Fa_AllocatorContextScope(Fa_AllocatorContext& ctx) { g_context = &ctx; }
    ~Fa_AllocatorContextScope() { g_context = nullptr; }

    Fa_AllocatorContextScope(Fa_AllocatorContextScope const&) = delete;
    Fa_AllocatorContextScope& operator=(Fa_AllocatorContextScope const&) = delete;
}; // struct Fa_AllocatorContextScope

} // namespace fairuz

#endif // FA_ARENA_HPP
