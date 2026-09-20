//
// stdlib.cc
//

#include "fbuiltins.hpp"
#include "fdiagnostic.hpp"
#include "fmacros.hpp"
#include "fobj_header.hpp"
#include "fobject.hpp"
#include "futil.hpp"
#include "fvalue.hpp"
#include "fvm.hpp"
#include <algorithm>
#include <array>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include <simdutf.h>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <zlib.h>

namespace fairuz::runtime {

static constexpr u32 MAX_RENDER_DEPTH = 128;

static StringRef format_double_string(f64 value)
{
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), value);
    if (ec == std::errc())
        return StringRef(std::string(buf, static_cast<size_t>(ptr - buf)).c_str());

    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::setprecision(14) << std::noshowpoint << value;
    return StringRef(oss.str().c_str());
}

static void append_rendered_value(StringRef& out, Value v, bool quote_strings,
    std::unordered_set<ObjHeader*>& active_containers, u32 depth)
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
        out += StringRef(std::to_string(v.as_int()).c_str());
        return;
    }
    if (v.is_double()) {
        f64 d = v.as_double();
        if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15)
            out += StringRef(std::to_string(static_cast<i64>(d)).c_str());
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
    if ((v.is_list() || v.is_dict()) && depth >= MAX_RENDER_DEPTH) {
        out += "<max-depth>";
        return;
    }
    if (v.is_list()) {
        ObjList* list = v.as_list();
        if (!active_containers.insert(&list->obj).second) {
            out += "<cycle>";
            return;
        }
        out += '[';
        for (u32 i = 0, n = list->elements.size(); i < n; i++) {
            if (i > 0)
                out += ", ";
            append_rendered_value(out, list->elements[i], true, active_containers, depth + 1);
        }
        out += ']';
        active_containers.erase(&list->obj);
        return;
    }
    if (v.is_dict()) {
        ObjDict* dict = v.as_dict();
        if (!active_containers.insert(&dict->obj).second) {
            out += "<cycle>";
            return;
        }
        out += '{';
        u32 i = 0, end = dict->data.size() - 1;
        for (auto [k, v] : dict->data) {
            append_rendered_value(out, k, k.is_string(), active_containers, depth + 1);
            out += ": ";
            append_rendered_value(out, v, v.is_string(), active_containers, depth + 1);
            if (i == end)
                break;
            out += ", ";
            i++;
        }

        out += '}';
        active_containers.erase(&dict->obj);
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
        ObjClass* klass = v.as_class();
        out += klass->name;
        out += '>';
        return;
    }
    if (v.is_instance()) {
        out += '<';
        ObjInstance* instance = v.as_instance();
        out += instance->klass->name;
        out += " instance>";
        return;
    }
}

static StringRef value_to_string(Value v)
{
    StringRef out = "";
    std::unordered_set<ObjHeader*> active_containers;
    append_rendered_value(out, v, false, active_containers, 0);
    return out;
}

Value VM::len(int argc, Value* argv)
{
    if (argc == 0 || argv == nullptr)
        return Value::nil();

    if (argc == 1) {
        if (argv[0].is_string()) {
            StringRef const& str = argv[0].as_string()->str;
            // Strings also serve as the language's byte container at native
            // boundaries (files, codecs, compression).  Preserve Unicode
            // code-point length for text, but never try to decode arbitrary
            // binary payloads such as gzip streams.
            if (!simdutf::validate_utf8(str.data(), str.len()))
                return Value::from_int(static_cast<i64>(str.len()));
            size_t byte_pos = 0;
            i64 char_count = 0;

            while (byte_pos < str.len()) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_count++;
            }

            return Value::from_int(char_count);
        }

        if (argv[0].is_list())
            return Value::from_int(argv[0].as_list()->elements.size());
        if (argv[0].is_dict())
            return Value::from_int(argv[0].as_dict()->data.size());
    }

    /// do not accept multiple args for len
    return Value::nil();
}

