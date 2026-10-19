//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Defines bytecode chunks, constants, and source locations.
//===----------------------------------------------------------------------===//

#ifndef FAIRUZ_COMPILER_CHUNK_HPP
#define FAIRUZ_COMPILER_CHUNK_HPP

#include "Compiler/opcode.hpp"
#include "Diag/runtime_error.hpp"
#include "Interpreter/object.hpp"
#include "Macros.hpp"
#include "Support/array.hpp"
#include "Support/string.hpp"

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
    std::function<ErrorOr<void, RuntimeError>(Chunk*)> deferred_body;

    Array<InstrType> code;
    Array<SourceLocation> locations;
    Array<Value> constants;
    // Large literals are parsed once into normalized, non-GC limb constants.
    Array<integer::Data> big_ints;
    Array<LineEntry> lines;
    Array<Chunk*> functions;
    Array<ICSlot> ic_slots;
    Array<u64*> global_cache;
    Array<ClassDescriptor> class_descriptors; // new

    ClassIndex add_class_descriptor(ClassDescriptor&& d);
    FunctionIndex add_function(Chunk* function);

    Chunk() = default;
    ~Chunk() = default;

    Chunk(Chunk const&) = delete;
    Chunk(Chunk&&) = default;

    Chunk& operator=(Chunk const&) = delete;
    Chunk& operator=(Chunk&&) = default;

    PCType emit(InstrType instr, SourceLocation loc);
    bool patch_jump(PCType instr_idx);
    ConstantIndex add_constant(Value const v);
    BigIntIndex add_big_int(integer::Data const& v);
    BigIntIndex add_big_int(i64 v) { return add_big_int(integer::from_i64(v)); }
    CacheSlot alloc_ic_slot();
    u32 get_line(u32 const instr_idx) const;
    void disassemble() const;
    void add_line(u32 line);
}; // struct Chunk

template<typename... Args>
static inline Chunk* make_chunk(Args&&... args) { return get_allocator().allocate_object<Chunk>(std::forward<Args>(args)...); }

} // namespace fairuz::runtime

#endif // FAIRUZ_COMPILER_CHUNK_HPP
