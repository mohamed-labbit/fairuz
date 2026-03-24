#ifndef NARROW_TABLE_HPP
#define NARROW_TABLE_HPP

#include "array.hpp"

namespace mylang {

template<typename K, typename V, typename Hash, typename Equal>
class NarrowHashTable {
    struct Entry {
        K key { };
        V value { };
        uint64_t hash { 0 };
        bool occupied { false };
    };

    static constexpr uint32_t kMinCapacity = 16;
    static constexpr uint32_t kLoadPercent = 70;

    Array<Entry> Buckets_;
    uint32_t Size_ { 0 };
    Hash Hash_ { };
    Equal Equal_ { };

    static uint32_t nextPowerOfTwo(uint32_t n)
    {
        uint32_t cap = kMinCapacity;
        while (cap < n)
            cap <<= 1;
        return cap;
    }

    uint32_t mask() const
    {
        assert(!Buckets_.empty());
        return Buckets_.size() - 1;
    }

    uint32_t bucketIndex(uint64_t hash) const
    {
        return static_cast<uint32_t>(hash) & mask();
    }

    void reinsertInto(Array<Entry>& dst, Entry const& src)
    {
        uint32_t dst_mask = dst.size() - 1;
        uint32_t idx = static_cast<uint32_t>(src.hash) & dst_mask;
        while (dst[idx].occupied)
            idx = (idx + 1) & dst_mask;
        dst[idx] = src;
    }

    void growIfNeeded()
    {
        if (Buckets_.empty()) {
            Buckets_ = Array<Entry>(kMinCapacity, Entry { });
            return;
        }

        if ((Size_ + 1) * 100 < Buckets_.size() * kLoadPercent)
            return;

        Array<Entry> grown(nextPowerOfTwo(Buckets_.size() << 1), Entry { });
        for (uint32_t i = 0; i < Buckets_.size(); ++i) {
            if (Buckets_[i].occupied)
                reinsertInto(grown, Buckets_[i]);
        }
        Buckets_ = std::move(grown);
    }

    Entry* findEntry(K const& key, uint64_t hash)
    {
        if (Buckets_.empty())
            return nullptr;

        uint32_t idx = bucketIndex(hash);
        while (Buckets_[idx].occupied) {
            Entry& entry = Buckets_[idx];
            if (entry.hash == hash && Equal_(entry.key, key))
                return &entry;
            idx = (idx + 1) & mask();
        }
        return nullptr;
    }

    Entry const* findEntry(K const& key, uint64_t hash) const
    {
        if (Buckets_.empty())
            return nullptr;

        uint32_t idx = bucketIndex(hash);
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
        for (uint32_t i = 0; i < Buckets_.size(); ++i)
            Buckets_[i] = Entry { };
        Size_ = 0;
    }

    uint32_t size() const { return Size_; }
    bool empty() const { return Size_ == 0; }

    V* findPtr(K const& key)
    {
        uint64_t hash = static_cast<uint64_t>(Hash_(key));
        Entry* entry = findEntry(key, hash);
        return entry ? &entry->value : nullptr;
    }

    V const* findPtr(K const& key) const
    {
        uint64_t hash = static_cast<uint64_t>(Hash_(key));
        Entry const* entry = findEntry(key, hash);
        return entry ? &entry->value : nullptr;
    }

    bool contains(K const& key) const { return findPtr(key) != nullptr; }

    V& insertOrAssign(K const& key, V const& value)
    {
        growIfNeeded();

        uint64_t hash = static_cast<uint64_t>(Hash_(key));
        if (Entry* existing = findEntry(key, hash)) {
            existing->value = value;
            return existing->value;
        }

        uint32_t idx = bucketIndex(hash);
        while (Buckets_[idx].occupied)
            idx = (idx + 1) & mask();

        Buckets_[idx].occupied = true;
        Buckets_[idx].hash = hash;
        Buckets_[idx].key = key;
        Buckets_[idx].value = value;
        ++Size_;
        return Buckets_[idx].value;
    }
};

} // namespace mylang

#endif // NARROW_TABLE_HPP