static void print_runtime_value_impl(Value v,
    std::unordered_set<ObjHeader*>& active_containers, u32 depth)
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
        ObjHeader* obj = v.as_obj();

        if ((obj->type == ObjType::LIST || obj->type == ObjType::DICT)
            && depth >= MAX_RENDER_DEPTH) {
            std::cout << "<max-depth>";
            return;
        }

        switch (obj->type) {
        case ObjType::STRING:
            std::cout << reinterpret_cast<ObjString*>(obj)->str;
            return;

        case ObjType::LIST: {
            auto list = reinterpret_cast<ObjList*>(obj);
            if (!active_containers.insert(&list->obj).second) {
                std::cout << "<cycle>";
                return;
            }
            std::cout << '[';
            for (u32 i = 0, n = list->size(); i < n; i++) {
                if (i > 0)
                    std::cout << ", ";
                Value elem = list->elements[i];
                if (elem.is_obj() && elem.as_obj()->type == ObjType::STRING) {
                    std::cout << '"';
                    std::cout << elem.as_string()->str;
                    std::cout << '"';
                } else {
                    print_runtime_value_impl(elem, active_containers, depth + 1);
                }
            }
            std::cout << ']';
            active_containers.erase(&list->obj);
            return;
        }

        case ObjType::DICT: {
            auto dict = reinterpret_cast<ObjDict*>(obj);
            if (!active_containers.insert(&dict->obj).second) {
                std::cout << "<cycle>";
                return;
            }
            std::cout << '{';
            for (auto [k, v] : dict->data) {
                if (k.is_string()) {
                    std::cout << '"';
                    std::cout << k.as_string()->str;
                    std::cout << '"';
                } else {
                    print_runtime_value_impl(k, active_containers, depth + 1);
                }
                std::cout << ": ";
                if (v.is_string()) {
                    std::cout << '"';
                    std::cout << v.as_string()->str;
                    std::cout << '"';
                } else {
                    print_runtime_value_impl(v, active_containers, depth + 1);
                }
                std::cout << ", ";
            }
            std::cout << '}';
            active_containers.erase(&dict->obj);
            return;
        }

        case ObjType::NATIVE: {
            auto nat = reinterpret_cast<ObjNative*>(obj);
            std::cout << "<native ";
            if (nat->name)
                std::cout << nat->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case ObjType::FUNCTION: {
            auto* fn = reinterpret_cast<ObjFunction*>(obj);
            std::cout << "<function ";
            std::cout << fn->name();
            std::cout << '>';
            return;
        }

        case ObjType::CLASS: {
            auto klass = reinterpret_cast<ObjClass*>(obj);
            std::cout << "<class " << klass->name << '>';
            return;
        }

        case ObjType::INSTANCE: {
            auto instance = reinterpret_cast<ObjInstance*>(obj);
            std::cout << '<';
            if (instance->klass)
                std::cout << instance->klass->name;
            else
                std::cout << "?";
            std::cout << " instance>";
            return;
        }

        case ObjType::FILE_HANDLE: {
            auto file_handle = reinterpret_cast<ObjFileHandle*>(obj);
            std::cout << '{' << '\n';
            std::cout << '\t' << "ptr: " << file_handle->fp << '\n';
            std::cout << '\t' << "is_open: " << (file_handle->is_open ? "true" : "false") << '\n';
            std::cout << '}';
            return;
        }

        case ObjType::MODULE: {
            auto module = reinterpret_cast<ObjModule*>(obj);
            std::cout << "<module " << module->name << '>';
            return;
        }
#if FA_USE_NANBOX
        case fairuz::runtime::ObjType::INT: {
            auto int_obj = reinterpret_cast<ObjBigInt*>(obj);
            /// add '<' and '>' to distinguish a fallback int object from a NAN-BOXed int value
            std::cout << "<" << int_obj->val << ">" << '\n';
            return;
        }
#endif

        case ObjType::_COUNT:
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

static void print_runtime_value(Value v)
{
    std::unordered_set<ObjHeader*> active_containers;
    print_runtime_value_impl(v, active_containers, 0);
}

Value VM::print(int argc, Value* argv)
{
    if (argc == 0 || argv == nullptr) {
        std::cout << '\n';
        return Value::nil();
    }

    for (int i = 0; i < argc; i++) {
        if (i > 0)
            std::cout << '\t';
        print_runtime_value(argv[i]);
    }
    std::cout << '\n';
    return Value::nil();
}

Value VM::type(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Value::nil();

    Value& v = argv[0];
    TypeTag type = value_type_tag(v);
    switch (type) {
    case TypeTag::NONE: return m_gc.make_string("لاشيء");
    case TypeTag::NIL: return m_gc.make_string("عدم");
    case TypeTag::BOOL: return m_gc.make_string("منطقي");
    case TypeTag::INT: return m_gc.make_string("طبيعي");
    case TypeTag::DOUBLE: return m_gc.make_string("حقيقي");
    case TypeTag::STRING: return m_gc.make_string("سلسلة");
    case TypeTag::LIST: return m_gc.make_string("قائمة");
    case TypeTag::FUNCTION: return m_gc.make_string("دالة");
    case TypeTag::NATIVE: return m_gc.make_string("دالة");
    case TypeTag::CLASS: return m_gc.make_string(v.as_class()->name);
    case TypeTag::INSTANCE: return m_gc.make_string(v.as_instance()->klass->name);
    case TypeTag::DICT: return m_gc.make_string("قاموس");
    case TypeTag::FILE_HANDLE: return m_gc.make_string("ملف");
    case TypeTag::MODULE: return m_gc.make_string("وحدة");
    }

    return Value::nil(); // unreachable
}

Value VM::Int(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Value::nil();
    if (argv[0].is_number())
        return Value::from_int(static_cast<i64>(argv[0].as_double_any()));
    return Value::nil();
}

Value VM::Float(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Value::nil();

    if (argv[0].is_number())
        return Value::from_real(argv[0].as_double_any());

    return Value::nil();
}

Value VM::append(int argc, Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        raise_error(ErrorCode::APPEND_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value& list_v = argv[0];
    if (!list_v.is_list()) {
        raise_error(ErrorCode::APPEND_TYPE_ERROR);
        return Value::nil();
    }

    ObjList* list_obj = list_v.as_list();

    for (int i = 1; i < argc; i++)
        list_obj->elements.push(argv[i]);

    return Value::nil();
}

Value VM::pop(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::POP_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value& list_v = argv[0];
    if (!list_v.is_list()) {
        raise_error(ErrorCode::POP_TYPE_ERROR);
        return Value::nil();
    }

    ObjList* list_obj = list_v.as_list();

    if (list_obj->empty())
        raise_error(ErrorCode::POP_EMPTY_LIST);

    list_obj->elements.pop();
    return list_v;
}

Value VM::slice(int argc, Value* argv)
{
    /// cut a copy of a container, with inclusive indices
    /// accept [container, start, end]
    /// a, b are the indices
    /// if b is null then cut [start:]

    if (argc < 2 || argc > 3 || argv == nullptr) {
        raise_error(ErrorCode::SLICE_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value container = argv[0];
    Value start_value = argv[1];
    Value end_value = argc < 3 ? Value::nil() : argv[2];

    if (UNLIKELY(!container.is_string() && !container.is_list()))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "slice expects a string or list as its first argument");
    if (UNLIKELY(!start_value.is_int() || !(end_value.is_nil() || end_value.is_int())))
        raise_error(ErrorCode::INDEX_TYPE_ERROR);

    i64 start_i = start_value.as_int();
    size_t container_size = container.is_string()
        ? container.as_string()->str.len()
        : static_cast<size_t>(container.as_list()->size());

    if (container_size == 0) {
        if (start_i == 0 && end_value.is_nil())
            return container.is_string() ? m_gc.make_string("") : m_gc.make_list();
        raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);
    }

    i64 end_i = end_value.is_nil()
        ? static_cast<i64>(container_size - 1)
        : end_value.as_int();

    if (UNLIKELY(start_i < 0 || end_i < 0 || start_i > end_i
            || static_cast<u64>(start_i) >= container_size
            || static_cast<u64>(end_i) >= container_size))
        raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

    size_t start = static_cast<size_t>(start_i);
    size_t end = static_cast<size_t>(end_i);

    if (container.is_string()) {
        ObjString* str_obj = container.as_string();
        return m_gc.make_string(str_obj->str.slice(start, end + 1));
    } else if (container.is_list()) {
        ObjList* list_obj = container.as_list();
        ObjList* ret_list = m_gc.make_obj_list();
        for (size_t i = start; i <= end; i++)
            ret_list->elements.push(list_obj->elements[static_cast<u32>(i)]);

        return Value::from_list(ret_list);
    }

    return Value::nil();
}

Value VM::input(int /*argc*/, Value* /*argv*/) // input takes no args for now
{
    // read until user hits ENTER
    StringRef ret_str = "";
    std::string help = ""; // getline only accepts std::string

    if (!std::getline(std::cin, help))
        // don't know what error to report
        return Value::nil();

    ret_str = help.data();
    Value ret = m_gc.make_string(ret_str);
    return ret;
}

Value VM::str(int argc, Value* argv)
{
    if (argc > 1) {
        raise_error(ErrorCode::STR_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    StringRef output = "";

    if (argc == 0 || argv == nullptr)
        return m_gc.make_string(output); // return empty on no arg

    if (argv[0].is_string())
        return m_gc.make_string(argv[0].as_string()->str);

    StringRef rendered = value_to_string(argv[0]);
    return m_gc.make_string(rendered);
}

Value VM::Bool(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::BOOL_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    return argv[0].is_truthy() ? Value::from_bool(true) : Value::from_bool(false);
}

Value VM::list(int argc, Value* argv)
{
    Value ret = m_gc.make_list();
    ObjList* list_obj = ret.as_list();

    for (int i = 0; i < argc; i++)
        list_obj->elements.push(argv[i]);

    return ret;
}

Value VM::dict(int argc, Value* argv)
{
    Value ret = m_gc.make_dict();
    if (argc <= 0 || argv == nullptr)
        return ret;

    ObjDict* dict_obj = ret.as_dict();
    for (int i = 0; i + 1 < argc; i += 2)
        dict_obj->set(argv[i], argv[i + 1]);

    return ret;
}

Value VM::dict_keys(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dictionary keys expects one dictionary");

    Value result = m_gc.make_list();
    ObjDict* dict = argv[0].as_dict();
    for (Value key : dict->insertion_order)
        result.as_list()->elements.push(key);
    return result;
}

Value VM::dict_contains(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dictionary contains expects a dictionary and key");
    return Value::from_bool(argv[0].as_dict()->data.contains(argv[1]));
}

Value VM::dict_delete(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dictionary delete expects a dictionary and key");
    Value removed = Value::nil();
    argv[0].as_dict()->erase(argv[1], &removed);
    return removed;
}

Value VM::split(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Value::nil();
    if (!argv[0].is_string() || !argv[1].is_string())
        return Value::nil();

    StringRef src = argv[0].as_string()->str;
    StringRef delim = argv[1].as_string()->str;

    Value ret = m_gc.make_list();
    ObjList* list = ret.as_list();

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

Value VM::join(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Value::nil();
    if (!argv[0].is_list() || !argv[1].is_string())
        return Value::nil();

    ObjList* list = argv[0].as_list();
    StringRef delim = argv[1].as_string()->str;
    StringRef out = "";

    for (u32 i = 0; i < list->elements.size(); i++) {
        if (i > 0)
            out += delim;

        out += value_to_string(list->elements[i]);
    }

    return m_gc.make_string(out);
}

Value VM::substr(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr) {
        raise_error(ErrorCode::SUBSTR_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    if (UNLIKELY(!argv[0].is_string() || !argv[1].is_int() || !argv[2].is_int()))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "substr expects (string, integer, integer)");

    StringRef str = argv[0].as_string()->str;
    i64 start_cp = argv[1].as_int();
    i64 end_cp = argv[2].as_int();
    if (start_cp < 0 || end_cp < start_cp)
        raise_error(ErrorCode::INDEX_OUT_OF_BOUNDS);

    size_t byte_pos = 0;
    i64 cp_pos = 0;
    size_t start_byte = str.len();
    size_t end_byte = str.len();
    while (byte_pos < str.len()) {
        if (cp_pos == start_cp)
            start_byte = byte_pos;
        if (cp_pos == end_cp) {
            end_byte = byte_pos;
            break;
        }
        u64 step = 0;
        util::decode_utf8_at(str, byte_pos, &step);
        byte_pos += step;
        cp_pos++;
    }
    if (cp_pos == start_cp)
        start_byte = byte_pos;
    if (cp_pos <= end_cp)
        end_byte = byte_pos;
    if (start_cp > cp_pos)
        start_byte = end_byte = str.len();
    return m_gc.make_string(str.slice(start_byte, end_byte));
}

Value VM::contains(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Value::nil();
    if (!argv[0].is_string() || !argv[1].is_string())
        return Value::nil();

    StringRef haystack = argv[0].as_string()->str;
    StringRef needle = argv[1].as_string()->str;
    if (needle.empty())
        return Value::from_bool(true);

    return Value::from_bool(haystack.find(needle));
}

Value VM::trim(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Value::nil();
    if (!argv[0].is_string())
        return Value::nil();

    StringRef str = argv[0].as_string()->str;
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

Value VM::char_from_codepoint(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_int())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "character conversion expects one integer");
    i64 value = argv[0].as_int();
    if (value < 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        raise_error(ErrorCode::NUMERIC_OUT_OF_RANGE);
    return m_gc.make_string(util::encode_utf8_str(static_cast<u32>(value)));
}

Value VM::number_from_text(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "number parsing expects one string");
    StringRef text = argv[0].as_string()->str;
    if (text.empty() || text[0] == '+')
        return Value::nil();

    bool integral = true;
    for (size_t i = 0; i < text.len(); ++i) {
        if (text[i] == '.' || text[i] == 'e' || text[i] == 'E') {
            integral = false;
            break;
        }
    }
    if (integral) {
        i64 value = 0;
        auto parsed = std::from_chars(text.data(), text.data() + text.len(), value);
        if (parsed.ec == std::errc() && parsed.ptr == text.data() + text.len())
            return Value::from_int(value);
        return Value::nil();
    }

    std::string owned(text.data(), text.len());
    char* end = nullptr;
    errno = 0;
    double value = std::strtod(owned.c_str(), &end);
    if (errno == ERANGE || end != owned.c_str() + owned.size() || !std::isfinite(value))
        return Value::nil();
    return Value::from_real(value);
}

Value VM::number_finite(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_number())
        return Value::from_bool(false);
    return Value::from_bool(argv[0].is_int() || std::isfinite(argv[0].as_double()));
}

Value VM::number_is_nan(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_number())
        return Value::from_bool(false);
    return Value::from_bool(argv[0].is_double() && std::isnan(argv[0].as_double()));
}

Value VM::json_escape(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "JSON escaping expects one string");
    StringRef input = argv[0].as_string()->str;
    std::string output;
    output.reserve(input.len());
    static constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < input.len(); ++i) {
        unsigned char ch = static_cast<unsigned char>(input[i]);
        switch (ch) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (ch < 0x20) {
                output += "\\u00";
                output.push_back(hex[ch >> 4]);
                output.push_back(hex[ch & 0x0F]);
            } else {
                output.push_back(static_cast<char>(ch));
            }
        }
    }
    return m_gc.make_string(output.c_str());
}

