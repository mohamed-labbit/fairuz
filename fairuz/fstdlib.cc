//
// stdlib.cc
//

#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "futil.hpp"
#include "fvalue.hpp"
#include "fvm.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fairuz::runtime {

using RuntimeErrorCode = diagnostic::errc::runtime::Code;
using StdlibErrorCode = diagnostic::errc::stdlib::Code;

static Fa_StringRef format_double_string(f64 value)
{
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc())
        return Fa_StringRef(std::string(buf, static_cast<size_t>(ptr - buf)).c_str());

    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setprecision(14) << std::noshowpoint << value;
    return Fa_StringRef(oss.str().c_str());
}

static void append_rendered_value(Fa_StringRef& out, Fa_Value v, bool quote_strings)
{
    if (v.is_nil()) {
        out += "nil";
        return;
    }
    if (v.is_bool()) {
        out += v.as_bool() ? "صحيح" : "خطا";
        return;
    }
    if (v.is_int()) {
        out += Fa_StringRef(std::to_string(v.as_int()).c_str());
        return;
    }
    if (v.is_double()) {
        f64 d = v.as_double();
        if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15)
            out += Fa_StringRef(std::to_string(static_cast<i64>(d)).c_str());
        else
            out += format_double_string(d);
        return;
    }
    if (v.is_string()) {
        if (quote_strings)
            out += '"';
        out += v.as_string()->str;
        if (quote_strings)
            out += '"';
        return;
    }
    if (v.is_list()) {
        Fa_ObjList* list = v.as_list();
        out += '[';
        for (u32 i = 0, n = list->elements.size(); i < n; i++) {
            if (i > 0)
                out += ", ";
            append_rendered_value(out, list->elements[i], true);
        }
        out += ']';
        return;
    }
    if (v.is_dict()) {
        Fa_ObjDict* dict = v.as_dict();
        out += '{';
        u32 i = 0, end = dict->data.size() - 1;
        for (auto [k, v] : dict->data) {
            append_rendered_value(out, k, k.is_string());
            out += ": ";
            append_rendered_value(out, v, v.is_string());
            if (i == end)
                break;
            out += ", ";
            i++;
        }

        out += '}';
        return;
    }
    if (v.is_native()) {
        out += "<native>";
        return;
    }
    if (v.is_function()) {
        out += "<function>";
        return;
    }
    if (v.is_class()) {
        out += "<class ";
        Fa_ObjClass* klass = v.as_class();
        out += klass->name;
        out += '>';
        return;
    }
    if (v.is_instance()) {
        out += '<';
        Fa_ObjInstance* instance = v.as_instance();
        out += instance->klass->name;
        out += " instance>";
        return;
    }
}

static Fa_StringRef value_to_string(Fa_Value v)
{
    Fa_StringRef out = "";
    append_rendered_value(out, v, false);
    return out;
}

Fa_Value Fa_VM::Fa_len(int argc, Fa_Value* argv)
{
    if (argc == 0 || argv == nullptr)
        return Fa_Value::nil();

    if (argc == 1) {
        if (argv[0].is_string()) {
            Fa_StringRef const& str = argv[0].as_string()->str;
            size_t byte_pos = 0;
            i64 char_count = 0;

            while (byte_pos < str.len()) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_count++;
            }

            return Fa_Value::from_int(char_count);
        }

        if (argv[0].is_list())
            return Fa_Value::from_int(argv[0].as_list()->elements.size());
        if (argv[0].is_dict())
            return Fa_Value::from_int(argv[0].as_dict()->data.size());
    }

    /// do not accept multiple args for len
    return Fa_Value::nil();
}

