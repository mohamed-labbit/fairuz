#ifndef NARROW_TABLE_HPP
#define NARROW_TABLE_HPP

#include "array.hpp"
#include <initializer_list>

namespace fairuz {

template<typename K, typename V, typename Hash, typename Equal>
class Fa_HashTable {
    struct Entry {
        K key { };
        V m_value { };
        u64 m_hash { 0 };
        bool occupied { false };
    }; // struct Entry

    static constexpr u32 k_min_capacity = 16;
    static constexpr u32 k_load_percent = 70;

    Fa_Array<Entry> m_buckets;
    u32 m_size { 0 };
    Hash m_hash { };
    Equal m_equal { };

    static u32 next_power_of_two(u32 n)
    {
        u32 m_cap = k_min_capacity;
        while (m_cap < n)
            m_cap <<= 1;
        return m_cap;
    }

    u32 mask() const
    {
        assert(!m_buckets.empty());
        return m_buckets.size() - 1;
    }

    u32 bucket_index(u64 m_hash) const
    {
        return static_cast<u32>(m_hash) & mask();
    }

    void reinsert_into(Fa_Array<Entry>& dst, Entry const& src)
    {
        u32 dst_mask = dst.size() - 1;
        u32 idx = static_cast<u32>(src.m_hash) & dst_mask;
        while (dst[idx].occupied)
            idx = (idx + 1) & dst_mask;
        dst[idx] = src;
    }

    void grow_if_needed()
    {
        if (m_buckets.empty()) {
            m_buckets = Fa_Array<Entry>(k_min_capacity, Entry { });
            return;
        }

        if ((m_size + 1) * 100 < m_buckets.size() * k_load_percent)
            return;

        Fa_Array<Entry> grown(next_power_of_two(m_buckets.size() << 1), Entry { });
        for (u32 i = 0; i < m_buckets.size(); ++i) {
            if (m_buckets[i].occupied)
                reinsert_into(grown, m_buckets[i]);
        }
        m_buckets = std::move(grown);
    }

    Entry* find_entry(K const& key, u64 m_hash)
    {
        if (m_buckets.empty())
            return nullptr;

        u32 idx = bucket_index(m_hash);
        while (m_buckets[idx].occupied) {
            Entry& entry = m_buckets[idx];
            if (entry.m_hash == m_hash && m_equal(entry.key, key))
                return &entry;
            idx = (idx + 1) & mask();
        }
        return nullptr;
    }

    Entry const* find_entry(K const& key, u64 m_hash) const
    {
        if (m_buckets.empty())
            return nullptr;

        u32 idx = bucket_index(m_hash);
        while (m_buckets[idx].occupied) {
            Entry const& entry = m_buckets[idx];
            if (entry.m_hash == m_hash && m_equal(entry.key, key))
                return &entry;
            idx = (idx + 1) & mask();
        }
        return nullptr;
    }

public:
    Fa_HashTable() = default;
    
    explicit Fa_HashTable(std::initializer_list<std::pair<K, V>> list) 
    {    
        if (list.size() == 0)
            return;
        
        for (const std::pair<K, V>& pair : list)
            insert_or_assign(pair.first, pair.second);
    }
    
    ~Fa_HashTable() {}
    
    V& operator[](const K& key) { 
        auto hash_value = static_cast<u64>(m_hash(key));
        if (Entry* entry = find_entry(key, hash_value))
            return entry->m_value;
        return insert_or_assign(key, V{});
    }
    
    const V& operator[](const K& key) const = delete;

    void clear()
    {
        for (u32 i = 0; i < m_buckets.size(); ++i)
            m_buckets[i] = Entry { };
        m_size = 0;
    }

    u32 size() const { return m_size; }
    bool empty() const { return m_size == 0; }

    V* find_ptr(K const& key)
    {
        auto hash_value = static_cast<u64>(m_hash(key));
        Entry* entry = find_entry(key, hash_value);
        return entry ? &entry->m_value : nullptr;
    }

    V const* find_ptr(K const& key) const
    {
        auto hash_value = static_cast<u64>(m_hash(key));
        Entry const* entry = find_entry(key, hash_value);
        return entry ? &entry->m_value : nullptr;
    }

    bool contains(K const& key) const { return find_ptr(key) != nullptr; }

    V& insert_or_assign(K const& key, V const& m_value)
    {
        grow_if_needed();

        auto hash_value = static_cast<u64>(m_hash(key));
        if (Entry* existing = find_entry(key, hash_value)) {
            existing->m_value = m_value;
            return existing->m_value;
        }

        u32 idx = bucket_index(hash_value);
        while (m_buckets[idx].occupied)
            idx = (idx + 1) & mask();

        m_buckets[idx].occupied = true;
        m_buckets[idx].m_hash = hash_value;
        m_buckets[idx].key = key;
        m_buckets[idx].m_value = m_value;
        ++m_size;

        return m_buckets[idx].m_value;
    }
    
    struct Iterator {
        Entry* ptr;
        Entry* m_end;
    
        Iterator& operator++() {
            ++ptr;
            while (ptr != m_end && !ptr->occupied)
                ++ptr;
            return *this;
        }
    
        std::pair<K const&, V&> operator*() const { return { ptr->key, ptr->m_value }; }
        Entry* operator->() const { return ptr; }
        bool operator!=(const Iterator& other) const { return ptr != other.ptr; }
    };
    
    Iterator begin() {
        Entry* p = m_buckets.begin();
        Entry* e = m_buckets.end();
        while (p != e && !p->occupied) ++p;
        return { p, e };
    }
    
    Iterator end() {
        Entry* e = m_buckets.end();
        return { e, e };
    }
}; // class Fa_HashTable

} // namespace fairuz

#endif // NARROW_TABLE_HPP