Value VM::json_read_string(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_int())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "JSON string parsing expects a string and integer position");

    StringRef input = argv[0].as_string()->str;
    i64 requested = argv[1].as_int();
    if (requested < 0)
        return Value::nil();

    size_t byte_pos = 0;
    i64 cp_pos = 0;
    while (byte_pos < input.len() && cp_pos < requested) {
        u64 step = 0;
        util::decode_utf8_at(input, byte_pos, &step);
        byte_pos += step;
        cp_pos++;
    }
    if (cp_pos != requested || byte_pos >= input.len() || input[byte_pos] != '"')
        return Value::nil();

    byte_pos++;
    cp_pos++;
    std::string output;
    auto hex_digit = [](char ch) -> int {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return -1;
    };
    auto read_hex4 = [&](size_t at, u32* value) -> bool {
        if (at + 4 > input.len())
            return false;
        u32 result = 0;
        for (size_t i = 0; i < 4; ++i) {
            int digit = hex_digit(input[at + i]);
            if (digit < 0)
                return false;
            result = (result << 4) | static_cast<u32>(digit);
        }
        *value = result;
        return true;
    };

    while (byte_pos < input.len()) {
        unsigned char ch = static_cast<unsigned char>(input[byte_pos]);
        if (ch == '"') {
            Value result = m_gc.make_list();
            Value decoded = m_gc.make_string(output.c_str());
            result.as_list()->elements.push(decoded);
            result.as_list()->elements.push(Value::from_int(cp_pos + 1));
            return result;
        }
        if (ch < 0x20)
            return Value::nil();
        if (ch != '\\') {
            u64 step = 0;
            util::decode_utf8_at(input, byte_pos, &step);
            output.append(input.data() + byte_pos, static_cast<size_t>(step));
            byte_pos += step;
            cp_pos++;
            continue;
        }

        if (byte_pos + 1 >= input.len())
            return Value::nil();
        char escape = input[byte_pos + 1];
        switch (escape) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u': {
            u32 codepoint = 0;
            if (!read_hex4(byte_pos + 2, &codepoint))
                return Value::nil();
            size_t consumed = 6;
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (byte_pos + 12 > input.len() || input[byte_pos + 6] != '\\'
                    || input[byte_pos + 7] != 'u')
                    return Value::nil();
                u32 low = 0;
                if (!read_hex4(byte_pos + 8, &low) || low < 0xDC00 || low > 0xDFFF)
                    return Value::nil();
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                consumed = 12;
            } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                return Value::nil();
            }
            StringRef encoded = util::encode_utf8_str(codepoint);
            output.append(encoded.data(), encoded.len());
            byte_pos += consumed;
            cp_pos += static_cast<i64>(consumed);
            continue;
        }
        default: return Value::nil();
        }
        byte_pos += 2;
        cp_pos += 2;
    }
    return Value::nil();
}

Value VM::dynamic_call(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[1].is_list())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "dynamic call expects a callable and argument list");
    return call_value_sync(argv[0], argv[1].as_list());
}

namespace {

Value* dict_field(ObjDict* dict, Value key)
{
    return dict == nullptr ? nullptr : dict->data.find_ptr(key);
}

}

Value VM::executor_new(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_int() || argv[0].as_int() <= 0)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "executor expects a positive worker count");
    Value result = m_gc.make_dict();
    result.as_dict()->set(m_gc.make_string("kind"), m_gc.make_string("executor"));
    result.as_dict()->set(m_gc.make_string("closed"), Value::from_bool(false));
    return result;
}

Value VM::executor_close(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_bool())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "executor close expects a handle and wait flag");
    argv[0].as_dict()->set(m_gc.make_string("closed"), Value::from_bool(true));
    return Value::from_bool(true);
}

Value VM::task_start(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_dict() || !argv[2].is_list())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "task start expects executor, callable, and argument list");
    Value closed_key = m_gc.make_string("closed");
    Value* closed = dict_field(argv[0].as_dict(), closed_key);
    if (closed != nullptr && closed->is_truthy())
        raise_error(ErrorCode::TYPE_ERROR_CALL, "executor is closed");

    Value value = call_value_sync(argv[1], argv[2].as_list());
    Value task = m_gc.make_dict();
    task.as_dict()->set(m_gc.make_string("kind"), m_gc.make_string("task"));
    task.as_dict()->set(m_gc.make_string("done"), Value::from_bool(true));
    task.as_dict()->set(m_gc.make_string("cancelled"), Value::from_bool(false));
    task.as_dict()->set(m_gc.make_string("result"), value);
    return task;
}

Value VM::task_done(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "task done expects a handle");
    Value key = m_gc.make_string("done");
    Value* value = dict_field(argv[0].as_dict(), key);
    return Value::from_bool(value != nullptr && value->is_truthy());
}

Value VM::task_result(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_number())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "task result expects a handle and timeout");
    Value key = m_gc.make_string("result");
    Value* value = dict_field(argv[0].as_dict(), key);
    return value == nullptr ? Value::nil() : *value;
}

Value VM::task_cancel(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "task cancel expects a handle");
    Value done_key = m_gc.make_string("done");
    Value* done = dict_field(argv[0].as_dict(), done_key);
    if (done != nullptr && done->is_truthy())
        return Value::from_bool(false);
    argv[0].as_dict()->set(m_gc.make_string("cancelled"), Value::from_bool(true));
    argv[0].as_dict()->set(done_key, Value::from_bool(true));
    return Value::from_bool(true);
}

Value VM::task_wait_all(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_list() || !argv[1].is_number())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "wait all expects task handles and timeout");
    Value results = m_gc.make_list();
    Value result_key = m_gc.make_string("result");
    for (Value handle : argv[0].as_list()->elements) {
        if (!handle.is_dict())
            raise_error(ErrorCode::NATIVE_TYPE_ERROR, "invalid task handle");
        Value* value = dict_field(handle.as_dict(), result_key);
        results.as_list()->elements.push(value == nullptr ? Value::nil() : *value);
    }
    return results;
}

Value VM::file_open(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file open expects path and mode strings");
    StringRef path = argv[0].as_string()->str;
    StringRef mode = argv[1].as_string()->str;
    if (::memchr(path.data(), '\0', path.len()) != nullptr)
        return Value::nil();
    char const* native_mode = nullptr;
    if (mode == "قراءة" || mode == "اقرا")
        native_mode = "rb";
    else if (mode == "كتابة" || mode == "اكتب")
        native_mode = "wb";
    else if (mode == "اضافة" || mode == "اضف")
        native_mode = "ab+";
    else
        return Value::nil();
    FILE* file = std::fopen(path.data(), native_mode);
    return file == nullptr ? Value::nil() : m_gc.make_file_handle(file);
}

Value VM::file_read(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_file_handle() || !argv[1].is_int()
        || argv[1].as_int() < 0)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file read expects an open handle and non-negative byte count");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return Value::nil();
    size_t requested = static_cast<size_t>(argv[1].as_int());
    std::string output(requested, '\0');
    size_t count = requested == 0 ? 0 : std::fread(output.data(), 1, requested, handle->fp);
    output.resize(count);
    return m_gc.make_string(output.c_str());
}

Value VM::file_read_all(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_file_handle())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "read all expects an open file handle");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return Value::nil();
    std::string output;
    char buffer[8192];
    for (;;) {
        size_t count = std::fread(buffer, 1, sizeof(buffer), handle->fp);
        output.append(buffer, count);
        if (count < sizeof(buffer))
            break;
    }
    if (std::ferror(handle->fp))
        return Value::nil();
    return m_gc.make_string(output.c_str());
}

Value VM::file_read_line(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_file_handle())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "read line expects an open file handle");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return Value::nil();
    std::string output;
    int ch = 0;
    while ((ch = std::fgetc(handle->fp)) != EOF) {
        if (ch == '\n')
            break;
        output.push_back(static_cast<char>(ch));
    }
    if (ch == EOF && output.empty())
        return Value::nil();
    if (!output.empty() && output.back() == '\r')
        output.pop_back();
    return m_gc.make_string(output.c_str());
}

Value VM::file_write(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_file_handle() || !argv[1].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file write expects an open handle and string");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return Value::from_bool(false);
    StringRef text = argv[1].as_string()->str;
    size_t written = text.empty() ? 0 : std::fwrite(text.data(), 1, text.len(), handle->fp);
    return Value::from_bool(written == text.len());
}

Value VM::file_flush(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_file_handle())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file flush expects an open handle");
    ObjFileHandle* handle = argv[0].as_file_handle();
    return Value::from_bool(handle->is_open && handle->fp != nullptr
        && std::fflush(handle->fp) == 0);
}

namespace {

std::string native_path(Value value)
{
    if (!value.is_string())
        return { };
    StringRef path = value.as_string()->str;
    return std::string(path.data(), path.len());
}

std::string wildcard_regex(std::string const& pattern)
{
    std::string result = "^";
    for (char ch : pattern) {
        if (ch == '*')
            result += ".*";
        else if (ch == '?')
            result += '.';
        else {
            if (ch == '.' || ch == '+' || ch == '(' || ch == ')' || ch == '[' || ch == ']'
                || ch == '{' || ch == '}' || ch == '^' || ch == '$' || ch == '|' || ch == '\\')
                result.push_back('\\');
            result.push_back(ch);
        }
    }
    result.push_back('$');
    return result;
}

std::filesystem::path temporary_parent(Value value)
{
    if (value.is_nil())
        return std::filesystem::temp_directory_path();
    std::string path = native_path(value);
    return path.empty() ? std::filesystem::temp_directory_path() : std::filesystem::path(path);
}

std::filesystem::path unique_temporary_path(std::filesystem::path const& parent,
    std::string const& prefix, std::string const& suffix)
{
    static std::atomic<unsigned long long> counter { 0 };
    auto seed = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    for (unsigned attempt = 0; attempt < 1000; ++attempt) {
        auto id = seed ^ (++counter * 0x9E3779B97F4A7C15ULL) ^ attempt;
        auto candidate = parent / (prefix + std::to_string(id) + suffix);
        if (!std::filesystem::exists(candidate))
            return candidate;
    }
    return { };
}

}

Value VM::path_delete(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "path delete expects a string");
    std::error_code error;
    auto count = std::filesystem::remove_all(native_path(argv[0]), error);
    return Value::from_bool(!error && count > 0);
}

