#ifndef FA_CHUNK_HPP
#define FA_CHUNK_HPP

#include "farray.hpp"
#include "fobject.hpp"
#include "fopcode.hpp"
#include "fstring.hpp"

namespace fairuz::runtime {

class Value; // forward

struct ClassDescriptor {
    StringRef name;
    StringRef parent_name;
    u32 field_count { 0 };
    Array<StringRef> field_names; // for runtime slot-map / debug info
    u32 vtable_size { 0 };
    Array<StringRef> method_names; // parallel to vtable_indices
    Array<u32> vtable_indices;     // indices into Chunk::functions[]
                                   // of the enclosing (top-level) chunk
    static constexpr u32 NULL_SLOT = UINT32_MAX;
};

struct Chunk {
    StringRef name { "" };
    std::string source_path;
    diagnostic::SourcePtr source;
    GlobalEnvironment* globals { nullptr };
    int arity { 0 };
    u32 local_count { 0 };

    // Empty after successful first-call compilation. Captures definition-time context.
    std::function<bool(Chunk*)> deferred_body;

    Array<u32> code;
    Array<SourceLocation> locations;
    Array<Value> constants;
    // Large literals are parsed once into normalized, non-GC limb constants.
    Array<integer::Data> big_ints;
    Array<LineEntry> lines;
    Array<Chunk*> functions;
    Array<ICSlot> ic_slots;
    Array<u64*> global_cache;
    Array<ClassDescriptor> class_descriptors; // new

    u16 add_class_descriptor(ClassDescriptor&& d)
    {
        class_descriptors.push(std::move(d));
        return static_cast<u16>(class_descriptors.size() - 1);
    }

    Chunk() = default;
    ~Chunk() = default;

    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = default;

    Chunk& operator=(Chunk const&) = delete;
    Chunk& operator=(Chunk&&) = default;

    u32 emit(u32 instr, SourceLocation loc);
    bool patch_jump(u32 const instr_idx);
    u16 add_constant(Value const v);
    u16 add_big_int(integer::Data const& v);
    u16 add_big_int(i64 v) { return add_big_int(integer::from_i64(v)); }
    u8 alloc_ic_slot();
    u32 get_line(u32 const instr_idx) const;
    void disassemble() const;
    void add_line(u32 line);
}; // struct Chunk

template<typename... Args>
static inline Chunk* make_chunk(Args&&... args) { return get_allocator().allocate_object<Chunk>(std::forward<Args>(args)...); }

} // namespace fairuz::runtime

#endif // FA_CHUNK_HPP