static void print_runtime_value(Fa_Value v, int depth = 0)
{
    if (v.is_nil()) {
        std::cout << "nil";
        return;
    }

    if (v.is_bool()) {
        std::cout << (v.as_bool() ? "صحيح" : "خطا");
        return;
    }

    if (v.is_int()) {
        std::cout << v.as_int();
        return;
    }

    if (v.is_obj()) {
        Fa_ObjHeader* obj = v.as_obj();

        switch (obj->type) {
        case Fa_ObjType::STRING:
            std::cout << Fa_obj_cast<Fa_ObjString>(obj, Fa_ObjType::STRING)->str;
            return;

        case Fa_ObjType::LIST: {
            auto list = Fa_obj_cast<Fa_ObjList>(obj, Fa_ObjType::LIST);
            std::cout << '[';
            for (u32 i = 0, n = list->size(); i < n; i++) {
                if (i > 0)
                    std::cout << ", ";
                Fa_Value elem = list->elements[i];
                if (elem.is_obj() && elem.as_obj()->type == Fa_ObjType::STRING) {
                    std::cout << '"';
                    std::cout << elem.as_string()->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(elem, depth + 1);
                }
            }
            std::cout << ']';
            return;
        }

        case Fa_ObjType::DICT: {
            auto dict = Fa_obj_cast<Fa_ObjDict>(obj, Fa_ObjType::DICT);
            std::cout << '{';
            for (auto [k, v] : dict->data) {
                if (k.is_string()) {
                    std::cout << '"';
                    std::cout << k.as_string()->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(k, depth + 1);
                }
                std::cout << ": ";
                if (v.is_string()) {
                    std::cout << '"';
                    std::cout << v.as_string()->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(v, depth + 1);
                }
                std::cout << ", ";
            }
            std::cout << '}';
            return;
        }

        case Fa_ObjType::NATIVE: {
            auto nat = Fa_obj_cast<Fa_ObjNative>(obj, Fa_ObjType::NATIVE);
            std::cout << "<native ";
            if (nat->name)
                std::cout << nat->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case Fa_ObjType::FUNCTION: {
            auto* fn = Fa_obj_cast<Fa_ObjFunction>(obj, Fa_ObjType::FUNCTION);
            std::cout << "<function ";
            std::cout << fn->name();
            std::cout << '>';
            return;
        }

        case Fa_ObjType::CLASS: {
            auto klass = Fa_obj_cast<Fa_ObjClass>(obj, Fa_ObjType::CLASS);
            std::cout << "<class " << klass->name << '>';
            return;
        }

        case Fa_ObjType::INSTANCE: {
            auto instance = Fa_obj_cast<Fa_ObjInstance>(obj, Fa_ObjType::INSTANCE);
            std::cout << '<';
            if (instance->klass)
                std::cout << instance->klass->name;
            else
                std::cout << "?";
            std::cout << " instance>";
            return;
        }

        case Fa_ObjType::FILE_HANDLE: {
            auto file_handle = Fa_obj_cast<Fa_ObjFileHandle>(obj, Fa_ObjType::FILE_HANDLE);
            std::cout << '{' << '\n';
            std::cout << '\t' << "ptr: " << file_handle->fp << '\n';
            std::cout << '\t' << "is_open: " << (file_handle->is_open ? "true" : "false") << '\n';
            std::cout << '}';
            return;
        }
#if FA_USE_NANBOX
        case fairuz::runtime::Fa_ObjType::INT: // TODO:
#endif

        case Fa_ObjType::_COUNT:
            break;
        }
    }

    f64 d = v.as_double();
    if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15) {
        std::cout << static_cast<i64>(d);
    } else {
        std::cout << format_double_string(d);
    }
}

Fa_Value Fa_VM::Fa_print(int argc, Fa_Value* argv)
{
    if (argc == 0 || argv == nullptr) {
        std::cout << '\n';
        return Fa_Value::nil();
    }

    for (int i = 0; i < argc; i++) {
        if (i > 0)
            std::cout << '\t';
        print_runtime_value(argv[i]);
    }
    std::cout << '\n';
    return Fa_Value::nil();
}

Fa_Value Fa_VM::Fa_type(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_Value::nil();

    Fa_Value& v = argv[0];
    Fa_TypeTag type = value_type_tag(v);
    switch (type) {
    case Fa_TypeTag::NONE: return m_gc.make_string("لاشيء");
    case Fa_TypeTag::NIL: return m_gc.make_string("عدم");
    case Fa_TypeTag::BOOL: return m_gc.make_string("منطقي");
    case Fa_TypeTag::INT: return m_gc.make_string("طبيعي");
    case Fa_TypeTag::DOUBLE: return m_gc.make_string("حقيقي");
    case Fa_TypeTag::STRING: return m_gc.make_string("سلسلة");
    case Fa_TypeTag::LIST: return m_gc.make_string("قائمة");
    case Fa_TypeTag::FUNCTION: return m_gc.make_string("دالة");
    case Fa_TypeTag::NATIVE: return m_gc.make_string("دالة");
    case Fa_TypeTag::CLASS: return m_gc.make_string(v.as_class()->name);
    case Fa_TypeTag::INSTANCE: return m_gc.make_string(v.as_instance()->klass->name);
    case Fa_TypeTag::DICT: return m_gc.make_string("قاموس");
    case Fa_TypeTag::FILE_HANDLE: return m_gc.make_string("ملف");
    }

    return Fa_Value::nil(); // unreachable
}