Value VM::path_glob(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_bool())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "glob expects a pattern and recursive flag");
    std::filesystem::path pattern(native_path(argv[0]));
    std::filesystem::path parent = pattern.parent_path();
    if (parent.empty())
        parent = ".";
    std::regex matcher(wildcard_regex(pattern.filename().string()));
    Value result = m_gc.make_list();
    std::error_code error;
    if (argv[1].as_bool()) {
        for (std::filesystem::recursive_directory_iterator it(parent, error), end; !error && it != end; it.increment(error)) {
            if (std::regex_match(it->path().filename().string(), matcher))
                result.as_list()->elements.push(m_gc.make_string(it->path().string().c_str()));
        }
    } else {
        for (std::filesystem::directory_iterator it(parent, error), end; !error && it != end; it.increment(error)) {
            if (std::regex_match(it->path().filename().string(), matcher))
                result.as_list()->elements.push(m_gc.make_string(it->path().string().c_str()));
        }
    }
    return result;
}

Value VM::temp_file(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !(argv[2].is_nil() || argv[2].is_string()))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "temporary file expects prefix, suffix, and optional directory");
    std::filesystem::path path = unique_temporary_path(temporary_parent(argv[2]),
        native_path(argv[0]), native_path(argv[1]));
    if (path.empty())
        return Value::nil();
    std::ofstream created(path, std::ios::binary | std::ios::trunc);
    if (!created)
        return Value::nil();
    created.close();
    Value result = m_gc.make_dict();
    result.as_dict()->set(m_gc.make_string("path"), m_gc.make_string(path.string().c_str()));
    return result;
}

Value VM::temp_directory(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string()
        || !(argv[1].is_nil() || argv[1].is_string()))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "temporary directory expects prefix and optional parent");
    std::filesystem::path path = unique_temporary_path(temporary_parent(argv[1]),
        native_path(argv[0]), "");
    std::error_code error;
    if (path.empty() || !std::filesystem::create_directory(path, error) || error)
        return Value::nil();
    return m_gc.make_string(path.string().c_str());
}

Value VM::remove_tree(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "tree removal expects a path string");
    std::error_code error;
    std::filesystem::remove_all(native_path(argv[0]), error);
    return Value::from_bool(!error);
}

namespace {

bool utc_zone(Value value)
{
    return value.is_string() && value.as_string()->str == "UTC";
}

bool supported_zone(Value value)
{
    return value.is_string()
        && (value.as_string()->str == "UTC" || value.as_string()->str == "محلي");
}

bool calendar_fields(std::time_t timestamp, bool utc, std::tm* output)
{
#if defined(_WIN32)
    return (utc ? ::gmtime_s(output, &timestamp) : ::localtime_s(output, &timestamp)) == 0;
#else
    return (utc ? ::gmtime_r(&timestamp, output) : ::localtime_r(&timestamp, output)) != nullptr;
#endif
}

std::time_t utc_timestamp(std::tm* value)
{
#if defined(_WIN32)
    return ::_mkgmtime(value);
#else
    return ::timegm(value);
#endif
}

}

Value VM::datetime_now(int argc, Value* argv)
{
    (void)argv;
    if (argc != 0)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR, "current time takes no arguments");
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return Value::from_int(std::chrono::duration_cast<std::chrono::seconds>(now).count());
}

Value VM::datetime_from_fields(int argc, Value* argv)
{
    if (argc != 7 || argv == nullptr || !supported_zone(argv[6]))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime construction expects six integers and a supported zone");
    for (int i = 0; i < 6; ++i) {
        if (!argv[i].is_int())
            raise_error(ErrorCode::NATIVE_TYPE_ERROR,
                "datetime fields must be integers");
    }
    std::tm value { };
    value.tm_year = static_cast<int>(argv[0].as_int() - 1900);
    value.tm_mon = static_cast<int>(argv[1].as_int() - 1);
    value.tm_mday = static_cast<int>(argv[2].as_int());
    value.tm_hour = static_cast<int>(argv[3].as_int());
    value.tm_min = static_cast<int>(argv[4].as_int());
    value.tm_sec = static_cast<int>(argv[5].as_int());
    value.tm_isdst = -1;
    std::time_t timestamp = utc_zone(argv[6]) ? utc_timestamp(&value) : std::mktime(&value);
    if (timestamp == static_cast<std::time_t>(-1))
        return Value::nil();
    return Value::from_int(static_cast<i64>(timestamp));
}

Value VM::datetime_to_fields(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_number() || !supported_zone(argv[1]))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime fields expect epoch and supported zone");
    std::time_t timestamp = static_cast<std::time_t>(argv[0].as_double_any());
    std::tm fields { };
    if (!calendar_fields(timestamp, utc_zone(argv[1]), &fields))
        return Value::nil();
    Value result = m_gc.make_list();
    i64 values[] = {
        fields.tm_year + 1900,
        fields.tm_mon + 1,
        fields.tm_mday,
        fields.tm_hour,
        fields.tm_min,
        fields.tm_sec,
        fields.tm_wday,
        fields.tm_yday + 1,
    };
    for (i64 value : values)
        result.as_list()->elements.push(Value::from_int(value));
    return result;
}

Value VM::datetime_parse(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !supported_zone(argv[2]))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime parse expects text, format, and supported zone");
    std::string text = native_path(argv[0]);
    std::string format = native_path(argv[1]);
    if (format == "ISO8601")
        format = "%Y-%m-%dT%H:%M:%SZ";
    std::tm value { };
    std::istringstream stream(text);
    stream >> std::get_time(&value, format.c_str());
    if (stream.fail() || stream.peek() != std::char_traits<char>::eof())
        return Value::nil();
    value.tm_isdst = -1;
    std::time_t timestamp = utc_zone(argv[2]) ? utc_timestamp(&value) : std::mktime(&value);
    return timestamp == static_cast<std::time_t>(-1)
        ? Value::nil()
        : Value::from_int(static_cast<i64>(timestamp));
}

Value VM::datetime_format(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_number() || !supported_zone(argv[1])
        || !argv[2].is_string())
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "datetime format expects epoch, zone, and format");
    std::time_t timestamp = static_cast<std::time_t>(argv[0].as_double_any());
    std::tm fields { };
    if (!calendar_fields(timestamp, utc_zone(argv[1]), &fields))
        return Value::nil();
    std::string format = native_path(argv[2]);
    if (format == "ISO8601")
        format = utc_zone(argv[1]) ? "%Y-%m-%dT%H:%M:%SZ" : "%Y-%m-%dT%H:%M:%S%z";
    char output[256];
    size_t size = std::strftime(output, sizeof(output), format.c_str(), &fields);
    return size == 0 ? Value::nil() : m_gc.make_string(output);
}

namespace {

StringRef byte_string(std::string_view bytes)
{
    StringRef result(bytes.size(), '\0');
    if (!bytes.empty())
        std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

std::string_view string_bytes(Value value)
{
    StringRef const& text = value.as_string()->str;
    return { text.data(), text.len() };
}

constexpr char BASE64_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string encode_base64(std::string_view input, bool url_safe)
{
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t offset = 0; offset < input.size(); offset += 3) {
        size_t remaining = input.size() - offset;
        u32 block = static_cast<u8>(input[offset]) << 16;
        if (remaining > 1)
            block |= static_cast<u8>(input[offset + 1]) << 8;
        if (remaining > 2)
            block |= static_cast<u8>(input[offset + 2]);

        output.push_back(BASE64_ALPHABET[(block >> 18) & 0x3f]);
        output.push_back(BASE64_ALPHABET[(block >> 12) & 0x3f]);
        output.push_back(remaining > 1 ? BASE64_ALPHABET[(block >> 6) & 0x3f] : '=');
        output.push_back(remaining > 2 ? BASE64_ALPHABET[block & 0x3f] : '=');
    }

    if (url_safe) {
        std::replace(output.begin(), output.end(), '+', '-');
        std::replace(output.begin(), output.end(), '/', '_');
    }
    return output;
}

int base64_value(char character, bool url_safe)
{
    if (character >= 'A' && character <= 'Z')
        return character - 'A';
    if (character >= 'a' && character <= 'z')
        return character - 'a' + 26;
    if (character >= '0' && character <= '9')
        return character - '0' + 52;
    if (character == '+' || (url_safe && character == '-'))
        return 62;
    if (character == '/' || (url_safe && character == '_'))
        return 63;
    return -1;
}

bool decode_base64(std::string_view input, bool url_safe, std::string& output)
{
    output.clear();
    if (input.empty())
        return true;

    size_t padding = 0;
    while (padding < input.size() && input[input.size() - padding - 1] == '=')
        padding++;
    if (padding > 2)
        return false;

    size_t data_size = input.size() - padding;
    if (input.size() % 4 == 1)
        return false;
    for (size_t i = 0; i < data_size; i++) {
        if (input[i] == '=' || base64_value(input[i], url_safe) < 0)
            return false;
    }
    for (size_t i = data_size; i < input.size(); i++) {
        if (input[i] != '=')
            return false;
    }
    if (padding != 0 && input.size() % 4 != 0)
        return false;

    output.reserve((data_size * 6) / 8);
    u32 accumulator = 0;
    int bits = 0;
    for (size_t i = 0; i < data_size; i++) {
        accumulator = (accumulator << 6) | static_cast<u32>(base64_value(input[i], url_safe));
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<char>((accumulator >> bits) & 0xff));
        }
    }

    // Any unused low bits must be zero; otherwise the spelling is not a
    // canonical encoding and accepting it can hide corrupted data.
    if (bits != 0 && (accumulator & ((u32 { 1 } << bits) - 1)) != 0)
        return false;
    size_t expected_padding = (4 - (data_size % 4)) % 4;
    return padding == 0 || padding == expected_padding;
}

constexpr u32 rotate_right(u32 value, u32 count)
{
    return (value >> count) | (value << (32 - count));
}

class Sha256 {
public:
    void update(std::string_view input)
    {
        m_bit_count += static_cast<u64>(input.size()) * 8;
        for (unsigned char byte : input) {
            m_buffer[m_buffer_size++] = byte;
            if (m_buffer_size == m_buffer.size()) {
                transform(m_buffer.data());
                m_buffer_size = 0;
            }
        }
    }

