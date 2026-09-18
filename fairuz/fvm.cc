//
// vm.cc
//

#include "fvm.hpp"
#include "fcompiler.hpp"
#include "fdiagnostic.hpp"
#include "flexer.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "fopcode.hpp"
#include "fparser.hpp"
#include "fstring.hpp"
#include "futil.hpp"
#include "fvalue.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <system_error>

namespace fairuz::runtime {

namespace {

using ComparedObjects = std::set<std::pair<ObjHeader const*, ObjHeader const*>>;

char const* value_type_name(Value value)
{
    if (value.is_nil())
        return "عدم";
    if (value.is_bool())
        return "منطقي";
    if (value.is_int())
        return "طبيعي";
    if (value.is_double())
        return "حقيقي";
    if (value.is_string())
        return "سلسلة";
    if (value.is_list())
        return "قائمة";
    if (value.is_dict())
        return "قاموس";
    if (value.is_function() || value.is_native())
        return "دالة";
    if (value.is_class())
        return "نوع";
    if (value.is_instance())
        return "كائن";
    if (value.is_module())
        return "وحدة";
    return "مورد";
}

std::string similar_name(GlobalEnvironment* environment, std::string const& missing)
{
    auto points = [](StringRef const& name) {
        std::vector<u32> result;
        for (size_t i = 0; i < name.len() && result.size() < 65;) {
            u64 bytes = 0;
            result.push_back(util::decode_utf8_at(name, i, &bytes));
            i += bytes;
        }
        return result;
    };
    auto wanted = points(StringRef(missing.c_str()));
    if (wanted.size() < 3 || wanted.size() > 64)
        return { };
    size_t best = wanted.size() < 6 ? 2 : 3;
    std::string match;
    size_t examined = 0;
    for (auto* env = environment; env && examined < 1000; env = env->fallback) {
        for (auto const& [name, slot] : env->index) {
            if (++examined > 1000)
                break;
            auto candidate = points(name);
            if (candidate.size() > 64 || candidate.size() + best < wanted.size() || wanted.size() + best < candidate.size())
                continue;
            std::vector<size_t> previous(candidate.size() + 1), current(candidate.size() + 1);
            for (size_t j = 0; j <= candidate.size(); ++j)
                previous[j] = j;
            for (size_t i = 1; i <= wanted.size(); ++i) {
                current[0] = i;
                for (size_t j = 1; j <= candidate.size(); ++j)
                    current[j] = std::min({ previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (wanted[i - 1] != candidate[j - 1]) });
                previous.swap(current);
            }
            std::string text(name.data(), name.len());
            if (previous.back() < best || (previous.back() == best && !match.empty() && text < match)) {
                best = previous.back();
                match = std::move(text);
            }
        }
    }
    return match;
}

bool values_equal_impl(Value lhs, Value rhs, ComparedObjects& seen)
{
    if (lhs == rhs)
        return true;
    if (lhs.is_number() && rhs.is_number())
        return lhs.as_double_any() == rhs.as_double_any();
    if (lhs.is_nil() || rhs.is_nil() || lhs.is_bool() || rhs.is_bool())
        return false;
    if (lhs.is_string() && rhs.is_string())
        return lhs.as_string()->str == rhs.as_string()->str;
    if (!lhs.is_obj() || !rhs.is_obj() || lhs.as_obj()->type != rhs.as_obj()->type)
        return false;

    auto pair = std::make_pair(static_cast<ObjHeader const*>(lhs.as_obj()),
        static_cast<ObjHeader const*>(rhs.as_obj()));
    if (!seen.insert(pair).second)
        return true;

    if (lhs.is_list()) {
        ObjList* left = lhs.as_list();
        ObjList* right = rhs.as_list();
        if (left->elements.size() != right->elements.size())
            return false;
        for (u32 i = 0; i < left->elements.size(); ++i) {
            if (!values_equal_impl(left->elements[i], right->elements[i], seen))
                return false;
        }
        return true;
    }
    if (lhs.is_dict()) {
        ObjDict* left = lhs.as_dict();
        ObjDict* right = rhs.as_dict();
        if (left->data.size() != right->data.size())
            return false;
        for (auto const& [key, value] : left->data) {
            Value const* other = right->data.find_ptr(key);
            if (other == nullptr || !values_equal_impl(value, *other, seen))
                return false;
        }
        return true;
    }
    return false; // distinct functions, classes, instances, modules, and resources compare by identity
}

bool values_equal(Value lhs, Value rhs)
{
    ComparedObjects seen;
    return values_equal_impl(lhs, rhs, seen);
}

} // namespace

#define DISPATCH()                                                                       \
    do {                                                                                 \
        if (UNLIKELY(m_gc.should_collect()))                                             \
            m_gc.collect(this);                                                          \
        if (UNLIKELY(ip >= cur_chunk->code.size()))                                      \
            raise_error(ErrorCode::INVALID_OPCODE, "instruction pointer out of bounds"); \
        instr = cur_chunk->code[ip];                                                     \
        ip++;                                                                            \
        SAVE_IP();                                                                       \
        u8 opcode = static_cast<u8>(instr_op(instr));                                    \
        if (UNLIKELY(opcode >= static_cast<u8>(OpCode::_COUNT)))                         \
            raise_error(ErrorCode::INVALID_OPCODE);                                      \
        goto* dispatch_table[opcode];                                                    \
    } while (0)

#define BEGIN_DISPATCH() DISPATCH()
#define END_DISPATCH()
#define CASE(op) H_##op:

#define VMOPI(lhs, rhs, op) lhs.as_int() op rhs.as_int()
#define VMOPF(lhs, rhs, op) lhs.as_double_any() op rhs.as_double_any()

#define VM_ADDI(lhs, rhs) VMOPI(lhs, rhs, +)
#define VM_SUBI(lhs, rhs) VMOPI(lhs, rhs, -)
#define VM_MULI(lhs, rhs) VMOPI(lhs, rhs, *)
#define VM_DIVI(lhs, rhs) VMOPI(lhs, rhs, /)
#define VM_ADDF(lhs, rhs) VMOPF(lhs, rhs, +)
#define VM_SUBF(lhs, rhs) VMOPF(lhs, rhs, -)
#define VM_MULF(lhs, rhs) VMOPF(lhs, rhs, *)
#define VM_DIVF(lhs, rhs) VMOPF(lhs, rhs, /)
#define VM_INSTANCE_OP(op_name)                                                \
    do {                                                                       \
        ObjClass* self_klass = nullptr;                                        \
        Value self_val, arg_val = Value::nil();                                \
        int slot = -1;                                                         \
        if (lhs.is_instance()) {                                               \
            self_klass = lhs.as_instance()->klass;                             \
            slot = self_klass->method_slot(sp_method_name(ObjClass::op_name)); \
            if (slot >= 0) {                                                   \
                self_val = lhs;                                                \
                arg_val = rhs;                                                 \
            }                                                                  \
        }                                                                      \
        if (slot < 0 && rhs.is_instance()) {                                   \
            self_klass = rhs.as_instance()->klass;                             \
            slot = self_klass->method_slot(sp_method_name(ObjClass::op_name)); \
            if (slot >= 0) {                                                   \
                self_val = rhs;                                                \
                arg_val = lhs;                                                 \
            }                                                                  \
        }                                                                      \
        if (UNLIKELY(slot < 0))                                                \
            raise_error(ErrorCode::TYPE_ERROR_ARITH);                          \
        if (UNLIKELY(static_cast<u32>(slot) >= self_klass->vtable.size()))     \
            raise_error(ErrorCode::UNDEFINED_METHOD);                          \
        Chunk* target_chunk = self_klass->vtable[static_cast<u32>(slot)];      \
        int caller_stack_top = m_stack_top;                                    \
        int call_base = caller_stack_top;                                      \
        if (UNLIKELY(call_base + 2 >= STACK_SIZE))                             \
            raise_error(ErrorCode::STACK_OVERFLOW);                            \
        m_stack[call_base + 1] = arg_val;                                      \
        invoke_method(target_chunk, self_val, cur_frame_base + instr_A(instr), \
            call_base, 2, ip, caller_stack_top, target_chunk->globals);        \
    } while (0)

#define RA() cur_base[instr_A(instr)]
#define RB() cur_base[instr_B(instr)]
#define RC() cur_base[instr_C(instr)]

#define LOAD_FRAME()                 \
    do {                             \
        CallFrame& f = frame();      \
        cur_chunk = f.chunk;         \
        cur_frame_base = f.base;     \
        cur_base = &m_stack[f.base]; \
        ip = f.ip;                   \
    } while (0)

#define SAVE_IP()        \
    do {                 \
        frame().ip = ip; \
    } while (0)

#define RECORD_BINARY_IC(lhs, rhs, result)                     \
    do {                                                       \
        if (ip < cur_chunk->code.size()                        \
            && instr_op(cur_chunk->code[ip]) == OpCode::NOP)   \
            update_ic_binary(cur_chunk, ip, lhs, rhs, result); \
    } while (0)

#define REQUIRE_NUMBER(v)                             \
    do {                                              \
        if (!v.is_number())                           \
            raise_error(ErrorCode::TYPE_ERROR_ARITH); \
    } while (0)

StringRef sp_method_name(int m)
{
    switch (m) {
    case ObjClass::INIT: return "بداية";
    case ObjClass::CALL: return "نداء";
    case ObjClass::ADD: return "عملية+";
    case ObjClass::SUB: return "عملية-";
    case ObjClass::MUL: return "عملية*";
    case ObjClass::DIV: return "عملية/";
    case ObjClass::MOD: return "عملية%";
    case ObjClass::NEG: return "سالب";
    case ObjClass::EQ: return "يساوي";
    case ObjClass::NEQ: return "لا_يساوي";
    case ObjClass::LT: return "اصغر_من";
    case ObjClass::LTE: return "اصغر_او_يساوي";
    case ObjClass::GT: return "اكبر_من";
    case ObjClass::GTE: return "اكبر_او_يساوي";
    case ObjClass::REPR: return "كتابة";
    default:
        return { };
    }
}

static void check_stack_index(int index, int stack_size, char const* m_context)
{
    if (index < 0 || index >= stack_size)
        // This is a VM internal error, not a user error.
        diagnostic::panic(ErrorCode::INTERNAL_ERROR,
            std::string(" : ") + std::string("VM internal error: stack index ") + std::to_string(index)
                + " out of range [0," + std::to_string(stack_size) + ") in " + m_context);
}

VM::VM()
{
    diagnostic::reset();

    std::fill(m_stack, m_stack + STACK_SIZE, Value::nil());
    std::fill(m_frames, m_frames + MAX_FRAMES, CallFrame());

    m_root_environment.fallback = &m_builtin_environment;

    open_stdlib();
}

VM::~VM()
{
    m_gc.sweep_all();
}

#define PUSH_VALUE(v)                               \
    do {                                            \
        if (m_stack_top == STACK_SIZE)              \
            raise_error(ErrorCode::STACK_OVERFLOW); \
        m_stack[m_stack_top] = v;                   \
        m_stack_top++;                              \
    } while (0);

// Ensure the value stack has at least `needed` slots allocated, filling
// any new slots with NIL.  Checks against the hard cap.
void VM::ensure_stack_slots(int needed)
{
    if (needed > STACK_SIZE)
        raise_error(ErrorCode::STACK_OVERFLOW);

    while (m_stack_top < needed)
        PUSH_VALUE(Value::nil());
}

// Reference to the topmost call frame.
CallFrame& VM::top_frame()
{
    assert(m_frames_top > 0 && "topFrame called with empty frame stack");
    return m_frames[m_frames_top - 1];
}

CallFrame const& VM::top_frame() const
{
    assert(m_frames_top > 0 && "topFrame called with empty frame stack");
    return m_frames[m_frames_top - 1];
}

Value& VM::get_reg(CallFrame const& f, int reg)
{
    int abs = f.base + reg;
    check_stack_index(abs, STACK_SIZE, "getReg");
    return m_stack[abs];
}

GlobalEnvironment* VM::current_globals()
{
    if (m_frames_top == 0 || frame().globals == nullptr)
        return &m_root_environment;
    return frame().globals;
}

Value const* VM::find_global(GlobalEnvironment* env, StringRef const& name) const
{
    if (env == nullptr)
        env = const_cast<GlobalEnvironment*>(&m_root_environment);
    return env->find(name);
}

void VM::store_global(GlobalEnvironment* env, StringRef const& name, Value value)
{
    if (env == nullptr)
        env = &m_root_environment;
    if (u32* slot = env->index.find_ptr(name)) {
        env->slots[*slot] = value;
        return;
    }
    u32 slot = env->slots.size();
    env->slots.push(value);
    env->index.insert_or_assign(name, slot);
}

std::filesystem::path VM::resolve_module_path(std::string const& name) const
{
    std::filesystem::path relative;
    size_t begin = 0;
    while (begin < name.size()) {
        size_t dot = name.find('.', begin);
        relative /= name.substr(begin, dot == std::string::npos ? std::string::npos : dot - begin);
        if (dot == std::string::npos)
            break;
        begin = dot + 1;
    }
    relative += ".ف";

    std::vector<std::filesystem::path> roots;
    // Standard-library roots are authoritative for module names they contain.
    // This prevents a project file such as `file.ف` from accidentally
    // shadowing the bundled module. Names absent from these roots still fall
    // through to the importing module's directory for normal relative imports.
    if (char const* configured = std::getenv("FAIRUZ_STDLIB"))
        roots.emplace_back(configured);
#ifdef FAIRUZ_SOURCE_STDLIB_DIR
    roots.emplace_back(FAIRUZ_SOURCE_STDLIB_DIR);
#endif
#ifdef FAIRUZ_INSTALL_STDLIB_DIR
    roots.emplace_back(FAIRUZ_INSTALL_STDLIB_DIR);
#endif
    if (m_frames_top > 0 && !frame().chunk->source_path.empty())
        roots.push_back(std::filesystem::path(frame().chunk->source_path).parent_path());
    roots.push_back(std::filesystem::current_path());

    std::error_code ec;
    for (auto const& root : roots) {
        std::filesystem::path candidate = root / relative;
        if (!std::filesystem::is_regular_file(candidate, ec)) {
            ec.clear();
            continue;
        }
        auto canonical = std::filesystem::weakly_canonical(candidate, ec);
        return ec ? candidate.lexically_normal() : canonical;
    }
    return { };
}

ObjModule* VM::load_module(std::string const& name)
{
    std::filesystem::path path = resolve_module_path(name);
    if (path.empty())
        raise_error(ErrorCode::MODULE_NOT_FOUND, "'" + name + "'");
    std::string key = path.string();
    if (auto found = m_module_cache.find(key); found != m_module_cache.end())
        return found->second;

    auto globals = std::make_unique<GlobalEnvironment>();
    globals->fallback = &m_builtin_environment;
    GlobalEnvironment* globals_ptr = globals.get();
    m_module_environments.push_back(std::move(globals));

    ObjModule* module = m_gc.make_obj_module(name, key, globals_ptr);
    m_module_cache.emplace(key, module); // publish first so import cycles terminate

    auto source = std::make_unique<lex::FileManager>(key);
    diagnostic::SourceScope source_scope(source.get());
    parser::Parser parser(source.get());
    Array<AST::Stmt*> statements = parser.parse_program();
    if (diagnostic::has_errors())
        halt();

    Compiler compiler;
    Chunk* imported_chunk = compiler.compile(statements);
    if (imported_chunk == nullptr || diagnostic::has_errors())
        halt();
    imported_chunk->source_path = key;
    imported_chunk->name = StringRef(module->name.data(), module->name.size());
    module->chunk = imported_chunk;
    m_module_sources.push_back(std::move(source));
    return module;
}

Value VM::run(Chunk* chunk)
{
    if (chunk == nullptr)
        return Value::nil();

    diagnostic::reset();
    diagnostic::SourceScope source_scope(chunk->source);
    m_stack_top = 0;
    m_frames_top = 0;

    ObjFunction* fn = m_gc.make_obj_function(chunk, &m_root_environment);

    m_stack[0] = Value::from_obj(reinterpret_cast<ObjHeader*>(fn));
    m_stack_top = static_cast<int>(chunk->local_count) + 2;

    if (m_frames_top >= MAX_FRAMES || m_stack_top >= STACK_SIZE)
        raise_error(ErrorCode::STACK_OVERFLOW);

    m_frames[m_frames_top] = CallFrame(fn, chunk, 0, 1,
        static_cast<u16>(chunk->local_count), 0, 0, &m_root_environment);
    m_frames_top++;
    intern_chunk_constants(fn->chunk);

    if (m_gc.current_memory() >= GC_THRESHOLD)
        m_gc.collect(this);

    try {
        return execute();
    } catch (...) {
        unwind_failed_run();
        throw;
    }
}

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wpedantic"
#endif

Value VM::execute(int stop_frame_depth)
{
    static void* dispatch_table[] = {
#define X(name) &&H_##name,
        FA_OPCODE_LIST(X)
#undef X
    };

    if (m_frames_top == 0)
        return Value::nil();

    u32 instr;
    Chunk* cur_chunk = frame().chunk;
    int cur_frame_base = frame().base;
    Value* cur_base = &m_stack[cur_frame_base];
    u32 ip = frame().ip;

    using reg_t = u8;
    auto checked_int = [this](__int128 value) -> Value {
        if (value < static_cast<__int128>(Value::int_min())
            || value > static_cast<__int128>(Value::int_max()))
            raise_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
        return Value::from_int(static_cast<i64>(value));
    };

    BEGIN_DISPATCH();

    CASE(LOAD_NIL)
    {
        reg_t start = instr_B(instr);
        reg_t count = instr_C(instr);

        for (u8 i = 0; i < count; i++)
            cur_base[start + i] = Value::nil();

        DISPATCH();
    }
    CASE(LOAD_TRUE)
    {
        RA() = Value::from_bool(true);
        DISPATCH();
    }
    CASE(LOAD_FALSE)
    {
        RA() = Value::from_bool(false);
        DISPATCH();
    }
    CASE(LOAD_CONST)
    {
        u16 index = instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size()))
            raise_error(ErrorCode::INVALID_OPCODE, "constant index out of bounds");
        RA() = cur_chunk->constants[index];
        DISPATCH();
    }
    CASE(LOAD_INT)
    {
        Value& ret = RA();
        /// TODO: create a ObjInt if using nanboxing and v is larger than 48 bits
        ret = Value::from_int(static_cast<i32>(static_cast<u16>(instr_Bx(instr))) - JUMP_OFFSET);
        DISPATCH();
    }
    CASE(LOAD_GLOBAL)
    {
        {
            u16 index = instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size()
                    || !cur_chunk->constants[index].is_string()))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid global-name constant");
            Value name_v = cur_chunk->constants[index];
            StringRef name = name_v.as_string()->str;
            Value const* value = find_global(current_globals(), name);
            if (value == nullptr)
                raise_error(ErrorCode::UNDEFINED_GLOBAL, std::string(name.data(), name.len()));
            RA() = *value;
        }
        DISPATCH();
    }
    CASE(LOAD_GLOBAL_CACHED)
    {
        u32 slot_idx = instr_Bx(instr);
        GlobalEnvironment* env = current_globals();
        if (UNLIKELY(env == nullptr || slot_idx >= env->slots.size()))
            raise_error(ErrorCode::UNDEFINED_GLOBAL);
        RA() = env->slots[slot_idx];
        DISPATCH();
    }
    CASE(STORE_GLOBAL)
    {
        {
            u16 index = instr_Bx(instr);
            if (UNLIKELY(index >= cur_chunk->constants.size()
                    || !cur_chunk->constants[index].is_string()))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid global-name constant");
            Value name_v = cur_chunk->constants[index];
            StringRef name = name_v.as_string()->str;
            store_global(current_globals(), name, RA());
        }
        DISPATCH();
    }
    CASE(STORE_GLOBAL_CACHED)
    {
        u32 slot_idx = instr_Bx(instr);
        GlobalEnvironment* env = current_globals();
        if (UNLIKELY(env == nullptr || slot_idx >= env->slots.size()))
            raise_error(ErrorCode::UNDEFINED_GLOBAL);
        env->slots[slot_idx] = RA();
        DISPATCH();
    }
    CASE(IMPORT_MODULE)
    {
        u16 index = instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size() || !cur_chunk->constants[index].is_string()))
            raise_error(ErrorCode::INVALID_OPCODE, "invalid module-name constant");
        StringRef const& module_name = cur_chunk->constants[index].as_string()->str;
        ObjModule* module = load_module(std::string(module_name.data(), module_name.len()));
        RA() = Value::from_module(module);

        if (module->initialized || module->executing) {
            DISPATCH();
        }

        module->executing = true;
        if (UNLIKELY(module->chunk == nullptr || m_frames_top >= MAX_FRAMES))
            raise_error(ErrorCode::INVALID_OPCODE, "invalid imported module");
        int result_slot = cur_frame_base + instr_A(instr);
        int module_base = m_stack_top;
        int new_top = module_base + static_cast<int>(module->chunk->local_count) + 1;
        if (UNLIKELY(new_top > STACK_SIZE))
            raise_error(ErrorCode::STACK_OVERFLOW);
        while (m_stack_top < new_top)
            m_stack[m_stack_top++] = Value::nil();
        ObjFunction* module_fn = m_gc.make_obj_function(module->chunk, module->globals);
        SAVE_IP();
        m_frames[m_frames_top++] = CallFrame(module_fn, module->chunk, 0,
            static_cast<u16>(module_base), static_cast<u16>(module->chunk->local_count),
            static_cast<u16>(result_slot), static_cast<u16>(module_base), module->globals, module);
        intern_chunk_constants(module->chunk);
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(MOVE)
    {
        RA() = RB();
        DISPATCH();
    }
    CASE(OP_ADD)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();
        bool both_str = lhs.is_string() && rhs.is_string();

        if (both_str) {
            // String concatenation
            res = m_gc.make_string(lhs.as_string()->str + rhs.as_string()->str);
            RECORD_BINARY_IC(lhs, rhs, res);
            // VM_ABC(OpCode::OP_ADD_SS);
        } else if (lhs.is_int() && rhs.is_int()) {
            // Integer addition
            res = checked_int(static_cast<__int128>(lhs.as_int()) + rhs.as_int());
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            // Float addition (includes mixed int/float)
            res = Value::from_real(VM_ADDF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(ADD);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_SUB)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = checked_int(static_cast<__int128>(lhs.as_int()) - rhs.as_int());
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_real(VM_SUBF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(SUB);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_MUL)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = checked_int(static_cast<__int128>(lhs.as_int()) * rhs.as_int());
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_real(VM_MULF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(MUL);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_DIV)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            if (rhs.as_int() == 0)
                raise_error(ErrorCode::DIVISION_BY_ZERO);
            if (lhs.as_int() == Value::int_min() && rhs.as_int() == -1)
                raise_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
            if (lhs.as_int() % rhs.as_int() == INT64_C(0))
                res = Value::from_int(VM_DIVI(lhs, rhs));
            else
                res = Value::from_real(VM_DIVF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            if (rhs.as_double_any() == 0.0)
                raise_error(ErrorCode::DIVISION_BY_ZERO);
            res = Value::from_real(VM_DIVF(lhs, rhs));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(DIV);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_MOD)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            if (rhs.as_int() == 0)
                raise_error(ErrorCode::MODULO_BY_ZERO);
            // Preserve integer type for integer modulo, matching arithmetic
            // closure and making the result valid for indexing/ranges.
            res = Value::from_int(VMOPI(lhs, rhs, %));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            if (rhs.as_double_any() == 0.0)
                raise_error(ErrorCode::MODULO_BY_ZERO);
            res = Value::from_real(std::fmod(lhs.as_double_any(), rhs.as_double_any()));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(MOD);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_POW)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        REQUIRE_NUMBER(lhs);
        REQUIRE_NUMBER(rhs);

        res = Value::from_real(std::pow(lhs.as_double_any(), rhs.as_double_any()));
        DISPATCH();
    }
    CASE(OP_NEG)
    {
        Value& res = RA();
        Value operand = RB();

        if (operand.is_int()) {
            res = checked_int(-static_cast<__int128>(operand.as_int()));
        } else if (operand.is_double()) {
            res = Value::from_real(-operand.as_double_any());
        } else if (operand.is_instance()) {
            ObjInstance* instance = operand.as_instance();
            ObjClass* self_klass = instance->klass;

            int slot = self_klass->method_slot(sp_method_name(ObjClass::NEG));
            if (UNLIKELY(slot < 0))
                raise_error(ErrorCode::UNDEFINED_METHOD);

            if (UNLIKELY(static_cast<u32>(slot) >= self_klass->vtable.size()))
                raise_error(ErrorCode::UNDEFINED_METHOD);
            Chunk* target_chunk = self_klass->vtable[static_cast<u32>(slot)];
            int caller_stack_top = m_stack_top;
            int call_base = caller_stack_top;

            if (UNLIKELY(call_base + 1 >= STACK_SIZE))
                raise_error(ErrorCode::STACK_OVERFLOW);
            invoke_method(target_chunk, operand, cur_frame_base + instr_A(instr),
                call_base, 1, ip, caller_stack_top, target_chunk->globals);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_ARITH);
        }

        DISPATCH();
    }
    CASE(OP_BITAND)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int()))
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Value::from_int(VMOPI(lhs, rhs, &));
        DISPATCH();
    }
    CASE(OP_BITOR)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int()))
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Value::from_int(VMOPI(lhs, rhs, |));
        DISPATCH();
    }
    CASE(OP_BITXOR)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (UNLIKELY(!lhs.is_int() || !rhs.is_int()))
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Value::from_int(VMOPI(lhs, rhs, ^));
        DISPATCH();
    }
    CASE(OP_BITNOT)
    {
        Value& res = RA();
        Value operand = RB();
        if (UNLIKELY(!operand.is_int()))
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        res = Value::from_int(~operand.as_int());
        DISPATCH();
    }
    CASE(OP_LSHIFT)
    {
        Value& res = RA();
        Value lhs = RB();
        if (UNLIKELY(!lhs.is_int()))
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        reg_t imm = instr_C(instr);
        if (imm >= 64)
            raise_error(ErrorCode::INDEX_TYPE_ERROR);

        res = checked_int(static_cast<__int128>(lhs.as_int())
            * (static_cast<__int128>(1) << imm));
        DISPATCH();
    }
    CASE(OP_RSHIFT)
    {
        Value& res = RA();
        Value lhs = RB();
        if (UNLIKELY(!lhs.is_int()))
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        reg_t imm = instr_C(instr);
        if (imm >= 64)
            raise_error(ErrorCode::INDEX_TYPE_ERROR);

        u64 shifted = static_cast<u64>(lhs.as_int()) >> imm;
        if (lhs.as_int() < 0)
            res = Value::from_real(static_cast<f64>(shifted));
        else
            res = Value::from_int(static_cast<i64>(shifted));

        DISPATCH();
    }
    CASE(OP_EQ)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        bool has_overload = false;
        if (lhs.is_instance())
            has_overload = lhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::EQ)) >= 0;
        if (!has_overload && rhs.is_instance())
            has_overload = rhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::EQ)) >= 0;
        if (has_overload) {
            VM_INSTANCE_OP(EQ);
            LOAD_FRAME();
            DISPATCH();
        }
        res = Value::from_bool(values_equal(lhs, rhs));
        RECORD_BINARY_IC(lhs, rhs, res);

        DISPATCH();
    }
    CASE(OP_NEQ)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        bool has_overload = false;
        if (lhs.is_instance())
            has_overload = lhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::NEQ)) >= 0;
        if (!has_overload && rhs.is_instance())
            has_overload = rhs.as_instance()->klass->method_slot(sp_method_name(ObjClass::NEQ)) >= 0;
        if (has_overload) {
            VM_INSTANCE_OP(NEQ);
            LOAD_FRAME();
            DISPATCH();
        }
        res = Value::from_bool(!values_equal(lhs, rhs));
        RECORD_BINARY_IC(lhs, rhs, res);

        DISPATCH();
    }
    CASE(OP_LT)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Value::from_bool(VMOPI(lhs, rhs, <));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Value::from_bool(lhs.as_string()->str < rhs.as_string()->str);
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_bool(lhs.as_double_any() < rhs.as_double_any());
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(LT);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        DISPATCH();
    }
    CASE(OP_LTE)
    {
        Value& res = RA();
        Value lhs = RB();
        Value rhs = RC();

        if (lhs.is_int() && rhs.is_int()) {
            res = Value::from_bool(VMOPI(lhs, rhs, <=));
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_string() && rhs.is_string()) {
            res = Value::from_bool(lhs.as_string()->str <= rhs.as_string()->str);
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_number() && rhs.is_number()) {
            res = Value::from_bool(lhs.as_double_any() <= rhs.as_double_any());
            RECORD_BINARY_IC(lhs, rhs, res);
        } else if (lhs.is_instance() || rhs.is_instance()) {
            VM_INSTANCE_OP(LTE);
            LOAD_FRAME();
            DISPATCH();
        } else {
            raise_error(ErrorCode::TYPE_ERROR_COMPARE);
        }

        DISPATCH();
    }
    CASE(OP_NOT)
    {
        RA() = Value::from_bool(!RB().is_truthy());
        DISPATCH();
    }
    CASE(CONCAT)
    {
        DISPATCH();
    }
    CASE(LIST_NEW)
    {
        ObjList* list_obj = m_gc.make_obj_list();
        list_obj->reserve(instr_B(instr));
        RA() = Value::from_obj(reinterpret_cast<ObjHeader*>(list_obj));
        DISPATCH();
    }
    CASE(LIST_APPEND)
    {
        Value& list_v = RA();
        if (!list_v.is_list())
            raise_error(ErrorCode::TYPE_ERROR_CALL, "attempting to append on a non list");

        list_v.as_list()->elements.push(RB());
        DISPATCH();
    }
    CASE(LIST_GET)
    {
        Value& res = RA();
        Value list_v = RB();
        Value index_v = RC();

        if (!list_v.is_list())
            raise_error(ErrorCode::TYPE_ERROR_CALL, "attempting get on a non list");
        if (!index_v.is_int())
            raise_error(ErrorCode::INDEX_TYPE_ERROR, "attempting get with a non integer index");

        auto& elems = list_v.as_list()->elements;
        i64 idx = index_v.as_int();
        if (idx < 0 || idx >= static_cast<i64>(elems.size()))
            raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        res = elems[static_cast<u32>(idx)];
        DISPATCH();
    }
    CASE(LIST_SET)
    {
        Value& object_v = RA();
        Value index_v = RB();
        Value new_val = RC();

        if (object_v.is_list()) {
            if (!index_v.is_int())
                raise_error(ErrorCode::INDEX_TYPE_ERROR);

            auto& elems = object_v.as_list()->elements;
            i64 idx = index_v.as_int();

            if (idx < 0)
                idx += static_cast<i64>(elems.size());
            if (idx < 0 || idx >= static_cast<i64>(elems.size()))
                raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS, "index " + std::to_string(index_v.as_int()) + " for list of length " + std::to_string(elems.size()));

            elems[static_cast<u32>(idx)] = new_val;
        } else {
            raise_error(ErrorCode::TYPE_ERROR_CALL, "attempting set on a non list value");
        }

        DISPATCH();
    }
    CASE(LIST_LEN)
    {
        Value list_v = RB();
        if (!list_v.is_list())
            raise_error(ErrorCode::TYPE_ERROR_CALL, "attempting len on a non list value");

        RA() = Value::from_int(static_cast<i64>(list_v.as_list()->elements.size()));
        DISPATCH();
    }
    CASE(JUMP)
    {
        ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(JUMP_IF_TRUE)
    {
        if (RA().is_truthy())
            ip += instr_sBx(instr);

        DISPATCH();
    }
    CASE(JUMP_IF_FALSE)
    {
        if (!RA().is_truthy())
            ip += instr_sBx(instr);

        DISPATCH();
    }
    CASE(LOOP)
    {
        ip += instr_sBx(instr);
        DISPATCH();
    }
    CASE(FOR_PREP)
    {
        reg_t base_reg = instr_A(instr);
        Value init_v = cur_base[base_reg];
        Value limit_v = cur_base[base_reg + 1];
        Value step_v = cur_base[base_reg + 2];

        if (!init_v.is_int() || !limit_v.is_int() || !step_v.is_int())
            raise_error(ErrorCode::TYPE_ERROR_ARITH);

        i64 init = init_v.as_int();
        i64 limit = limit_v.as_int();
        i64 step = step_v.as_int();
        if (step == 0)
            raise_error(ErrorCode::DIVISION_BY_ZERO);

        cur_base[base_reg] = Value::from_int(limit);
        cur_base[base_reg + 1] = Value::from_int(step);
        cur_base[base_reg + 2] = Value::from_int(init);
        bool enters = (step > 0) ? (init <= limit) : (init >= limit);
        if (!enters)
            ip += instr_sBx(instr);

        DISPATCH();
    }
    CASE(FOR_STEP)
    {
        reg_t base_reg = instr_A(instr);
        Value limit_v = cur_base[base_reg];
        Value step_v = cur_base[base_reg + 1];
        Value control_v = cur_base[base_reg + 2];

        if (control_v.is_int()) {
            Value control_value = checked_int(static_cast<__int128>(control_v.as_int()) + step_v.as_int());
            i64 control = control_value.as_int();
            cur_base[base_reg + 2] = control_value;
            bool continues = (step_v.as_int() > 0) ? (control <= limit_v.as_int())
                                                   : (control >= limit_v.as_int());
            if (continues)
                ip += instr_sBx(instr);
        } else {
            f64 control = control_v.as_double_any() + step_v.as_double_any();
            cur_base[base_reg + 2] = Value::from_real(control);
            bool continues = step_v.as_double() > 0.0 ? control <= limit_v.as_double()
                                                      : control >= limit_v.as_double();
            if (continues)
                ip += instr_sBx(instr);
        }

        DISPATCH();
    }
    CASE(CLOSURE)
    {
        u16 fn_idx = instr_Bx(instr);
        if (UNLIKELY(fn_idx >= cur_chunk->functions.size()
                || cur_chunk->functions[fn_idx] == nullptr))
            raise_error(ErrorCode::INVALID_OPCODE, "function index out of bounds");
        Chunk* fn_chunk = cur_chunk->functions[fn_idx];
        RA() = m_gc.make_function(fn_chunk, current_globals());
        DISPATCH();
    }
    CASE(CALL)
    {
        reg_t fn_reg = instr_A(instr);
        reg_t argc = instr_B(instr);
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;

        SAVE_IP();
        call_value(callee, argc, base, false);

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(CALL_TAIL)
    {
        reg_t fn_reg = instr_A(instr);
        reg_t argc = instr_B(instr);
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;

        if (callee.is_native()) {
            ObjNative* nat = callee.as_native();
            if (UNLIKELY(nat->arity >= 0 && argc != nat->arity)) {
                std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
                raise_error(ErrorCode::NATIVE_ARG_COUNT,
                    std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
            }

            // Natives run synchronously with no pushed frame, so a tail call to
            // one must perform *this* frame's own return afterward — the
            // compiler omits a trailing RETURN for CALL_TAIL sites, trusting
            // the callee to terminate the frame itself.
            Value ret = call_native(nat, argc, base);
            CallFrame finished = frame();
            m_stack[finished.return_slot] = ret;
            m_frames_top -= 1;
            m_stack_top = finished.caller_stack_top;

            if (LIKELY(m_frames_top == 0))
                return ret;

            LOAD_FRAME();
            DISPATCH();
        }

        SAVE_IP();
        call_value(callee, argc, base, true);
        // A tail call to a class without a constructor completes synchronously
        // inside call_value().  In the outermost frame there is then no frame
        // for LOAD_FRAME() to read.
        if (UNLIKELY(m_frames_top == 0))
            return m_stack[0];
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(IC_CALL)
    {
        reg_t fn_reg = instr_A(instr);
        reg_t argc = instr_B(instr);
        reg_t ic_idx = instr_C(instr);
        u32 call_ip = ip - 1;
        Value callee = cur_base[fn_reg];
        int base = cur_frame_base + fn_reg + 1;
        bool has_slot = ic_idx < cur_chunk->ic_slots.size();

        if (has_slot) {
            auto& slot = cur_chunk->ic_slots[ic_idx];
            slot.seen_lhs |= static_cast<u8>(value_type_tag(callee));
            slot.hit_count++;
        }

        Chunk* caller_chunk = cur_chunk;
        int result_slot = base - 1;

        SAVE_IP();
        call_value(callee, argc, base, false);

        LOAD_FRAME();

        if (has_slot) {
            caller_chunk->ic_slots[ic_idx].seen_ret |= static_cast<u8>(value_type_tag(m_stack[result_slot]));
            caller_chunk->code[call_ip] = make_ABC(OpCode::CALL, fn_reg, argc, 0);
        }

        DISPATCH();
    }
    CASE(RETURN)
    {
        reg_t src = instr_A(instr);
        reg_t n_ret = instr_B(instr);
        Value ret = n_ret > 0 ? cur_base[src] : Value::nil();
        CallFrame finished = frame();
        if (finished.module != nullptr) {
            finished.module->executing = false;
            finished.module->initialized = true;
            ret = Value::from_module(finished.module);
        }
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth)
            return ret;
        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(RETURN_NIL)
    {
        CallFrame finished = frame();
        Value ret = Value::nil();
        if (finished.module != nullptr) {
            finished.module->executing = false;
            finished.module->initialized = true;
            ret = Value::from_module(finished.module);
        }
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth)
            return ret;
        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(RETURN1)
    {
        Value ret = cur_base[instr_A(instr)];
        CallFrame finished = frame();
        if (finished.module != nullptr) {
            finished.module->executing = false;
            finished.module->initialized = true;
            ret = Value::from_module(finished.module);
        }
        m_stack[finished.return_slot] = ret;
        m_frames_top -= 1;
        m_stack_top = finished.caller_stack_top;

        if (m_frames_top == stop_frame_depth)
            return ret;
        if (LIKELY(m_frames_top == 0))
            return ret;

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(INDEX_READ)
    {
        Value& res = RA();
        Value obj = RB();
        Value idx = RC();

        if (obj.is_string()) {
            if (UNLIKELY(!idx.is_int()))
                raise_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = idx.as_int();
            if (UNLIKELY(idx_int < 0))
                raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            StringRef const& str = obj.as_string()->str;

            size_t byte_pos = 0;
            i64 char_pos = 0;
            u64 char_bytes = 0;

            while (byte_pos < str.len() && char_pos < idx_int) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_pos++;
            }

            if (UNLIKELY(byte_pos >= str.len()))
                raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

            util::decode_utf8_at(str, byte_pos, &char_bytes);

            StringRef ch = str.slice(byte_pos, byte_pos + char_bytes);
            res = m_gc.make_string(ch);
        } else if (obj.is_list()) {
            if (UNLIKELY(!idx.is_int()))
                raise_error(ErrorCode::INDEX_TYPE_ERROR);

            i64 idx_int = idx.as_int();
            ListType list = obj.as_list()->elements;
            if (UNLIKELY(idx_int >= list.size() || idx_int < 0))
                raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS, "index " + std::to_string(idx_int) + " for list of length " + std::to_string(list.size()));

            res = obj.as_list()->elements[idx_int];
        } else if (obj.is_dict()) {
            ObjDict* dict_obj = obj.as_dict();
            if (dict_obj->data.find_ptr(idx) == nullptr)
                res = Value::nil();
            else
                res = dict_obj->data[idx];
        } else if (obj.is_instance()) {
            if (!idx.is_string())
                raise_error(ErrorCode::INDEX_TYPE_ERROR);
            ObjInstance* inst = obj.as_instance();
            int field_idx = inst->klass->field_index(idx.as_string()->str);
            if (field_idx < 0)
                raise_error(ErrorCode::UNDEFINED_METHOD); // or a dedicated "no such field" code
            res = inst->fields[field_idx];
        } else {
            raise_error(ErrorCode::INDEX_TYPE_ERROR);
        }

        DISPATCH();
    }
    CASE(INDEX_WRITE)
    {
        Value& obj = RA();
        Value idx = RB();
        Value val = RC();

        /// NOTE: 'str' is immutable by design, so INDEX_WRITE falls back to raise_error

        if (obj.is_list()) {
            if (!idx.is_int())
                raise_error(ErrorCode::INDEX_TYPE_ERROR);
            i64 idx_int = idx.as_int();
            ObjList* list = obj.as_list();
            if (idx_int >= list->size() || idx_int < 0)
                raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);
            list->elements[idx.as_int()] = val;
        } else if (obj.is_dict()) {
            obj.as_dict()->set(idx, val);
        } else if (obj.is_instance()) {
            /// TODO: go get the '[]' operator
            raise_error(ErrorCode::INDEX_OBJECT_TYPE_ERROR);
        } else {
            raise_error(ErrorCode::INDEX_OBJECT_TYPE_ERROR);
        }

        DISPATCH();
    }
    CASE(NEW_CLASS)
    {
        {
            Value& res = RA();
            u32 desc_idx = instr_Bx(instr);
            if (UNLIKELY(desc_idx >= cur_chunk->class_descriptors.size()))
                raise_error(ErrorCode::INVALID_OPCODE, "class descriptor index out of bounds");
            ClassDescriptor klass_desc = cur_chunk->class_descriptors[desc_idx];
            ObjClass* parent = nullptr;
            if (!klass_desc.parent_name.empty()) {
                Value const* parent_value = find_global(current_globals(), klass_desc.parent_name);
                if (parent_value == nullptr || !parent_value->is_class())
                    raise_error(ErrorCode::TYPE_ERROR_CALL,
                        "parent is not a class: " + std::string(klass_desc.parent_name.data(), klass_desc.parent_name.len()));
                parent = parent_value->as_class();
            }

            std::vector<StringRef> fields;
            std::vector<StringRef> methods;
            std::vector<Chunk*> vtable;
            if (parent != nullptr) {
                for (auto const& field : parent->field_names)
                    fields.push_back(field);
                for (auto const& method : parent->method_names)
                    methods.push_back(method);
                for (auto* fn : parent->vtable)
                    vtable.push_back(fn);
            } else {
                methods.resize(ObjClass::_COUNT);
                vtable.resize(ObjClass::_COUNT, nullptr);
            }

            for (u32 i = 0; i < klass_desc.field_names.size(); ++i) {
                bool duplicate = false;
                for (auto const& inherited : fields) {
                    if (inherited == klass_desc.field_names[i]) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    fields.push_back(klass_desc.field_names[i]);
            }

            for (u32 i = 0; i < klass_desc.vtable_size; ++i) {
                u32 fn_idx = klass_desc.vtable_indices[i];
                if (UNLIKELY(fn_idx != ClassDescriptor::NULL_SLOT
                        && fn_idx >= cur_chunk->functions.size()))
                    raise_error(ErrorCode::INVALID_OPCODE, "class method index out of bounds");
                if (fn_idx == ClassDescriptor::NULL_SLOT || klass_desc.method_names[i].empty())
                    continue;
                Chunk* method_chunk = cur_chunk->functions[fn_idx];
                method_chunk->globals = current_globals();

                int target = -1;
                if (i < ObjClass::_COUNT) {
                    target = static_cast<int>(i);
                } else {
                    for (u32 j = 0; j < methods.size(); ++j) {
                        if (methods[j] == klass_desc.method_names[i]) {
                            target = static_cast<int>(j);
                            break;
                        }
                    }
                }
                if (target < 0) {
                    target = static_cast<int>(methods.size());
                    methods.push_back(klass_desc.method_names[i]);
                    vtable.push_back(nullptr);
                }
                if (static_cast<size_t>(target) >= methods.size()) {
                    methods.resize(static_cast<size_t>(target) + 1);
                    vtable.resize(static_cast<size_t>(target) + 1, nullptr);
                }
                methods[static_cast<size_t>(target)] = klass_desc.method_names[i];
                vtable[static_cast<size_t>(target)] = method_chunk;
            }

            Array<StringRef, /*_Alloc=*/GarbageCollector> kfield_names {
                static_cast<u32>(fields.size()), { }, &m_gc
            };
            Array<StringRef, /*_Alloc=*/GarbageCollector> kmethod_names {
                static_cast<u32>(methods.size()), { }, &m_gc
            };
            Array<Chunk*, /*_Alloc=*/GarbageCollector> kvtable {
                static_cast<u32>(vtable.size()), { }, &m_gc
            };
            for (u32 i = 0; i < fields.size(); ++i)
                kfield_names[i] = fields[i];
            for (u32 i = 0; i < methods.size(); ++i)
                kmethod_names[i] = methods[i];
            for (u32 i = 0; i < vtable.size(); ++i)
                kvtable[i] = vtable[i];

            res = m_gc.make_class(klass_desc.name, kfield_names, kmethod_names, kvtable);
            res.as_class()->parent = parent;
            res.as_class()->globals = current_globals();
        }

        DISPATCH();
    }
    CASE(NEW_INSTANCE)
    {
        u16 index = instr_Bx(instr);
        if (UNLIKELY(index >= cur_chunk->constants.size()
                || !cur_chunk->constants[index].is_class()))
            raise_error(ErrorCode::INVALID_OPCODE, "invalid class constant");
        ObjClass* klass = cur_chunk->constants[index].as_class();
        RA() = m_gc.make_instance(klass);
        DISPATCH();
    }
    CASE(INVOKE)
    {
        reg_t self_reg = instr_A(instr);
        reg_t slot = instr_B(instr);
        reg_t argc = instr_C(instr);

        if (UNLIKELY(ip >= cur_chunk->code.size()
                || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
            raise_error(ErrorCode::INVALID_OPCODE, "missing INVOKE cache payload");
        ++ip; // consume the trailing NOP carrying the IC slot index

        Value self_val = cur_base[self_reg];

        if (UNLIKELY(!self_val.is_instance()))
            raise_error(ErrorCode::TYPE_ERROR_CALL, "(method call on non-instance)");

        ObjInstance* inst = self_val.as_instance();

        if (UNLIKELY(slot >= inst->klass->vtable.size() || inst->klass->vtable[slot] == nullptr))
            raise_error(ErrorCode::TYPE_ERROR_CALL, "method slot is empty");

        invoke_method(inst->klass->vtable[slot], self_val,
            cur_frame_base + self_reg, cur_frame_base + self_reg + 1,
            argc, ip, m_stack_top, inst->klass->vtable[slot]->globals);

        LOAD_FRAME();
        DISPATCH();
    }
    CASE(INVOKE_NAMED)
    {
        {
            Value inst = RA();
            u32 argc = instr_C(instr);

            if (UNLIKELY(ip >= cur_chunk->code.size()))
                raise_error(ErrorCode::INVALID_OPCODE, "missing INVOKE_NAMED payload");
            u32 payload = cur_chunk->code[ip++];
            if (UNLIKELY(instr_op(payload) != OpCode::NOP))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid INVOKE_NAMED payload");
            u32 name_idx = instr_Bx(payload);

            if (UNLIKELY(!inst.is_instance()))
                raise_error(ErrorCode::TYPE_ERROR_CALL, "(method call on non-instance)");

            ObjInstance* inst_obj = inst.as_instance();
            /// there many unchecked operations that may potentially fail here
            /// Compiler must guarantee that the name index in the constant table is valid
            /// otherwise it should throw an early error, also that index should always contain
            /// a string object and of course the index muse be an int, all of these must be
            /// guaranteed by the compiler before emitting this instruction
            if (UNLIKELY(name_idx >= cur_chunk->constants.size()
                    || !cur_chunk->constants[name_idx].is_string()))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid method-name constant");
            StringRef method_name = cur_chunk->constants[name_idx].as_string()->str;
            int slot = inst_obj->klass->method_slot(method_name);
            if (slot == -1)
                raise_error(ErrorCode::TYPE_ERROR_CALL,
                    "instance class does not define this method: " + std::string(method_name.data()));

            invoke_method(inst_obj->klass->vtable[slot], inst,
                cur_frame_base + instr_A(instr),
                cur_frame_base + instr_A(instr) + 1,
                static_cast<int>(argc), ip, m_stack_top, inst_obj->klass->vtable[slot]->globals);
        }
        LOAD_FRAME();
        DISPATCH();
    }
    CASE(GET_FIELD)
    {
        Value& res = RA();
        Value obj_v = RB();

        if (obj_v.is_module()) {
            if (UNLIKELY(instr_C(instr) != 0xFF || ip >= cur_chunk->code.size()
                    || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
                raise_error(ErrorCode::INVALID_OPCODE, "module access requires a name payload");
            u16 name_idx = instr_Bx(cur_chunk->code[ip++]);
            if (UNLIKELY(name_idx >= cur_chunk->constants.size() || !cur_chunk->constants[name_idx].is_string()))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid module attribute name");
            ObjModule* module = obj_v.as_module();
            StringRef const& name = cur_chunk->constants[name_idx].as_string()->str;
            Value const* value = module->globals == nullptr ? nullptr : module->globals->find(name);
            if (value == nullptr || (module->globals->index.find_ptr(name) == nullptr))
                raise_error(ErrorCode::UNDEFINED_GLOBAL,
                    "module '" + module->name + "' has no export '" + std::string(name.data(), name.len()) + "'");
            res = *value;
            DISPATCH();
        }

        if (!UNLIKELY(obj_v.is_instance()))
            raise_error(ErrorCode::UNDEFINED_FIELD,
                type(1, &obj_v).as_string()->str.data() + std::string(" is not a class"));

        ObjInstance* inst = obj_v.as_instance();
        u32 field_idx = instr_C(instr);

        if (field_idx == 0xFF) {
            if (UNLIKELY(ip >= cur_chunk->code.size()
                    || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
                raise_error(ErrorCode::INVALID_OPCODE, "missing GET_FIELD payload");
            u32 payload = cur_chunk->code[ip]; // the NOP immediately after GET_FIELD
            u16 name_idx = instr_Bx(payload);
            ip++; // consume the payload word so DISPATCH doesn't re-decode it as a real op

            if (UNLIKELY(name_idx >= cur_chunk->constants.size()
                    || !cur_chunk->constants[name_idx].is_string()))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid field-name constant");
            ObjString* name_obj = cur_chunk->constants[name_idx].as_string();
            int slot = inst->klass->field_index(name_obj->str); // needs a runtime-side field_index lookup

            if (UNLIKELY(slot < 0))
                raise_error(ErrorCode::UNDEFINED_FIELD,
                    type(1, &obj_v).as_string()->str.data() + std::string(" does not define ") + name_obj->str.data());

            res = inst->fields[static_cast<u32>(slot)];
            DISPATCH();
        }

        if (UNLIKELY(field_idx >= inst->fields.size()))
            raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        res = inst->fields[field_idx];
        DISPATCH();
    }
    CASE(SET_FIELD)
    {
        Value obj_v = RA();
        if (UNLIKELY(!obj_v.is_instance()))
            raise_error(ErrorCode::TYPE_ERROR_CALL, "SET_FIELD on non-instance");

        ObjInstance* inst = obj_v.as_instance();
        reg_t field_idx = instr_B(instr);

        if (field_idx == 0xFF) {
            if (UNLIKELY(ip >= cur_chunk->code.size()
                    || instr_op(cur_chunk->code[ip]) != OpCode::NOP))
                raise_error(ErrorCode::INVALID_OPCODE, "missing SET_FIELD payload");
            u32 payload = cur_chunk->code[ip++];
            u16 name_idx = instr_Bx(payload);

            if (UNLIKELY(name_idx >= cur_chunk->constants.size()
                    || !cur_chunk->constants[name_idx].is_string()))
                raise_error(ErrorCode::INVALID_OPCODE, "invalid field-name constant");

            ObjString* name_obj = cur_chunk->constants[name_idx].as_string();
            int slot = inst->klass->field_index(name_obj->str);
            if (UNLIKELY(slot < 0))
                raise_error(ErrorCode::UNDEFINED_FIELD,
                    std::string(name_obj->str.data(), name_obj->str.len()));

            inst->fields[static_cast<u32>(slot)] = RC();
            DISPATCH();
        }

        if (UNLIKELY(field_idx >= inst->fields.size()))
            raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

        inst->fields[field_idx] = RC();
        DISPATCH();
    }

    CASE(NOP) { DISPATCH(); }
    CASE(HALT) { halt(); }

    END_DISPATCH();

    return Value::nil(); // UNREACHABLE
}

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic pop
#endif

// Calls target_chunk with self_val in callee register 0. total_argc includes
// that implicit self slot; explicit arguments must already follow call_base.
void VM::invoke_method(Chunk* target_chunk, Value self_val,
    int result_slot, int call_base, int total_argc, u32 ip, int caller_stack_top,
    GlobalEnvironment* globals)
{
    if (UNLIKELY(target_chunk == nullptr))
        raise_error(ErrorCode::TYPE_ERROR_CALL, "method slot is empty");
    if (UNLIKELY(total_argc != target_chunk->arity))
        raise_error(ErrorCode::WRONG_ARG_COUNT);
    if (UNLIKELY(m_frames_top >= MAX_FRAMES))
        raise_error(ErrorCode::STACK_OVERFLOW);

    int local_count = target_chunk->local_count;
    int new_top = call_base + local_count + 1;
    if (UNLIKELY(new_top > STACK_SIZE))
        raise_error(ErrorCode::STACK_OVERFLOW);

    // The caller may stage method arguments immediately above its live top.
    // Preserve that initialized prefix before clearing the remaining locals.
    int initialized_top = call_base + total_argc;
    if (m_stack_top < initialized_top)
        m_stack_top = initialized_top;
    while (m_stack_top < new_top) {
        m_stack[m_stack_top] = Value::nil();
        m_stack_top++;
    }

    m_stack[call_base] = self_val; // self lands in register 0 of the callee
    // explicit args are assumed already placed at call_base+1 .. call_base+explicit_argc
    // by the caller, before this is invoked.

    for (int i = total_argc; i < local_count; i++)
        m_stack[call_base + i] = Value::nil();

    SAVE_IP();
    m_frames[m_frames_top] = CallFrame(nullptr, target_chunk, 0,
        static_cast<u16>(call_base), static_cast<u16>(local_count),
        static_cast<u16>(result_slot), static_cast<u16>(caller_stack_top),
        globals == nullptr ? current_globals() : globals);
    m_frames_top++;
}

void VM::call_value(Value callee, int argc, int call_base, bool tail)
{
    if (callee.is_function()) {
        ObjFunction* fn = callee.as_func();
        Chunk* fchk = fn->chunk;
        int arity = fchk->arity;
        int local_count = fchk->local_count;

        if (argc != arity)
            raise_error(ErrorCode::WRONG_ARG_COUNT, std::string(fchk->name.data(), fchk->name.len()) + "() expected " + std::to_string(arity) + " arguments but got " + std::to_string(argc));

        if (local_count < argc)
            raise_error(ErrorCode::WRONG_ARG_COUNT);

        if (tail && m_frames_top > 0) {
            CallFrame old_frame = m_frames[m_frames_top - 1];
            int cur_base = old_frame.base;
            int new_top = cur_base + local_count + 1;
            if (UNLIKELY(new_top > STACK_SIZE))
                raise_error(ErrorCode::STACK_OVERFLOW);

            for (int i = 0; i < argc; i++)
                m_stack[cur_base + i] = m_stack[call_base + i];
            for (int i = argc; i < local_count; i++)
                m_stack[cur_base + i] = Value::nil();

            m_frames[m_frames_top - 1] = CallFrame(fn, fchk, 0,
                static_cast<u16>(cur_base), static_cast<u16>(local_count),
                old_frame.return_slot, old_frame.caller_stack_top, fn->globals);
            m_stack_top = new_top;
        } else {
            if (UNLIKELY(m_frames_top >= MAX_FRAMES))
                raise_error(ErrorCode::STACK_OVERFLOW);

            int caller_stack_top = m_stack_top;
            int new_top = call_base + local_count + 1;
            if (UNLIKELY(new_top > STACK_SIZE))
                raise_error(ErrorCode::STACK_OVERFLOW);

            while (m_stack_top < new_top) {
                m_stack[m_stack_top] = Value::nil();
                m_stack_top++;
            }

            for (int i = argc; i < local_count; i++)
                m_stack[call_base + i] = Value::nil();

            m_frames[m_frames_top] = CallFrame(fn, fchk, 0,
                static_cast<u16>(call_base), static_cast<u16>(local_count),
                static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top), fn->globals);
            m_frames_top++;
        }
        return;
    }

    if (callee.is_native()) {
        ObjNative* nat = callee.as_native();
        if (nat->arity >= 0 && argc != nat->arity) {
            std::string name = (nat->name ? std::string(nat->name->str.data(), nat->name->str.len()) : "?");
            raise_error(ErrorCode::NATIVE_ARG_COUNT,
                std::string("native '") + name + "' expected " + std::to_string(nat->arity) + " args, got " + std::to_string(argc));
        }

        m_stack[call_base - 1] = call_native(nat, argc, call_base);
        return;
    }

    if (callee.is_class()) {
        ObjClass* klass = callee.as_class();
        ObjInstance* inst = m_gc.make_obj_instance(klass);
        Value instance = Value::from_obj(reinterpret_cast<ObjHeader*>(inst));

        int ctor_slot = klass->method_slot(StringRef { "بداية" });
        if (ctor_slot < 0)
            ctor_slot = klass->method_slot(StringRef { "init" });

        if (ctor_slot < 0) {
            if (argc != 0)
                raise_error(ErrorCode::WRONG_ARG_COUNT);
            if (tail && m_frames_top > 0) {
                CallFrame finished = frame();
                m_stack[finished.return_slot] = instance;
                m_frames_top -= 1;
                m_stack_top = finished.caller_stack_top;
                return;
            }
            m_stack[call_base - 1] = instance;
            return;
        }

        Chunk* ctor_chunk = klass->vtable[static_cast<u32>(ctor_slot)];
        if (UNLIKELY(argc + 1 != ctor_chunk->arity))
            raise_error(ErrorCode::WRONG_ARG_COUNT);

        int local_count = ctor_chunk->local_count;

        // NEW: honor tail — reuse the caller's own frame slot instead of
        // stacking a fresh one, exactly like the is_function tail path does.
        CallFrame old_frame;
        if (tail && m_frames_top > 0)
            old_frame = m_frames[m_frames_top - 1];
        int dest_base = (tail && m_frames_top > 0) ? old_frame.base : call_base;
        int caller_stack_top = m_stack_top;

        if (UNLIKELY(m_stack_top + 1 >= STACK_SIZE))
            raise_error(ErrorCode::STACK_OVERFLOW);

        if (tail && dest_base < call_base) {
            for (int i = 0; i < argc; ++i)
                m_stack[dest_base + i + 1] = m_stack[call_base + i];
        } else {
            for (int i = argc - 1; i >= 0; --i)
                m_stack[dest_base + i + 1] = m_stack[call_base + i];
        }
        m_stack[dest_base] = instance;

        int new_top = dest_base + local_count + 1;
        if (UNLIKELY(new_top > STACK_SIZE))
            raise_error(ErrorCode::STACK_OVERFLOW);
        while (m_stack_top < new_top) {
            m_stack[m_stack_top] = Value::nil();
            m_stack_top++;
        }
        for (int i = argc + 1; i < local_count; i++)
            m_stack[dest_base + i] = Value::nil();

        if (tail && m_frames_top > 0) {
            m_frames[m_frames_top - 1] = CallFrame(nullptr, ctor_chunk, 0,
                static_cast<u16>(dest_base), static_cast<u16>(local_count),
                old_frame.return_slot, old_frame.caller_stack_top, ctor_chunk->globals);
            m_stack_top = new_top;
        } else {
            if (UNLIKELY(m_frames_top >= MAX_FRAMES))
                raise_error(ErrorCode::STACK_OVERFLOW);
            m_frames[m_frames_top] = CallFrame(nullptr, ctor_chunk, 0,
                static_cast<u16>(dest_base), static_cast<u16>(local_count),
                static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top), ctor_chunk->globals);
            m_frames_top++;
        }
        return;
    }

    if (callee.is_instance()) {
        ObjInstance* inst = callee.as_instance();
        int slot = inst->klass->method_slot(sp_method_name(ObjClass::CALL));
        if (UNLIKELY(slot < 0 || static_cast<u32>(slot) >= inst->klass->vtable.size()
                || inst->klass->vtable[static_cast<u32>(slot)] == nullptr))
            raise_error(ErrorCode::NON_FUNCTION_CALL);

        Chunk* target = inst->klass->vtable[static_cast<u32>(slot)];
        if (UNLIKELY(argc + 1 != target->arity))
            raise_error(ErrorCode::WRONG_ARG_COUNT);

        if (tail && m_frames_top > 0) {
            CallFrame old_frame = m_frames[m_frames_top - 1];
            int dest_base = old_frame.base;
            int new_top = dest_base + static_cast<int>(target->local_count) + 1;
            if (UNLIKELY(new_top > STACK_SIZE))
                raise_error(ErrorCode::STACK_OVERFLOW);
            for (int i = 0; i < argc; ++i)
                m_stack[dest_base + i + 1] = m_stack[call_base + i];
            m_stack[dest_base] = callee;
            for (int i = argc + 1; i < static_cast<int>(target->local_count); ++i)
                m_stack[dest_base + i] = Value::nil();
            m_frames[m_frames_top - 1] = CallFrame(nullptr, target, 0,
                static_cast<u16>(dest_base), static_cast<u16>(target->local_count),
                old_frame.return_slot, old_frame.caller_stack_top, target->globals);
            m_stack_top = new_top;
            return;
        }

        int caller_stack_top = m_stack_top;
        int method_base = caller_stack_top;
        int new_top = method_base + static_cast<int>(target->local_count) + 1;
        if (UNLIKELY(m_frames_top >= MAX_FRAMES || new_top > STACK_SIZE))
            raise_error(ErrorCode::STACK_OVERFLOW);
        m_stack[method_base] = callee;
        for (int i = 0; i < argc; ++i)
            m_stack[method_base + i + 1] = m_stack[call_base + i];
        for (int i = argc + 1; i < static_cast<int>(target->local_count); ++i)
            m_stack[method_base + i] = Value::nil();
        m_stack_top = new_top;
        m_frames[m_frames_top++] = CallFrame(nullptr, target, 0,
            static_cast<u16>(method_base), static_cast<u16>(target->local_count),
            static_cast<u16>(call_base - 1), static_cast<u16>(caller_stack_top), target->globals);
        return;
    }

    StringRef fn_name = "";
    if (callee.is_function())
        fn_name = callee.as_func()->name();

    raise_error(ErrorCode::NON_FUNCTION_CALL, std::string(fn_name.data(), fn_name.len()));
}

Value VM::call_native(ObjNative* nat, int argc, int call_base)
{
    return (this->*nat->fn)(argc, &m_stack[call_base]);
}

Value VM::call_value_sync(Value callee, ObjList* arguments)
{
    if (arguments == nullptr)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "dynamic call expects an argument list");
    int argc = static_cast<int>(arguments->elements.size());
    int stop_depth = m_frames_top;
    int saved_top = m_stack_top;
    ensure_stack_slots(argc + 1);
    int callee_slot = m_stack_top;
    m_stack[callee_slot] = callee;
    for (int i = 0; i < argc; ++i)
        m_stack[callee_slot + 1 + i] = arguments->elements[static_cast<u32>(i)];
    m_stack_top = callee_slot + argc + 1;

    call_value(callee, argc, callee_slot + 1, false);
    Value result = m_frames_top == stop_depth
        ? m_stack[callee_slot]
        : execute(stop_depth);
    m_stack_top = saved_top;
    return result;
}

Value VM::call_special_sync(Value receiver, int special_slot)
{
    if (!receiver.is_instance() || m_frames_top == 0)
        return Value::nil();

    ObjInstance* instance = receiver.as_instance();
    StringRef name = sp_method_name(special_slot);
    int slot = instance->klass->method_slot(name);
    if (slot < 0 || static_cast<u32>(slot) >= instance->klass->vtable.size())
        return Value::nil();
    Chunk* target = instance->klass->vtable[static_cast<u32>(slot)];
    if (target == nullptr || target->arity != 1)
        return Value::nil();

    int stop_depth = m_frames_top;
    int caller_top = m_stack_top;
    int result_slot = caller_top;
    int call_base = caller_top + 1;
    invoke_method(target, receiver, result_slot, call_base, 1,
        frame().ip, caller_top, target->globals);
    return execute(stop_depth);
}

ObjString* VM::intern(StringRef const& str)
{
    if (ObjString** existing = m_string_table.find_ptr(str))
        return *existing;

    ObjString* obj = m_gc.make_obj_string(str);
    m_string_table.insert_or_assign(str, obj);
    return obj;
}

void VM::intern_chunk_constants(Chunk* ch)
{
    if (ch == nullptr)
        return;

    for (u32 i = 0; i < ch->constants.size(); i++) {
        if (ch->constants[i].is_string())
            ch->constants[i] = Value::from_obj(reinterpret_cast<ObjHeader*>(intern(ch->constants[i].as_string()->str)));
    }

    for (auto* fn : ch->functions)
        intern_chunk_constants(fn);
}

void VM::open_stdlib()
{
    // Collections
    (void)register_native("طول", &VM::len, 1);
    (void)register_native("اضف", &VM::append, -1);
    (void)register_native("احذف", &VM::pop, 1);
    (void)register_native("مقطع", &VM::slice, -1);
    (void)register_native("قائمة", &VM::list, -1);
    (void)register_native("قاموس", &VM::dict, -1);
    (void)register_native("__قاموس_مفاتيح__", &VM::dict_keys, 1);
    (void)register_native("__قاموس_يحتوي__", &VM::dict_contains, 2);
    (void)register_native("__قاموس_احذف__", &VM::dict_delete, 2);
    // I/O
    (void)register_native("اكتب", &VM::print, -1);
    (void)register_native("ادخل", &VM::input, 0);
    (void)register_native("افتح", &VM::open, 2);
    (void)register_native("اضف_ملف", &VM::append_file, 2);
    (void)register_native("اغلق", &VM::close, 1);
    // Type system / conversion
    (void)register_native("صنف", &VM::type, 1);
    (void)register_native("طبيعي", &VM::Int, 1);
    (void)register_native("حقيقي", &VM::Float, 1);
    (void)register_native("سلسلة", &VM::str, -1);
    (void)register_native("منطقي", &VM::Bool, 1);
    // String ops
    (void)register_native("اقسم", &VM::split, 2);
    (void)register_native("اجمع", &VM::join, 2);
    (void)register_native("جزء", &VM::substr, 3);
    (void)register_native("يحتوي", &VM::contains, 2);
    (void)register_native("قص", &VM::trim, 1);
    (void)register_native("__نص_من_رمز__", &VM::char_from_codepoint, 1);
    (void)register_native("__عدد_من_نص__", &VM::number_from_text, 1);
    (void)register_native("__عدد_منته__", &VM::number_finite, 1);
    (void)register_native("__عدد_ليس_رقما__", &VM::number_is_nan, 1);
    (void)register_native("__JSON_اهرب__", &VM::json_escape, 1);
    (void)register_native("__JSON_اقرا_سلسلة__", &VM::json_read_string, 2);
    (void)register_native("__استدعاء__", &VM::dynamic_call, 2);
    (void)register_native("__منفذ_جديد__", &VM::executor_new, 1);
    (void)register_native("__منفذ_اغلق__", &VM::executor_close, 2);
    (void)register_native("__مهمة_ابدأ__", &VM::task_start, 3);
    (void)register_native("__مهمة_تمت__", &VM::task_done, 1);
    (void)register_native("__مهمة_نتيجة__", &VM::task_result, 2);
    (void)register_native("__مهمة_الغ__", &VM::task_cancel, 1);
    (void)register_native("__مهمة_انتظر_الكل__", &VM::task_wait_all, 2);
    (void)register_native("__ملف_افتح__", &VM::file_open, 2);
    (void)register_native("__ملف_اقرا__", &VM::file_read, 2);
    (void)register_native("__ملف_اقرا_الكل__", &VM::file_read_all, 1);
    (void)register_native("__ملف_اقرا_سطر__", &VM::file_read_line, 1);
    (void)register_native("__ملف_اكتب__", &VM::file_write, 2);
    (void)register_native("__ملف_اضف__", &VM::file_write, 2);
    (void)register_native("__ملف_ادفع__", &VM::file_flush, 1);
    (void)register_native("__ملف_اغلق__", &VM::close, 1);
    (void)register_native("__مسار_احذف__", &VM::path_delete, 1);
    (void)register_native("__مسار_glob__", &VM::path_glob, 2);
    (void)register_native("__ملف_مؤقت__", &VM::temp_file, 3);
    (void)register_native("__مجلد_مؤقت__", &VM::temp_directory, 2);
    (void)register_native("__نظام_احذف_شجرة__", &VM::remove_tree, 1);
    (void)register_native("__وقت_الان__", &VM::datetime_now, 0);
    (void)register_native("__وقت_من_حقول__", &VM::datetime_from_fields, 7);
    (void)register_native("__وقت_الى_حقول__", &VM::datetime_to_fields, 2);
    (void)register_native("__وقت_حلل__", &VM::datetime_parse, 3);
    (void)register_native("__وقت_نسق__", &VM::datetime_format, 3);
    (void)register_native("__64_رمز__", &VM::base64_encode, 2);
    (void)register_native("__64_فك__", &VM::base64_decode, 2);
    (void)register_native("__16_رمز__", &VM::hex_encode, 1);
    (void)register_native("__16_فك__", &VM::hex_decode, 1);
    (void)register_native("__هاش_جديد__", &VM::hash_new, 1);
    (void)register_native("__هاش_حدث__", &VM::hash_update, 2);
    (void)register_native("__هاش_ناتج__", &VM::hash_digest, 2);
    (void)register_native("__HMAC__", &VM::hmac, 3);
    (void)register_native("__ضغط__", &VM::compress, 3);
    (void)register_native("__فك_ضغط__", &VM::decompress, 3);
    // Math
    (void)register_native("ادنى", &VM::floor, 1);
    (void)register_native("اعلى", &VM::ceil, 1);
    (void)register_native("تقريب", &VM::round, 1);
    (void)register_native("مطلق", &VM::abs, 1);
    (void)register_native("اصغر", &VM::min, -1);
    (void)register_native("اكبر", &VM::max, -1);
    (void)register_native("قوة", &VM::pow, 2);
    (void)register_native("جذر", &VM::sqrt, 1);
    (void)register_native("__رياضيات__", &VM::math_unary, 2);
    (void)register_native("__رياضيات2__", &VM::math_binary, 3);
    (void)register_native("__URL_اهرب__", &VM::url_encode, 1);
    (void)register_native("__URL_فك__", &VM::url_decode, 1);
    (void)register_native("__URL_حلل__", &VM::url_parse, 1);
    (void)register_native("__URL_ركب__", &VM::url_build, 1);
    (void)register_native("__نمط_اجمع__", &VM::regex_compile, 2);
    (void)register_native("__نمط_بحث__", &VM::regex_search, 3);
    (void)register_native("__نمط_طابق__", &VM::regex_match, 3);
    (void)register_native("__نمط_كامل__", &VM::regex_fullmatch, 2);
    (void)register_native("__نمط_الكل__", &VM::regex_findall, 2);
    (void)register_native("__نمط_اقسم__", &VM::regex_split, 3);
    (void)register_native("__نمط_استبدل__", &VM::regex_replace, 4);
    // Runtime / diagnostics
    (void)register_native("تاكد", &VM::Assert, -1);
    (void)register_native("ساعة", &VM::clock, 0);
    (void)register_native("عطل", &VM::error, -1);
    (void)register_native("وقت", &VM::time, 0);
}

bool VM::register_native(StringRef const& name, NativeFn fn, int arity)
{
    ObjString* name_obj = m_gc.make_obj_string(name);
    Value val = m_gc.make_native(fn, name_obj, arity);

    store_global(&m_builtin_environment, name, val);

    return true;
}

SourceLocation VM::current_location() const
{
    if (m_frames_top == 0)
        return { };

    CallFrame const& f = top_frame();
    size_t off = f.ip > 0 ? f.ip - 1 : 0;
    Chunk const& ch = *f.chunk;

    if (off < ch.locations.size())
        return ch.locations[off];

    return { };
}

void VM::_raise_error(ErrorCode errc, std::string const& detail)
{
    SourceLocation loc = current_location();
    diagnostic::SourceScope source_scope(m_frames_top > 0 ? frame().chunk->source : diagnostic::engine.source());
    auto id = diagnostic::report(diagnostic::Severity::ERROR, loc, errc, detail);
    if (errc == ErrorCode::UNDEFINED_GLOBAL && !detail.empty()) {
        auto candidate = similar_name(current_globals(), detail);
        if (!candidate.empty())
            diagnostic::engine.add_suggestion(id, "Did you mean '" + candidate + "'?");
    }

    for (int i = 0; i < m_frames_top; ++i) {
        CallFrame* p = &m_frames[i];
        if (p->chunk == nullptr)
            continue;

        Chunk const& ch = *p->chunk;
        size_t off = p->ip > 0 ? p->ip - 1 : 0;
        if (off >= ch.locations.size())
            continue;

        SourceLocation frame_loc = ch.locations[off];
        auto name = p->func ? p->func->name() : ch.name;
        diagnostic::engine.add_frame(id, ch.source, frame_loc, name.empty() ? "<main>" : std::string(name.data(), name.len()));
    }

    diagnostic::dump();
    halt();
}

void VM::raise_error(ErrorCode errc, std::string const& detail)
{
    if (errc == ErrorCode::TYPE_ERROR_ARITH && detail.empty() && m_frames_top > 0) {
        auto const& current = frame();
        if (current.ip > 0 && current.ip <= current.chunk->code.size()) {
            u32 instruction = current.chunk->code[current.ip - 1];
            char const* operation = nullptr;
            switch (instr_op(instruction)) {
            case OpCode::OP_ADD: operation = "+"; break;
            case OpCode::OP_SUB: operation = "-"; break;
            case OpCode::OP_MUL: operation = "*"; break;
            case OpCode::OP_DIV: operation = "/"; break;
            case OpCode::OP_MOD: operation = "%"; break;
            default: break;
            }
            if (operation) {
                _raise_error(errc, std::string("operator '") + operation + "' received " + value_type_name(get_reg(current, instr_B(instruction))) + " and " + value_type_name(get_reg(current, instr_C(instruction))));
                return;
            }
        }
    }
    _raise_error(errc, detail);
}

void VM::unwind_failed_run()
{
    // A partially initialized module is useful only while breaking an active
    // import cycle. It must never masquerade as a successful import on retry.
    for (auto it = m_module_cache.begin(); it != m_module_cache.end();) {
        if (!it->second->initialized) {
            it->second->executing = false;
            it = m_module_cache.erase(it);
        } else
            ++it;
    }
    std::fill(m_stack, m_stack + std::clamp(m_stack_top, 0, STACK_SIZE), Value::nil());
    std::fill(m_frames, m_frames + std::clamp(m_frames_top, 0, MAX_FRAMES), CallFrame());
    m_frames_top = 0;
    m_stack_top = 0;
}

void VM::halt()
{
    unwind_failed_run();
    throw RuntimeHalt();
}

CallFrame& VM::frame() { return m_frames[m_frames_top - 1]; }
CallFrame const& VM::frame() const { return m_frames[m_frames_top - 1]; }

Chunk* VM::chunk() { return top_frame().chunk; }
Value& VM::reg(int r) { return m_stack[top_frame().base + r]; }

void VM::update_ic_binary(Chunk* ch, u32 nop_ip, Value lhs, Value rhs, Value result)
{
    if (ch == nullptr)
        return;

    u32 nop = ch->code[nop_ip];
    u8 ic_idx = instr_A(nop);

    if (ic_idx < ch->ic_slots.size()) {
        ICSlot& slot = ch->ic_slots[ic_idx];
        slot.seen_lhs |= static_cast<u8>(value_type_tag(lhs));
        slot.seen_rhs |= static_cast<u8>(value_type_tag(rhs));
        slot.seen_ret |= static_cast<u8>(value_type_tag(result));
        slot.hit_count++;
    }
}

} // namespace fairuz::runtime