Fa_Value Fa_VM::Fa_int(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_Value::nil();
    if (argv[0].is_number())
        return Fa_Value::from_int(static_cast<i64>(argv[0].as_double_any()));
    return Fa_Value::nil();
}

Fa_Value Fa_VM::Fa_float(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_Value::nil();

    if (argv[0].is_number())
        return Fa_Value::from_real(argv[0].as_double_any());

    return Fa_Value::nil();
}

Fa_Value Fa_VM::Fa_append(int argc, Fa_Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::APPEND_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    Fa_Value& list_v = argv[0];
    if (!list_v.is_list()) {
        stdlib_error(StdlibErrorCode::APPEND_TYPE_ERROR);
        return Fa_Value::nil();
    }

    Fa_ObjList* list_obj = list_v.as_list();

    for (int i = 1; i < argc; i++)
        list_obj->elements.push(argv[i]);

    return Fa_Value::nil();
}

Fa_Value Fa_VM::Fa_pop(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::POP_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    Fa_Value& list_v = argv[0];
    if (!list_v.is_list()) {
        stdlib_error(StdlibErrorCode::POP_TYPE_ERROR);
        return Fa_Value::nil();
    }

    Fa_ObjList* list_obj = list_v.as_list();

    if (list_obj->empty())
        stdlib_error(StdlibErrorCode::POP_EMPTY_LIST);

    list_obj->elements.pop();
    return list_v;
}