    std::array<u8, 32> finish()
    {
        u64 original_bit_count = m_bit_count;
        m_buffer[m_buffer_size++] = 0x80;
        if (m_buffer_size > 56) {
            std::fill(m_buffer.begin() + static_cast<ptrdiff_t>(m_buffer_size), m_buffer.end(), 0);
            transform(m_buffer.data());
            m_buffer_size = 0;
        }
        std::fill(m_buffer.begin() + static_cast<ptrdiff_t>(m_buffer_size), m_buffer.begin() + 56, 0);
        for (int i = 0; i < 8; i++)
            m_buffer[63 - i] = static_cast<u8>(original_bit_count >> (i * 8));
        transform(m_buffer.data());

        std::array<u8, 32> digest { };
        for (size_t i = 0; i < m_state.size(); i++) {
            digest[i * 4] = static_cast<u8>(m_state[i] >> 24);
            digest[i * 4 + 1] = static_cast<u8>(m_state[i] >> 16);
            digest[i * 4 + 2] = static_cast<u8>(m_state[i] >> 8);
            digest[i * 4 + 3] = static_cast<u8>(m_state[i]);
        }
        return digest;
    }

private:
    void transform(u8 const* block)
    {
        static constexpr std::array<u32, 64> constants {
            0x428a2f98,
            0x71374491,
            0xb5c0fbcf,
            0xe9b5dba5,
            0x3956c25b,
            0x59f111f1,
            0x923f82a4,
            0xab1c5ed5,
            0xd807aa98,
            0x12835b01,
            0x243185be,
            0x550c7dc3,
            0x72be5d74,
            0x80deb1fe,
            0x9bdc06a7,
            0xc19bf174,
            0xe49b69c1,
            0xefbe4786,
            0x0fc19dc6,
            0x240ca1cc,
            0x2de92c6f,
            0x4a7484aa,
            0x5cb0a9dc,
            0x76f988da,
            0x983e5152,
            0xa831c66d,
            0xb00327c8,
            0xbf597fc7,
            0xc6e00bf3,
            0xd5a79147,
            0x06ca6351,
            0x14292967,
            0x27b70a85,
            0x2e1b2138,
            0x4d2c6dfc,
            0x53380d13,
            0x650a7354,
            0x766a0abb,
            0x81c2c92e,
            0x92722c85,
            0xa2bfe8a1,
            0xa81a664b,
            0xc24b8b70,
            0xc76c51a3,
            0xd192e819,
            0xd6990624,
            0xf40e3585,
            0x106aa070,
            0x19a4c116,
            0x1e376c08,
            0x2748774c,
            0x34b0bcb5,
            0x391c0cb3,
            0x4ed8aa4a,
            0x5b9cca4f,
            0x682e6ff3,
            0x748f82ee,
            0x78a5636f,
            0x84c87814,
            0x8cc70208,
            0x90befffa,
            0xa4506ceb,
            0xbef9a3f7,
            0xc67178f2,
        };

        std::array<u32, 64> words { };
        for (size_t i = 0; i < 16; i++) {
            size_t offset = i * 4;
            words[i] = (static_cast<u32>(block[offset]) << 24)
                | (static_cast<u32>(block[offset + 1]) << 16)
                | (static_cast<u32>(block[offset + 2]) << 8)
                | static_cast<u32>(block[offset + 3]);
        }
        for (size_t i = 16; i < words.size(); i++) {
            u32 s0 = rotate_right(words[i - 15], 7) ^ rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3);
            u32 s1 = rotate_right(words[i - 2], 17) ^ rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        u32 a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        u32 e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];
        for (size_t i = 0; i < words.size(); i++) {
            u32 sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
            u32 choice = (e & f) ^ (~e & g);
            u32 temp1 = h + sum1 + choice + constants[i] + words[i];
            u32 sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
            u32 majority = (a & b) ^ (a & c) ^ (b & c);
            u32 temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
        m_state[5] += f;
        m_state[6] += g;
        m_state[7] += h;
    }

    std::array<u32, 8> m_state {
        0x6a09e667,
        0xbb67ae85,
        0x3c6ef372,
        0xa54ff53a,
        0x510e527f,
        0x9b05688c,
        0x1f83d9ab,
        0x5be0cd19,
    };
    std::array<u8, 64> m_buffer { };
    size_t m_buffer_size { 0 };
    u64 m_bit_count { 0 };
};

std::array<u8, 32> sha256(std::string_view input)
{
    Sha256 hash;
    hash.update(input);
    return hash.finish();
}

std::string hex_string(u8 const* bytes, size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '\0');
    for (size_t i = 0; i < size; i++) {
        result[i * 2] = digits[bytes[i] >> 4];
        result[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    return result;
}

std::array<u8, 32> hmac_sha256(std::string_view key, std::string_view data)
{
    std::array<u8, 64> key_block { };
    if (key.size() > key_block.size()) {
        auto hashed_key = sha256(key);
        std::copy(hashed_key.begin(), hashed_key.end(), key_block.begin());
    } else if (!key.empty()) {
        std::memcpy(key_block.data(), key.data(), key.size());
    }

    std::array<char, 64> inner_pad { };
    std::array<char, 64> outer_pad { };
    for (size_t i = 0; i < key_block.size(); i++) {
        inner_pad[i] = static_cast<char>(key_block[i] ^ 0x36);
        outer_pad[i] = static_cast<char>(key_block[i] ^ 0x5c);
    }

    Sha256 inner;
    inner.update({ inner_pad.data(), inner_pad.size() });
    inner.update(data);
    auto inner_digest = inner.finish();

    Sha256 outer;
    outer.update({ outer_pad.data(), outer_pad.size() });
    outer.update({ reinterpret_cast<char const*>(inner_digest.data()), inner_digest.size() });
    return outer.finish();
}

bool gzip_compress(std::string_view input, int level, std::string& output)
{
    if (input.size() > std::numeric_limits<uInt>::max())
        return false;

    z_stream stream { };
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    if (deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return false;

    output.clear();
    std::array<char, 16384> buffer { };
    int status = Z_OK;
    do {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = deflate(&stream, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END) {
            deflateEnd(&stream);
            return false;
        }
        output.append(buffer.data(), buffer.size() - stream.avail_out);
    } while (status != Z_STREAM_END);

    return deflateEnd(&stream) == Z_OK;
}

bool gzip_decompress(std::string_view input, size_t maximum, std::string& output)
{
    if (input.size() > std::numeric_limits<uInt>::max())
        return false;

    z_stream stream { };
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    if (inflateInit2(&stream, 15 + 16) != Z_OK)
        return false;

    output.clear();
    std::array<char, 16384> buffer { };
    for (;;) {
        stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
        stream.avail_out = static_cast<uInt>(buffer.size());
        int status = inflate(&stream, Z_NO_FLUSH);
        size_t produced = buffer.size() - stream.avail_out;
        if (produced > maximum - std::min(maximum, output.size())) {
            inflateEnd(&stream);
            return false;
        }
        output.append(buffer.data(), produced);

        if (status == Z_STREAM_END)
            return inflateEnd(&stream) == Z_OK;
        if (status != Z_OK || (stream.avail_in == 0 && produced == 0)) {
            inflateEnd(&stream);
            return false;
        }
    }
}

} // namespace

Value VM::base64_encode(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_bool())
        return Value::nil();
    std::string encoded = encode_base64(string_bytes(argv[0]), argv[1].as_bool());
    return m_gc.make_string(byte_string(encoded));
}

Value VM::base64_decode(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_bool())
        return Value::nil();
    std::string decoded;
    if (!decode_base64(string_bytes(argv[0]), argv[1].as_bool(), decoded))
        return Value::nil();
    return m_gc.make_string(byte_string(decoded));
}

Value VM::hex_encode(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return Value::nil();
    auto bytes = string_bytes(argv[0]);
    std::string encoded = hex_string(reinterpret_cast<u8 const*>(bytes.data()), bytes.size());
    return m_gc.make_string(byte_string(encoded));
}

Value VM::hex_decode(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return Value::nil();
    auto input = string_bytes(argv[0]);
    if (input.size() % 2 != 0)
        return Value::nil();

    auto nibble = [](char character) -> int {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return character - 'a' + 10;
        if (character >= 'A' && character <= 'F')
            return character - 'A' + 10;
        return -1;
    };
    std::string decoded(input.size() / 2, '\0');
    for (size_t i = 0; i < decoded.size(); i++) {
        int high = nibble(input[i * 2]);
        int low = nibble(input[i * 2 + 1]);
        if (high < 0 || low < 0)
            return Value::nil();
        decoded[i] = static_cast<char>((high << 4) | low);
    }
    return m_gc.make_string(byte_string(decoded));
}

Value VM::hash_new(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string() || string_bytes(argv[0]) != "sha256")
        return Value::nil();

    Value handle = dict(0, nullptr);
    dict_put(&handle, m_gc.make_string("__algorithm"), argv[0]);
    dict_put(&handle, m_gc.make_string("__data"), m_gc.make_string(""));
    return handle;
}

Value VM::hash_update(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_string())
        return Value::nil();
    Value key = m_gc.make_string("__data");
    Value existing = dict_get(&argv[0], key);
    if (!existing.is_string())
        return Value::nil();
    StringRef combined = existing.as_string()->str + argv[1].as_string()->str;
    dict_put(&argv[0], key, m_gc.make_string(combined));
    return Value::from_bool(true);
}

Value VM::hash_digest(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_dict() || !argv[1].is_bool())
        return Value::nil();
    Value algorithm = dict_get(&argv[0], m_gc.make_string("__algorithm"));
    Value data = dict_get(&argv[0], m_gc.make_string("__data"));
    if (!algorithm.is_string() || string_bytes(algorithm) != "sha256" || !data.is_string())
        return Value::nil();

    auto digest = sha256(string_bytes(data));
    if (argv[1].as_bool()) {
        std::string encoded = hex_string(digest.data(), digest.size());
        return m_gc.make_string(byte_string(encoded));
    }
    return m_gc.make_string(byte_string({ reinterpret_cast<char const*>(digest.data()), digest.size() }));
}

