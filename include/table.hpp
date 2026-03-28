#ifndef NARROW_TABLE_HPP
#define NARROW_TABLE_HPP

#include "array.hpp"

namespace fairuz {

template<typename K, typename V, typename Hash, typename Equal>
class NarrowHashTable {
    struct Entry {
        K key { };
        V value { };
        u64 hash { 0 };
        bool occupied { false };
    }; // struct Entry

    static constexpr u32 kMinCapacity = 16;
    static constexpr u32 kLoadPercent = 70;

    Fa_Array<Entry> Buckets_;
    u32 Size_ { 0 };
    Hash Hash_ { };
    Equal Equal_ { };

    static u32 nextPowerOfTwo(u32 n)
    {
        u32 cap = kMinCapacity;
        while (cap < n)
            cap <<= 1;
        return cap;
    }

    u32 mask() const
    {
        assert(!Buckets_.empty());
        return Buckets_.size() - 1;
    }

    u32 bucketIndex(u64 hash) const
    {
        return static_cast<u32>(hash) & mask();
    }

    void reinsertInto(Fa_Array<Entry>& dst, Entry const& src)
    {
        u32 dst_mask = dst.size() - 1;
        u32 idx = static_cast<u32>(src.hash) & dst_mask;
        while (dst[idx].occupied)
            idx = (idx + 1) & dst_mask;
        dst[idx] = src;
    }

    void growIfNeeded()
    {
        if (Buckets_.empty()) {
            Buckets_ = Fa_Array<Entry>(kMinCapacity, Entry { });
            return;
        }

        if ((Size_ + 1) * 100 < Buckets_.size() * kLoadPercent)
            return;

        Fa_Array<Entry> grown(nextPowerOfTwo(Buckets_.size() << 1), Entry { });
        for (u32 i = 0; i < Buckets_.size(); ++i) {
            if (Buckets_[i].occupied)
                reinsertInto(grown, Buckets_[i]);
        }
        Buckets_ = std::move(grown);
    }

    Entry* findEntry(K const& key, u64 hash)
    {
        if (Buckets_.empty())
            return nullptr;

        u32 idx = bucketIndex(hash);
        while (Buckets_[idx].occupied) {
            Entry& entry = Buckets_[idx];
            if (entry.hash == hash && Equal_(entry.key, key))
                return &entry;
            idx = (idx + 1) & mask();
        }
        return nullptr;
    }

    Entry const* findEntry(K const& key, u64 hash) const
    {
        if (Buckets_.empty())
            return nullptr;

        u32 idx = bucketIndex(hash);
        while (Buckets_[idx].occupied) {
            Entry const& entry = Buckets_[idx];
            if (entry.hash == hash && Equal_(entry.key, key))
                return &entry;
            idx = (idx + 1) & mask();
        }
        return nullptr;
    }

public:
    NarrowHashTable() = default;

    void clear()
    {
        for (u32 i = 0; i < Buckets_.size(); ++i)
            Buckets_[i] = Entry { };
        Size_ = 0;
    }

    u32 size() const { return Size_; }
    bool empty() const { return Size_ == 0; }

    V* findPtr(K const& key)
    {
        auto hash = static_cast<u64>(Hash_(key));
        Entry* entry = findEntry(key, hash);
        return entry ? &entry->value : nullptr;
    }

    V const* findPtr(K const& key) const
    {
        auto hash = static_cast<u64>(Hash_(key));
        Entry const* entry = findEntry(key, hash);
        return entry ? &entry->value : nullptr;
    }

    bool contains(K const& key) const { return findPtr(key) != nullptr; }

    V& insertOrAssign(K const& key, V const& value)
    {
        growIfNeeded();

        auto hash = static_cast<u64>(Hash_(key));
        if (Entry* existing = findEntry(key, hash)) {
            existing->value = value;
            return existing->value;
        }

        u32 idx = bucketIndex(hash);
        while (Buckets_[idx].occupied)
            idx = (idx + 1) & mask();

        Buckets_[idx].occupied = true;
        Buckets_[idx].hash = hash;
        Buckets_[idx].key = key;
        Buckets_[idx].value = value;
        ++Size_;

        return Buckets_[idx].value;
    }
}; // class NarrowHashTable

} // namespace fairuz

#endif // NARROW_TABLE_HPP