Fa_Value Fa_VM::Fa_slice(int argc, Fa_Value* argv)
{
    /// cut a copy of a container, with inclusive indices
    /// accept [container, start, end]
    /// a, b are the indices
    /// if b is null then cut [start:]

    if (argc < 2 || argc > 3) {
        stdlib_error(StdlibErrorCode::SLICE_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    auto resolve_end = [](Fa_Value& c) -> i64 {
        if (c.is_string())
            return c.as_string()->str.len() - 1;
        if (c.is_list())
            return c.as_list()->size() - 1;
        return INT64_C(0);
    };

    Fa_Value container = argv[0];
    Fa_Value start_value = argv[1];
    Fa_Value end_value = argc < 3 ? Fa_Value::nil() : argv[2];

    if (UNLIKELY(!start_value.is_int() || !(end_value.is_nil() || end_value.is_int())))
        runtime_error(RuntimeErrorCode::INDEX_TYPE_ERROR);

    i64 start_i = start_value.as_int();
    i64 end_i = end_value.is_nil() ? resolve_end(container) : end_value.as_int();

    if (UNLIKELY(start_i < 0 || end_i < 0 || start_i > end_i))
        runtime_error(RuntimeErrorCode::INDEX_OUT_OF_BOUNDS);

    u32 start = static_cast<u32>(start_i);
    u32 end = static_cast<u32>(end_i);

    if (container.is_string()) {
        Fa_ObjString* str_obj = container.as_string();
        if (end >= str_obj->str.len())
            runtime_error(RuntimeErrorCode::INDEX_OUT_OF_BOUNDS);

        return m_gc.make_string(str_obj->str.slice(start, end));
    } else if (container.is_list()) {
        Fa_ObjList* list_obj = container.as_list();
        if (end >= list_obj->size())
            runtime_error(RuntimeErrorCode::INDEX_OUT_OF_BOUNDS);

        Fa_ObjList* ret_list = m_gc.make_obj_list();
        for (u32 i = start; i <= end; i++)
            ret_list->elements.push(list_obj->elements[i]);

        return Fa_Value::from_list(ret_list);
    }

    return Fa_Value::nil();
}

Fa_Value Fa_VM::Fa_input(int /*argc*/, Fa_Value* /*argv*/) // input takes no args for now
{
    // read until user hits ENTER
    Fa_StringRef ret_str = "";
    std::string help = ""; // getline only accepts std::string

    if (!std::getline(std::cin, help))
        // don't know what error to report
        return Fa_Value::nil();

    ret_str = help.data();
    Fa_Value ret = m_gc.make_string(ret_str);
    return ret;
}

Fa_Value Fa_VM::Fa_str(int argc, Fa_Value* argv)
{
    if (argc > 1) {
        stdlib_error(StdlibErrorCode::STR_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    Fa_StringRef output = "";

    if (argc == 0 || argv == nullptr)
        return m_gc.make_string(output); // return empty on no arg

    if (argv[0].is_string())
        return m_gc.make_string(argv[0].as_string()->str);

    Fa_StringRef rendered = value_to_string(argv[0]);
    return m_gc.make_string(rendered);
}

Fa_Value Fa_VM::Fa_bool(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::BOOL_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    return argv[0].is_truthy() ? Fa_Value::from_bool(true) : Fa_Value::from_bool(false);
}

Fa_Value Fa_VM::Fa_list(int argc, Fa_Value* argv)
{
    Fa_Value ret = m_gc.make_list();
    Fa_ObjList* list_obj = ret.as_list();

    for (int i = 0; i < argc; i++)
        list_obj->elements.push(argv[i]);

    return ret;
}

Fa_Value Fa_VM::Fa_dict(int argc, Fa_Value* argv)
{
    Fa_Value ret = m_gc.make_dict();
    if (argc <= 0 || argv == nullptr)
        return ret;

    Fa_ObjDict* dict_obj = ret.as_dict();
    for (int i = 0; i + 1 < argc; i += 2)
        dict_obj->data[argv[i]] = argv[i + 1];

    return ret;
}

Fa_Value Fa_VM::Fa_split(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Fa_Value::nil();
    if (!argv[0].is_string() || !argv[1].is_string())
        return Fa_Value::nil();

    Fa_StringRef src = argv[0].as_string()->str;
    Fa_StringRef delim = argv[1].as_string()->str;

    Fa_Value ret = m_gc.make_list();
    Fa_ObjList* list = ret.as_list();

    if (delim.empty()) {
        list->elements.push(m_gc.make_string(src));
        return ret;
    }

    size_t start = 0;
    while (start <= src.len()) {
        size_t pos = start;
        bool found = false;
        while (pos + delim.len() <= src.len()) {
            if (::memcmp(src.data() + pos, delim.data(), delim.len()) == 0) {
                found = true;
                break;
            }

            pos++;
        }

        if (!found) {
            list->elements.push(m_gc.make_string(src.substr(start, src.len())));
            break;
        }

        list->elements.push(m_gc.make_string(src.substr(start, pos)));
        start = pos + delim.len();
    }

    return ret;
}

Fa_Value Fa_VM::Fa_join(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Fa_Value::nil();
    if (!argv[0].is_list() || !argv[1].is_string())
        return Fa_Value::nil();

    Fa_ObjList* list = argv[0].as_list();
    Fa_StringRef delim = argv[1].as_string()->str;
    Fa_StringRef out = "";

    for (u32 i = 0; i < list->elements.size(); i++) {
        if (i > 0)
            out += delim;

        out += value_to_string(list->elements[i]);
    }

    return m_gc.make_string(out);
}

Fa_Value Fa_VM::Fa_substr(int argc, Fa_Value* argv)
{
    if (argc != 3 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::SUBSTR_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    Fa_StringRef str = argv[0].as_string()->str;

    if (UNLIKELY(argv[1].is_nil() || argv[2].is_nil()))
        return Fa_Value::nil();

    i64 a = argv[1].as_int();
    i64 b = argv[2].as_int();

    Fa_StringRef ret = str.substr(a, b);
    return m_gc.make_string(ret);
}

Fa_Value Fa_VM::Fa_contains(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Fa_Value::nil();
    if (!argv[0].is_string() || !argv[1].is_string())
        return Fa_Value::nil();

    Fa_StringRef haystack = argv[0].as_string()->str;
    Fa_StringRef needle = argv[1].as_string()->str;
    if (needle.empty())
        return Fa_Value::from_bool(true);

    return Fa_Value::from_bool(haystack.find(needle));
}

Fa_Value Fa_VM::Fa_trim(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_Value::nil();
    if (!argv[0].is_string())
        return Fa_Value::nil();

    Fa_StringRef str = argv[0].as_string()->str;
    size_t start = 0;
    size_t end = str.len();

    auto is_trim_space = [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };

    while (start < end && is_trim_space(str[start]))
        start++;
    while (end > start && is_trim_space(str[end - 1]))
        end -= 1;

    return m_gc.make_string(str.substr_copy(start, end));
}

Fa_Value Fa_VM::Fa_floor(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::FLOOR_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    if (!argv[0].is_number()) {
        stdlib_error(StdlibErrorCode::FLOOR_TYPE_ERROR);
        return Fa_Value::nil();
    }

    if (argv[0].is_int())
        return argv[0];

    return Fa_Value::from_real(std::floor(argv[0].as_double()));
}

Fa_Value Fa_VM::Fa_ceil(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::CEIL_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    if (!argv[0].is_number()) {
        stdlib_error(StdlibErrorCode::CEIL_TYPE_ERROR);
        return Fa_Value::nil();
    }

    if (argv[0].is_int())
        return argv[0];

    return Fa_Value::from_real(std::ceil(argv[0].as_double_any()));
}

Fa_Value Fa_VM::Fa_round(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::ROUND_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }
    if (!argv[0].is_number()) {
        stdlib_error(StdlibErrorCode::ROUND_TYPE_ERROR);
        return Fa_Value::nil();
    }
    if (argv[0].is_int())
        return argv[0];

    return Fa_Value::from_real(std::round(argv[0].as_double_any()));
}

Fa_Value Fa_VM::Fa_abs(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::ABS_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    if (!argv[0].is_number()) {
        stdlib_error(StdlibErrorCode::ABS_TYPE_ERROR);
        return Fa_Value::nil();
    }

    if (argv[0].is_int()) {
        i64 v = argv[0].as_int();
        if (v == INT64_MIN) {
            stdlib_error(StdlibErrorCode::ABS_OUT_OF_RANGE);
            return Fa_Value::nil();
        }
        return Fa_Value::from_int(std::abs(argv[0].as_int()));
    }
    return Fa_Value::from_real(std::fabs(argv[0].as_double()));
}

Fa_Value Fa_VM::Fa_min(int argc, Fa_Value* argv)
{
    if (argc < 1 || !argv) {
        stdlib_error(StdlibErrorCode::MIN_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    // Determine mode from argv[0]
    bool all_ints = argv[0].is_int();
    bool all_strs = argv[0].is_string();

    // Validate all args match the expected type
    for (int i = 1; i < argc; i++) {
        if (!argv[i].is_int())
            all_ints = false;
        if (!argv[i].is_string())
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; i++) {
            if (argv[i].as_string()->str < ret.as_string()->str)
                ret = argv[i];
        }

        return ret;
    }

    Fa_Value ret = Fa_Value::from_real(argv[0].as_double_any());
    for (int i = 1; i < argc; i++)
        ret = Fa_Value::from_real(std::fmin(ret.as_double_any(), argv[i].as_double_any()));

    if (all_ints)
        return Fa_Value::from_int(static_cast<i64>(ret.as_double_any()));

    return ret;
}

Fa_Value Fa_VM::Fa_max(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::MAX_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    // Determine mode from argv[0]
    bool all_ints = argv[0].is_int();
    bool all_strs = argv[0].is_string();

    // Validate all args match the expected type
    for (int i = 1; i < argc; i++) {
        if (!argv[i].is_int())
            all_ints = false;
        if (!argv[i].is_string())
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; i++) {
            if (argv[i].as_string()->str > ret.as_string()->str)
                ret = argv[i];
        }
        return ret;
    }

    Fa_Value ret = Fa_Value::from_real(argv[0].as_double_any());
    for (int i = 1; i < argc; i++)
        ret = Fa_Value::from_real(std::fmax(ret.as_double_any(), argv[i].as_double_any()));

    if (all_ints)
        return Fa_Value::from_int(static_cast<i64>(ret.as_double_any()));

    return ret;
}

Fa_Value Fa_VM::Fa_pow(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::POW_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    Fa_Value base = argv[0];
    Fa_Value exponent = argv[1];

    if (UNLIKELY(!base.is_number() || !exponent.is_number())) {
        stdlib_error(StdlibErrorCode::POW_TYPE_ERROR);
        return Fa_Value::nil();
    }

    if (base.is_int() && exponent.is_int())
        // return an int even if the result may be larger than 48 bit range
        return Fa_Value::from_int(std::pow(base.as_int(), exponent.as_int()));
    else
        return Fa_Value::from_real(std::pow(base.as_double_any(), exponent.as_double_any()));

    return Fa_Value::nil();
}

Fa_Value Fa_VM::Fa_sqrt(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::SQRT_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_Value::nil();
    }

    Fa_Value n = argv[0];

    if (UNLIKELY(!n.is_number())) {
        stdlib_error(StdlibErrorCode::SQRT_TYPE_ERROR);
        return Fa_Value::nil();
    }

    f64 val = n.as_double_any();
    if (val < 0.0)
        return Fa_Value::nil();

    return Fa_Value::from_real(std::sqrt(val));
}

Fa_Value Fa_VM::Fa_assert(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::ASSERT_ARG_COUNT, "got" + std::to_string(argc));
        return Fa_Value::nil();
    }

    for (int i = 0; i < argc; i++) {
        if (!argv[i].is_truthy()) // eval entire expr
            stdlib_error(StdlibErrorCode::ASSERT_FAILED);
    }

    return Fa_Value::nil(); // success
}

Fa_Value Fa_VM::Fa_open(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::OPEN_ARG_COUNT);
        return Fa_Value::nil();
    }

    char const* filename = argv[0].as_string()->str.data();
    Fa_StringRef mode_arg = argv[1].as_string()->str;

    Fa_StringRef fmode;
    if (mode_arg == "اضف")
        fmode = "a";
    else if (mode_arg == "اقرا")
        fmode = "r";
    else if (mode_arg == "اكتب")
        fmode = "w";
    else
        runtime_error(RuntimeErrorCode::NATIVE_TYPE_ERROR);

    FILE* fp = fopen(filename, fmode.data());
    if (fp == NULL) {
        runtime_error(RuntimeErrorCode::NATIVE_TYPE_ERROR);
        return Fa_Value::nil();
    }

    return m_gc.make_file_handle(fp);
}