Value VM::hmac(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_string() || string_bytes(argv[0]) != "sha256")
        return Value::nil();
    auto digest = hmac_sha256(string_bytes(argv[1]), string_bytes(argv[2]));
    std::string encoded = hex_string(digest.data(), digest.size());
    return m_gc.make_string(byte_string(encoded));
}

Value VM::compress(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_int() || string_bytes(argv[0]) != "gzip")
        return Value::nil();
    i64 level = argv[2].as_int();
    if (level < 0 || level > 9)
        return Value::nil();
    std::string output;
    if (!gzip_compress(string_bytes(argv[1]), static_cast<int>(level), output))
        return Value::nil();
    return m_gc.make_string(byte_string(output));
}

Value VM::decompress(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_int() || string_bytes(argv[0]) != "gzip" || argv[2].as_int() <= 0)
        return Value::nil();
    std::string output;
    if (!gzip_decompress(string_bytes(argv[1]), static_cast<size_t>(argv[2].as_int()), output))
        return Value::nil();
    return m_gc.make_string(byte_string(output));
}

Value VM::floor(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::FLOOR_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].is_number()) {
        raise_error(ErrorCode::FLOOR_TYPE_ERROR);
        return Value::nil();
    }

    if (argv[0].is_int())
        return argv[0];

    return Value::from_int(static_cast<i64>(std::floor(argv[0].as_double())));
}

Value VM::ceil(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::CEIL_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].is_number()) {
        raise_error(ErrorCode::CEIL_TYPE_ERROR);
        return Value::nil();
    }

    if (argv[0].is_int())
        return argv[0];

    return Value::from_int(static_cast<i64>(std::ceil(argv[0].as_double_any())));
}

Value VM::round(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::ROUND_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }
    if (!argv[0].is_number()) {
        raise_error(ErrorCode::ROUND_TYPE_ERROR);
        return Value::nil();
    }
    if (argv[0].is_int())
        return argv[0];

    return Value::from_int(static_cast<i64>(std::round(argv[0].as_double_any())));
}

Value VM::abs(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::ABS_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    if (!argv[0].is_number()) {
        raise_error(ErrorCode::ABS_TYPE_ERROR);
        return Value::nil();
    }

    if (argv[0].is_int()) {
        i64 v = argv[0].as_int();
        if (v == INT64_MIN) {
            raise_error(ErrorCode::ABS_OUT_OF_RANGE);
            return Value::nil();
        }
        return Value::from_int(std::abs(argv[0].as_int()));
    }
    return Value::from_real(std::fabs(argv[0].as_double()));
}

Value VM::min(int argc, Value* argv)
{
    if (argc < 1 || !argv) {
        raise_error(ErrorCode::MIN_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    // Determine mode from argv[0]
    bool all_numbers = argv[0].is_number();
    bool all_ints = argv[0].is_int();
    bool all_strs = argv[0].is_string();

    // Validate all args match the expected type
    for (int i = 1; i < argc; i++) {
        all_numbers = all_numbers && argv[i].is_number();
        all_ints = all_ints && argv[i].is_int();
        all_strs = all_strs && argv[i].is_string();
    }

    if (all_strs) {
        Value ret = argv[0];
        for (int i = 1; i < argc; i++) {
            if (argv[i].as_string()->str < ret.as_string()->str)
                ret = argv[i];
        }

        return ret;
    }

    if (!all_numbers)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "min expects either all numbers or all strings");

    Value ret = Value::from_real(argv[0].as_double_any());
    for (int i = 1; i < argc; i++)
        ret = Value::from_real(std::fmin(ret.as_double_any(), argv[i].as_double_any()));

    if (all_ints)
        return Value::from_int(static_cast<i64>(ret.as_double_any()));

    return ret;
}

Value VM::max(int argc, Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        raise_error(ErrorCode::MAX_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    // Determine mode from argv[0]
    bool all_numbers = argv[0].is_number();
    bool all_ints = argv[0].is_int();
    bool all_strs = argv[0].is_string();

    // Validate all args match the expected type
    for (int i = 1; i < argc; i++) {
        all_numbers = all_numbers && argv[i].is_number();
        all_ints = all_ints && argv[i].is_int();
        all_strs = all_strs && argv[i].is_string();
    }

    if (all_strs) {
        Value ret = argv[0];
        for (int i = 1; i < argc; i++) {
            if (argv[i].as_string()->str > ret.as_string()->str)
                ret = argv[i];
        }
        return ret;
    }

    if (!all_numbers)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "max expects either all numbers or all strings");

    Value ret = Value::from_real(argv[0].as_double_any());
    for (int i = 1; i < argc; i++)
        ret = Value::from_real(std::fmax(ret.as_double_any(), argv[i].as_double_any()));

    if (all_ints)
        return Value::from_int(static_cast<i64>(ret.as_double_any()));

    return ret;
}

Value VM::pow(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr) {
        raise_error(ErrorCode::POW_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value base = argv[0];
    Value exponent = argv[1];

    if (UNLIKELY(!base.is_number() || !exponent.is_number())) {
        raise_error(ErrorCode::POW_TYPE_ERROR);
        return Value::nil();
    }

    if (base.is_int() && exponent.is_int())
        // return an int even if the result may be larger than 48 bit range
        return Value::from_int(std::pow(base.as_int(), exponent.as_int()));
    else
        return Value::from_real(std::pow(base.as_double_any(), exponent.as_double_any()));

    return Value::nil();
}

Value VM::sqrt(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::SQRT_ARG_COUNT, "got " + std::to_string(argc));
        return Value::nil();
    }

    Value n = argv[0];

    if (UNLIKELY(!n.is_number())) {
        raise_error(ErrorCode::SQRT_TYPE_ERROR);
        return Value::nil();
    }

    f64 val = n.as_double_any();
    if (val < 0.0)
        return Value::nil();

    return Value::from_real(std::sqrt(val));
}

Value VM::math_unary(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_number())
        return Value::nil();

    std::string_view operation = string_bytes(argv[0]);
    f64 value = argv[1].as_double_any();
    if (operation == "sin")
        return Value::from_real(std::sin(value));
    if (operation == "cos")
        return Value::from_real(std::cos(value));
    if (operation == "tan")
        return Value::from_real(std::tan(value));
    if (operation == "log")
        return Value::from_real(std::log(value));
    if (operation == "exp")
        return Value::from_real(std::exp(value));
    return Value::nil();
}

Value VM::math_binary(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string()
        || !argv[1].is_number() || !argv[2].is_number())
        return Value::nil();

    std::string_view operation = string_bytes(argv[0]);
    f64 first = argv[1].as_double_any();
    f64 second = argv[2].as_double_any();
    if (operation == "hypot")
        return Value::from_real(std::hypot(first, second));
    if (operation == "atan2")
        return Value::from_real(std::atan2(first, second));
    return Value::nil();
}

Value VM::url_encode(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return Value::nil();
    constexpr char hex[] = "0123456789ABCDEF";
    std::string output;
    for (unsigned char byte : string_bytes(argv[0])) {
        if ((byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z')
            || (byte >= '0' && byte <= '9') || byte == '-' || byte == '.'
            || byte == '_' || byte == '~') {
            output.push_back(static_cast<char>(byte));
        } else {
            output.push_back('%');
            output.push_back(hex[byte >> 4]);
            output.push_back(hex[byte & 0x0f]);
        }
    }
    return m_gc.make_string(byte_string(output));
}

Value VM::url_decode(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return Value::nil();
    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9')
            return ch - '0';
        if (ch >= 'a' && ch <= 'f')
            return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F')
            return ch - 'A' + 10;
        return -1;
    };
    std::string_view input = string_bytes(argv[0]);
    std::string output;
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '+') {
            output.push_back(' ');
        } else if (input[i] == '%' && i + 2 < input.size()) {
            int high = hex_value(input[i + 1]);
            int low = hex_value(input[i + 2]);
            if (high < 0 || low < 0)
                return Value::nil();
            output.push_back(static_cast<char>((high << 4) | low));
            i += 2;
        } else if (input[i] == '%') {
            return Value::nil();
        } else {
            output.push_back(input[i]);
        }
    }
    return m_gc.make_string(byte_string(output));
}

Value VM::url_parse(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return Value::nil();
    static std::regex const pattern(
        R"(^([A-Za-z][A-Za-z0-9+.-]*)://(\[[^\]]+\]|[^/?#:]*)(?::([0-9]+))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?$)");
    std::string input(string_bytes(argv[0]));
    std::smatch match;
    if (!std::regex_match(input, match, pattern) || match[2].str().empty())
        return Value::nil();

    Value result = m_gc.make_dict();
    dict_put(&result, m_gc.make_string("scheme"), m_gc.make_string(match[1].str().c_str()));
    dict_put(&result, m_gc.make_string("host"), m_gc.make_string(match[2].str().c_str()));
    Value port = Value::nil();
    if (match[3].matched) {
        std::string port_text = match[3].str();
        i64 parsed_port = 0;
        auto conversion = std::from_chars(port_text.data(), port_text.data() + port_text.size(), parsed_port);
        if (conversion.ec != std::errc() || conversion.ptr != port_text.data() + port_text.size()
            || parsed_port < 0 || parsed_port > 65535)
            return Value::nil();
        port = Value::from_int(parsed_port);
    }
    dict_put(&result, m_gc.make_string("port"), port);
    dict_put(&result, m_gc.make_string("path"), m_gc.make_string(match[4].str().c_str()));
    dict_put(&result, m_gc.make_string("query"), m_gc.make_string(match[5].str().c_str()));
    dict_put(&result, m_gc.make_string("fragment"), m_gc.make_string(match[6].str().c_str()));
    return result;
}

Value VM::url_build(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_dict())
        return Value::nil();
    auto field = [&](char const* name) { return dict_get(&argv[0], m_gc.make_string(name)); };
    Value scheme = field("scheme");
    Value host = field("host");
    Value port = field("port");
    Value path = field("path");
    Value query = field("query");
    Value fragment = field("fragment");
    if (!scheme.is_string() || !host.is_string())
        return Value::nil();
    std::string output(string_bytes(scheme));
    output += "://";
    output += string_bytes(host);
    if (port.is_int()) {
        output.push_back(':');
        output += std::to_string(port.as_int());
    }
    if (path.is_string())
        output += string_bytes(path);
    if (query.is_string() && !string_bytes(query).empty()) {
        output.push_back('?');
        output += string_bytes(query);
    }
    if (fragment.is_string() && !string_bytes(fragment).empty()) {
        output.push_back('#');
        output += string_bytes(fragment);
    }
    return m_gc.make_string(byte_string(output));
}

namespace {

size_t utf8_byte_offset(std::string_view text, i64 character_offset)
{
    if (character_offset < 0)
        return std::string_view::npos;
    i64 characters = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80) {
            if (characters == character_offset)
                return i;
            ++characters;
        }
    }
    return characters == character_offset ? text.size() : std::string_view::npos;
}

i64 utf8_character_offset(std::string_view text, size_t byte_offset)
{
    i64 characters = 0;
    for (size_t i = 0; i < std::min(byte_offset, text.size()); ++i) {
        if ((static_cast<unsigned char>(text[i]) & 0xc0) != 0x80)
            ++characters;
    }
    return characters;
}

}

Value VM::make_regex_result(std::string const& input, std::smatch const& match, size_t base_offset)
{
    Value result = m_gc.make_dict();
    size_t match_start = base_offset + static_cast<size_t>(match.position(0));
    size_t match_end = match_start + static_cast<size_t>(match.length(0));
    dict_put(&result, m_gc.make_string("start"),
        Value::from_int(utf8_character_offset(input, match_start)));
    dict_put(&result, m_gc.make_string("end"),
        Value::from_int(utf8_character_offset(input, match_end)));

    Value groups = m_gc.make_list();
    for (size_t i = 0; i < match.size(); ++i) {
        groups.as_list()->elements.push(match[i].matched
                ? m_gc.make_string(byte_string(match[i].str()))
                : Value::nil());
    }
    dict_put(&result, m_gc.make_string("groups"), groups);
    dict_put(&result, m_gc.make_string("named"), m_gc.make_dict());
    return result;
}

Value VM::regex_compile(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_int())
        return Value::nil();
    try {
        std::regex validation(std::string(string_bytes(argv[0])));
        (void)validation;
    } catch (std::regex_error const&) {
        return Value::nil();
    }
    return argv[0];
}

Value VM::regex_search(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string()
        || !argv[1].is_string() || !argv[2].is_int())
        return Value::nil();
    std::string input(string_bytes(argv[1]));
    size_t start = utf8_byte_offset(input, argv[2].as_int());
    if (start == std::string::npos)
        return Value::nil();
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::smatch match;
        std::string suffix = input.substr(start);
        if (!std::regex_search(suffix, match, pattern))
            return Value::nil();
        return make_regex_result(input, match, start);
    } catch (std::regex_error const&) {
        return Value::nil();
    }
}

Value VM::regex_match(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string()
        || !argv[1].is_string() || !argv[2].is_int())
        return Value::nil();
    std::string input(string_bytes(argv[1]));
    size_t start = utf8_byte_offset(input, argv[2].as_int());
    if (start == std::string::npos)
        return Value::nil();
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::smatch match;
        std::string suffix = input.substr(start);
        if (!std::regex_search(suffix, match, pattern, std::regex_constants::match_continuous))
            return Value::nil();
        return make_regex_result(input, match, start);
    } catch (std::regex_error const&) {
        return Value::nil();
    }
}

Value VM::regex_fullmatch(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string())
        return Value::nil();
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::string input(string_bytes(argv[1]));
        std::smatch match;
        if (!std::regex_match(input, match, pattern))
            return Value::nil();
        return make_regex_result(input, match, 0);
    } catch (std::regex_error const&) {
        return Value::nil();
    }
}

Value VM::regex_findall(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string())
        return Value::nil();
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::string input(string_bytes(argv[1]));
        Value results = m_gc.make_list();
        for (std::sregex_iterator it(input.begin(), input.end(), pattern), end; it != end; ++it)
            results.as_list()->elements.push(make_regex_result(input, *it, 0));
        return results;
    } catch (std::regex_error const&) {
        return Value::nil();
    }
}

Value VM::regex_split(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string()
        || !argv[1].is_string() || !argv[2].is_int() || argv[2].as_int() < 0)
        return Value::nil();
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::string input(string_bytes(argv[1]));
        i64 limit = argv[2].as_int();
        size_t previous = 0;
        i64 splits = 0;
        Value results = m_gc.make_list();
        for (std::sregex_iterator it(input.begin(), input.end(), pattern), end;
            it != end && (limit == 0 || splits < limit); ++it, ++splits) {
            size_t position = static_cast<size_t>(it->position());
            results.as_list()->elements.push(m_gc.make_string(byte_string(
                std::string_view(input).substr(previous, position - previous))));
            previous = position + static_cast<size_t>(it->length());
        }
        results.as_list()->elements.push(m_gc.make_string(byte_string(std::string_view(input).substr(previous))));
        return results;
    } catch (std::regex_error const&) {
        return Value::nil();
    }
}

Value VM::regex_replace(int argc, Value* argv)
{
    if (argc != 4 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_string() || !argv[3].is_int() || argv[3].as_int() < 0)
        return Value::nil();
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::string input(string_bytes(argv[1]));
        std::string replacement(string_bytes(argv[2]));
        i64 limit = argv[3].as_int();
        size_t previous = 0;
        i64 replacements = 0;
        std::string output;
        for (std::sregex_iterator it(input.begin(), input.end(), pattern), end;
            it != end && (limit == 0 || replacements < limit); ++it, ++replacements) {
            size_t position = static_cast<size_t>(it->position());
            output.append(input, previous, position - previous);
            output += it->format(replacement);
            previous = position + static_cast<size_t>(it->length());
        }
        output.append(input, previous, std::string::npos);
        return m_gc.make_string(byte_string(output));
    } catch (std::regex_error const&) {
        return Value::nil();
    }
}

Value VM::Assert(int argc, Value* argv)
{
    if (argc < 1 || argc > 2 || argv == nullptr) {
        raise_error(ErrorCode::ASSERT_ARG_COUNT, "got" + std::to_string(argc));
        return Value::nil();
    }

    // Python-style assert accepts an optional diagnostic value; that second
    // argument describes a failed assertion and is not itself a condition.
    if (!argv[0].is_truthy()) {
        std::string detail;
        if (argc == 2) {
            StringRef rendered = value_to_string(argv[1]);
            detail.assign(rendered.data(), rendered.len());
        }
        raise_error(ErrorCode::ASSERT_FAILED, detail);
    }

    return Value::nil(); // success
}

Value VM::open(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr) {
        raise_error(ErrorCode::OPEN_ARG_COUNT);
        return Value::nil();
    }

    if (UNLIKELY(!argv[0].is_string() || !argv[1].is_string()))
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "open expects (string, string)");

    StringRef const& filename_arg = argv[0].as_string()->str;
    if (::memchr(filename_arg.data(), '\0', filename_arg.len()) != nullptr)
        raise_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file path contains a NUL byte");

    char const* filename = filename_arg.data();
    StringRef mode_arg = argv[1].as_string()->str;

    StringRef fmode;
    if (mode_arg == "اضف")
        fmode = "a";
    else if (mode_arg == "اقرا")
        fmode = "r";
    else if (mode_arg == "اكتب")
        fmode = "w";
    else
        raise_error(ErrorCode::NATIVE_TYPE_ERROR);

    FILE* fp = fopen(filename, fmode.data());
    if (fp == NULL) {
        raise_error(ErrorCode::NATIVE_TYPE_ERROR);
        return Value::nil();
    }

    return m_gc.make_file_handle(fp);
}

Value VM::append_file(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr) {
        raise_error(ErrorCode::APPEND_FILE_ARG_COUNT);
        return Value::nil();
    }

    Value& file = argv[0];
    Value& content = argv[1];

    if (!file.is_file_handle()) {
        raise_error(ErrorCode::APPEND_FILE_TYPE_ERROR);
        return Value::nil();
    }

    if (!content.is_string()) {
        raise_error(ErrorCode::APPEND_FILE_TYPE_ERROR);
        return Value::nil();
    }

    ObjFileHandle* file_handle = file.as_file_handle();
    if (!file_handle->is_open || file_handle->fp == nullptr) {
        raise_error(ErrorCode::APPEND_FILE_TYPE_ERROR,
            "file handle is closed");
        return Value::from_bool(false);
    }
    ObjString* str_obj = content.as_string();
    FILE* fp = file_handle->fp;
    StringRef content_str = str_obj->str;

    if (content_str.empty())
        return Value::from_bool(true); // nothing to write is trivially successful

    size_t const written = std::fwrite(content_str.data(), 1, content_str.len(), fp);
    // ::fflush(fp);

    if (written != content_str.len()) {
        raise_error(ErrorCode::APPEND_FILE_FAILED, std::strerror(errno));
        return Value::from_bool(false);
    }

    return Value::from_bool(true);
}

Value VM::close(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        raise_error(ErrorCode::CLOSE_ARG_COUNT);
        return Value::from_bool(false);
    }

    if (!argv[0].is_file_handle()) {
        raise_error(ErrorCode::CLOSE_TYPE_ERROR);
        return Value::from_bool(false);
    }

    ObjFileHandle* file_handle = argv[0].as_file_handle();
    if (file_handle->close())
        return Value::from_bool(true);

    return Value::from_bool(false);
}