Fa_Value Fa_VM::Fa_append_file(int argc, Fa_Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_ARG_COUNT);
        return Fa_Value::nil();
    }

    Fa_Value& file = argv[0];
    Fa_Value& content = argv[1];

    if (!file.is_file_handle()) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_TYPE_ERROR);
        return Fa_Value::nil();
    }

    if (!content.is_string()) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_TYPE_ERROR);
        return Fa_Value::nil();
    }

    Fa_ObjFileHandle* file_handle = file.as_file_handle();
    Fa_ObjString* str_obj = content.as_string();
    FILE* fp = file_handle->fp;
    Fa_StringRef content_str = str_obj->str;

    if (content_str.empty())
        return Fa_Value::from_bool(true); // nothing to write is trivially successful

    size_t const written = std::fwrite(content_str.data(), 1, content_str.len(), fp);
    // ::fflush(fp);

    if (written != content_str.len()) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_FAILED, std::strerror(errno));
        return Fa_Value::from_bool(false);
    }

    return Fa_Value::from_bool(true);
}

Fa_Value Fa_VM::Fa_close(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::CLOSE_ARG_COUNT);
        return Fa_Value::from_bool(false);
    }

    if (!argv[0].is_file_handle()) {
        stdlib_error(StdlibErrorCode::CLOSE_TYPE_ERROR);
        return Fa_Value::from_bool(false);
    }

    Fa_ObjFileHandle* file_handle = argv[0].as_file_handle();
    if (file_handle->close())
        return Fa_Value::from_bool(true);

    return Fa_Value::from_bool(false);
}

Fa_Value Fa_VM::Fa_clock(int /*argc*/, Fa_Value* /*argv*/) { return Fa_Value::nil(); }
Fa_Value Fa_VM::Fa_error(int /*argc*/, Fa_Value* /*argv*/) { return Fa_Value::nil(); }
Fa_Value Fa_VM::Fa_time(int /*argc*/, Fa_Value* /*argv*/) { return Fa_Value::nil(); }

// stdlib helpers
void Fa_VM::Fa_dict_put(Fa_Value* dict_ptr, Fa_Value k, Fa_Value v)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return;

    Fa_ObjDict* as_dict = dict_ptr->as_dict();
    as_dict->data[k] = v;
}

Fa_Value Fa_VM::Fa_dict_get(Fa_Value* dict_ptr, Fa_Value k)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return Fa_Value::nil();

    Fa_ObjDict* as_dict = dict_ptr->as_dict();
    return as_dict->data[k];
}

} // namespace fairuz::runtime