Value VM::clock(int argc, Value* argv)
{
    if (argc != 0)
        return Value::nil();
    (void)argv;
    auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return Value::from_real(std::chrono::duration<f64>(elapsed).count());
}
Value VM::error(int /*argc*/, Value* /*argv*/) { return Value::nil(); }
Value VM::time(int /*argc*/, Value* /*argv*/) { return Value::nil(); }

// stdlib helpers
void VM::dict_put(Value* dict_ptr, Value k, Value v)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return;

    ObjDict* as_dict = dict_ptr->as_dict();
    as_dict->set(k, v);
}

Value VM::dict_get(Value* dict_ptr, Value k)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return Value::nil();

    ObjDict* as_dict = dict_ptr->as_dict();
    return as_dict->data[k];
}

namespace {

consteval auto make_builtin_registry()
{
    auto entries = std::array {
        BuiltinDefinition { "طول", &VM::len, 1 },
        BuiltinDefinition { "اضف", &VM::append, -1 },
        BuiltinDefinition { "احذف", &VM::pop, 1 },
        BuiltinDefinition { "مقطع", &VM::slice, -1 },
        BuiltinDefinition { "قائمة", &VM::list, -1 },
        BuiltinDefinition { "قاموس", &VM::dict, -1 },
        BuiltinDefinition { "__قاموس_مفاتيح__", &VM::dict_keys, 1 },
        BuiltinDefinition { "__قاموس_يحتوي__", &VM::dict_contains, 2 },
        BuiltinDefinition { "__قاموس_احذف__", &VM::dict_delete, 2 },
        BuiltinDefinition { "اكتب", &VM::print, -1 },
        BuiltinDefinition { "ادخل", &VM::input, 0 },
        BuiltinDefinition { "افتح", &VM::open, 2 },
        BuiltinDefinition { "اضف_ملف", &VM::append_file, 2 },
        BuiltinDefinition { "اغلق", &VM::close, 1 },
        BuiltinDefinition { "صنف", &VM::type, 1 },
        BuiltinDefinition { "طبيعي", &VM::Int, 1 },
        BuiltinDefinition { "حقيقي", &VM::Float, 1 },
        BuiltinDefinition { "سلسلة", &VM::str, -1 },
        BuiltinDefinition { "منطقي", &VM::Bool, 1 },
        BuiltinDefinition { "اقسم", &VM::split, 2 },
        BuiltinDefinition { "اجمع", &VM::join, 2 },
        BuiltinDefinition { "جزء", &VM::substr, 3 },
        BuiltinDefinition { "يحتوي", &VM::contains, 2 },
        BuiltinDefinition { "قص", &VM::trim, 1 },
        BuiltinDefinition { "__نص_من_رمز__", &VM::char_from_codepoint, 1 },
        BuiltinDefinition { "__عدد_من_نص__", &VM::number_from_text, 1 },
        BuiltinDefinition { "__عدد_منته__", &VM::number_finite, 1 },
        BuiltinDefinition { "__عدد_ليس_رقما__", &VM::number_is_nan, 1 },
        BuiltinDefinition { "__JSON_اهرب__", &VM::json_escape, 1 },
        BuiltinDefinition { "__JSON_اقرا_سلسلة__", &VM::json_read_string, 2 },
        BuiltinDefinition { "__استدعاء__", &VM::dynamic_call, 2 },
        BuiltinDefinition { "__منفذ_جديد__", &VM::executor_new, 1 },
        BuiltinDefinition { "__منفذ_اغلق__", &VM::executor_close, 2 },
        BuiltinDefinition { "__مهمة_ابدأ__", &VM::task_start, 3 },
        BuiltinDefinition { "__مهمة_تمت__", &VM::task_done, 1 },
        BuiltinDefinition { "__مهمة_نتيجة__", &VM::task_result, 2 },
        BuiltinDefinition { "__مهمة_الغ__", &VM::task_cancel, 1 },
        BuiltinDefinition { "__مهمة_انتظر_الكل__", &VM::task_wait_all, 2 },
        BuiltinDefinition { "__ملف_افتح__", &VM::file_open, 2 },
        BuiltinDefinition { "__ملف_اقرا__", &VM::file_read, 2 },
        BuiltinDefinition { "__ملف_اقرا_الكل__", &VM::file_read_all, 1 },
        BuiltinDefinition { "__ملف_اقرا_سطر__", &VM::file_read_line, 1 },
        BuiltinDefinition { "__ملف_اكتب__", &VM::file_write, 2 },
        BuiltinDefinition { "__ملف_اضف__", &VM::file_write, 2 },
        BuiltinDefinition { "__ملف_ادفع__", &VM::file_flush, 1 },
        BuiltinDefinition { "__ملف_اغلق__", &VM::close, 1 },
        BuiltinDefinition { "__مسار_احذف__", &VM::path_delete, 1 },
        BuiltinDefinition { "__مسار_glob__", &VM::path_glob, 2 },
        BuiltinDefinition { "__ملف_مؤقت__", &VM::temp_file, 3 },
        BuiltinDefinition { "__مجلد_مؤقت__", &VM::temp_directory, 2 },
        BuiltinDefinition { "__نظام_احذف_شجرة__", &VM::remove_tree, 1 },
        BuiltinDefinition { "__وقت_الان__", &VM::datetime_now, 0 },
        BuiltinDefinition { "__وقت_من_حقول__", &VM::datetime_from_fields, 7 },
        BuiltinDefinition { "__وقت_الى_حقول__", &VM::datetime_to_fields, 2 },
        BuiltinDefinition { "__وقت_حلل__", &VM::datetime_parse, 3 },
        BuiltinDefinition { "__وقت_نسق__", &VM::datetime_format, 3 },
        BuiltinDefinition { "__64_رمز__", &VM::base64_encode, 2 },
        BuiltinDefinition { "__64_فك__", &VM::base64_decode, 2 },
        BuiltinDefinition { "__16_رمز__", &VM::hex_encode, 1 },
        BuiltinDefinition { "__16_فك__", &VM::hex_decode, 1 },
        BuiltinDefinition { "__هاش_جديد__", &VM::hash_new, 1 },
        BuiltinDefinition { "__هاش_حدث__", &VM::hash_update, 2 },
        BuiltinDefinition { "__هاش_ناتج__", &VM::hash_digest, 2 },
        BuiltinDefinition { "__HMAC__", &VM::hmac, 3 },
        BuiltinDefinition { "__ضغط__", &VM::compress, 3 },
        BuiltinDefinition { "__فك_ضغط__", &VM::decompress, 3 },
        BuiltinDefinition { "ادنى", &VM::floor, 1 },
        BuiltinDefinition { "اعلى", &VM::ceil, 1 },
        BuiltinDefinition { "تقريب", &VM::round, 1 },
        BuiltinDefinition { "مطلق", &VM::abs, 1 },
        BuiltinDefinition { "اصغر", &VM::min, -1 },
        BuiltinDefinition { "اكبر", &VM::max, -1 },
        BuiltinDefinition { "قوة", &VM::pow, 2 },
        BuiltinDefinition { "جذر", &VM::sqrt, 1 },
        BuiltinDefinition { "__رياضيات__", &VM::math_unary, 2 },
        BuiltinDefinition { "__رياضيات2__", &VM::math_binary, 3 },
        BuiltinDefinition { "__URL_اهرب__", &VM::url_encode, 1 },
        BuiltinDefinition { "__URL_فك__", &VM::url_decode, 1 },
        BuiltinDefinition { "__URL_حلل__", &VM::url_parse, 1 },
        BuiltinDefinition { "__URL_ركب__", &VM::url_build, 1 },
        BuiltinDefinition { "__نمط_اجمع__", &VM::regex_compile, 2 },
        BuiltinDefinition { "__نمط_بحث__", &VM::regex_search, 3 },
        BuiltinDefinition { "__نمط_طابق__", &VM::regex_match, 3 },
        BuiltinDefinition { "__نمط_كامل__", &VM::regex_fullmatch, 2 },
        BuiltinDefinition { "__نمط_الكل__", &VM::regex_findall, 2 },
        BuiltinDefinition { "__نمط_اقسم__", &VM::regex_split, 3 },
        BuiltinDefinition { "__نمط_استبدل__", &VM::regex_replace, 4 },
        BuiltinDefinition { "تاكد", &VM::Assert, -1 },
        BuiltinDefinition { "ساعة", &VM::clock, 0 },
        BuiltinDefinition { "عطل", &VM::error, -1 },
        BuiltinDefinition { "وقت", &VM::time, 0 },
    };
    std::sort(entries.begin(), entries.end(), [](auto const& lhs, auto const& rhs) {
        return lhs.name < rhs.name;
    });
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name.empty() || entries[i].fn == nullptr || entries[i].arity < -1)
            throw "invalid builtin definition";
        if (i > 0 && entries[i - 1].name == entries[i].name)
            throw "duplicate builtin name";
    }
    return entries;
}

constexpr auto builtin_registry = make_builtin_registry();

} // namespace

BuiltinsList::BuiltinsList(VM& vm)
    : m_vm(vm)
    , m_values(builtin_registry.size(), Value::nil())
{
}

std::span<BuiltinDefinition const> BuiltinsList::definitions()
{
    return builtin_registry;
}

Value const* BuiltinsList::find(StringRef const& name) const
{
    std::string_view key(name.data(), name.len());
    auto found = std::lower_bound(builtin_registry.begin(), builtin_registry.end(), key,
        [](BuiltinDefinition const& entry, std::string_view value) { return entry.name < value; });
    if (found == builtin_registry.end() || found->name != key)
        return nullptr;

    Value& value = m_values[static_cast<size_t>(found - builtin_registry.begin())];
    if (value.is_nil()) {
        ObjString* native_name = m_vm.m_gc.make_obj_string(StringRef(found->name.data()));
        value = m_vm.m_gc.make_native(found->fn, native_name, found->arity);
    }
    return &value;
}

} // namespace fairuz::runtime
