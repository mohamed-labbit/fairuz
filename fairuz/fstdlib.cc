//
// stdlib.cc
//

#include "fobj_header.hpp"
#include "fobject.hpp"
#include "futil.hpp"
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
    if (Fa_is_nil(v)) {
        out += "nil";
        return;
    }
    if (Fa_is_bool(v)) {
        out += Fa_as_bool(v) ? "صحيح" : "خطا";
        return;
    }
    if (Fa_is_int(v)) {
        out += Fa_StringRef(std::to_string(Fa_as_int(v)).c_str());
        return;
    }
    if (Fa_is_double(v)) {
        f64 d = Fa_as_double(v);
        if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15)
            out += Fa_StringRef(std::to_string(static_cast<i64>(d)).c_str());
        else
            out += format_double_string(d);
        return;
    }
    if (Fa_is_string(v)) {
        if (quote_strings)
            out += '"';
        out += Fa_as_string(v)->str;
        if (quote_strings)
            out += '"';
        return;
    }
    if (Fa_is_list(v)) {
        Fa_ObjList* list = Fa_as_list(v);
        out += '[';
        for (u32 i = 0, n = list->elements.size(); i < n; i += 1) {
            if (i > 0)
                out += ", ";
            append_rendered_value(out, list->elements[i], true);
        }
        out += ']';
        return;
    }
    if (Fa_is_dict(v)) {
        Fa_ObjDict* dict = Fa_as_dict(v);
        out += '{';
        u32 i = 0, end = dict->data.size() - 1;
        for (auto [k, v] : dict->data) {
            append_rendered_value(out, k, Fa_is_string(k));
            out += ": ";
            append_rendered_value(out, v, Fa_is_string(v));
            if (i == end)
                break;
            out += ", ";
            i++;
        }

        out += '}';
        return;
    }
    if (Fa_is_native(v)) {
        out += "<native>";
        return;
    }
    if (Fa_is_function(v)) {
        out += "<function>";
        return;
    }
    if (Fa_is_class(v)) {
        out += "<class ";
        Fa_ObjClass* klass = Fa_as_class(v);
        out += klass->name;
        out += '>';
        return;
    }
    if (Fa_is_instance(v)) {
        out += '<';
        Fa_ObjInstance* instance = Fa_as_instance(v);
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
        return Fa_make_nil();

    if (argc == 1) {
        if (Fa_is_string(argv[0])) {
            Fa_StringRef const& str = Fa_as_string(argv[0])->str;
            size_t byte_pos = 0;
            i64 char_count = 0;

            while (byte_pos < str.len()) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_count += 1;
            }

            return Fa_make_int(char_count);
        }

        if (Fa_is_list(argv[0]))
            return Fa_make_int(Fa_as_list(argv[0])->elements.size());
        if (Fa_is_dict(argv[0]))
            return Fa_make_int(Fa_as_dict(argv[0])->data.size());
    }

    /// do not accept multiple args for len
    return Fa_make_nil();
}

static void print_runtime_value(Fa_Value v, int depth = 0)
{
    if (Fa_is_nil(v)) {
        std::cout << "nil";
        return;
    }

    if (Fa_is_bool(v)) {
        std::cout << (Fa_as_bool(v) ? "صحيح" : "خطا");
        return;
    }

    if (Fa_is_int(v)) {
        std::cout << Fa_as_int(v);
        return;
    }

    if (Fa_is_obj(v)) {
        Fa_ObjHeader* obj = Fa_as_obj(v);

        switch (obj->type) {
        case Fa_ObjType::STRING:
            std::cout << Fa_obj_cast<Fa_ObjString>(obj, Fa_ObjType::STRING)->str;
            return;

        case Fa_ObjType::LIST: {
            auto list = Fa_obj_cast<Fa_ObjList>(obj, Fa_ObjType::LIST);
            std::cout << '[';
            for (u32 i = 0, n = list->size(); i < n; i += 1) {
                if (i > 0)
                    std::cout << ", ";
                Fa_Value elem = list->elements[i];
                if (Fa_is_obj(elem) && Fa_as_obj(elem)->type == Fa_ObjType::STRING) {
                    std::cout << '"';
                    std::cout << Fa_as_string((elem))->str;
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
                if (Fa_is_string(k)) {
                    std::cout << '"';
                    std::cout << Fa_as_string(k)->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(k, depth + 1);
                }
                std::cout << ": ";
                if (Fa_is_string(v)) {
                    std::cout << '"';
                    std::cout << Fa_as_string(v)->str;
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

    f64 d = Fa_as_double(v);
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
        return Fa_make_nil();
    }

    for (int i = 0; i < argc; i += 1) {
        if (i > 0)
            std::cout << '\t';
        print_runtime_value(argv[i]);
    }
    std::cout << '\n';
    return Fa_make_nil();
}

Fa_Value Fa_VM::Fa_type(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_make_nil();

    return Fa_make_int(static_cast<i64>(value_type_tag(argv[0])));
}

Fa_Value Fa_VM::Fa_int(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_make_nil();
    if (Fa_is_number(argv[0]))
        return Fa_make_int(static_cast<i64>(Fa_as_double_any(argv[0])));
    return Fa_make_nil();
}

Fa_Value Fa_VM::Fa_float(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_make_nil();

    if (Fa_is_number(argv[0]))
        return Fa_make_real(Fa_as_double_any(argv[0]));

    return Fa_make_nil();
}

Fa_Value Fa_VM::Fa_append(int argc, Fa_Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::APPEND_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    Fa_Value& list_v = argv[0];
    if (!Fa_is_list(list_v)) {
        stdlib_error(StdlibErrorCode::APPEND_TYPE_ERROR);
        return Fa_make_nil();
    }

    Fa_ObjList* list_obj = Fa_as_list(list_v);

    for (int i = 1; i < argc; i += 1)
        list_obj->elements.push(argv[i]);

    return Fa_make_nil();
}

Fa_Value Fa_VM::Fa_pop(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::POP_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    Fa_Value& list_v = argv[0];
    if (!Fa_is_list(list_v)) {
        stdlib_error(StdlibErrorCode::POP_TYPE_ERROR);
        return Fa_make_nil();
    }

    Fa_as_list(list_v)->elements.pop();
    return Fa_make_nil();
}

Fa_Value Fa_VM::Fa_slice(int argc, Fa_Value* argv)
{
    /// cut a copy of a list, with inclusive indices
    /// accept [list, a, b]
    /// a, b are the indices
    /// if b is null then cut [a:]

    if (argc < 2) {
        stdlib_error(StdlibErrorCode::SLICE_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    Fa_ObjList* list_obj = Fa_as_list(argv[0]);
    Fa_Value ret = m_gc.make_list();
    Fa_ObjList* ret_list = Fa_as_list(ret);
    /// Expects indices to be ints
    u32 a = Fa_as_int(argv[1]);
    u32 b = argc == 3 ? Fa_as_int(argv[2]) : list_obj->size() - 1;

    for (u32 i = a; i <= b; i += 1)
        ret_list->elements.push(list_obj->elements[i]);

    return ret;
}

Fa_Value Fa_VM::Fa_input(int /*argc*/, Fa_Value* /*argv*/) // input takes no args for now
{
    // read until user hits ENTER
    Fa_StringRef ret_str = "";
    std::string help = ""; // getline only accepts std::string

    if (!std::getline(std::cin, help))
        // don't know what error to report
        return Fa_make_nil();

    ret_str = help.data();
    Fa_Value ret = m_gc.make_string(ret_str);
    return ret;
}

Fa_Value Fa_VM::Fa_str(int argc, Fa_Value* argv)
{
    if (argc > 1) {
        stdlib_error(StdlibErrorCode::STR_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    Fa_StringRef output = "";

    if (argc == 0 || argv == nullptr)
        return m_gc.make_string(output); // return empty on no arg

    if (Fa_is_string(argv[0]))
        return m_gc.make_string(Fa_as_string(argv[0])->str);

    Fa_StringRef rendered = value_to_string(argv[0]);
    return m_gc.make_string(rendered);
}

Fa_Value Fa_VM::Fa_bool(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::BOOL_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    return Fa_is_truthy(argv[0]) ? Fa_make_bool(true) : Fa_make_bool(false);
}

Fa_Value Fa_VM::Fa_list(int argc, Fa_Value* argv)
{
    Fa_Value ret = m_gc.make_list();
    Fa_ObjList* list_obj = Fa_as_list(ret);

    for (int i = 0; i < argc; i += 1)
        list_obj->elements.push(argv[i]);

    return ret;
}

Fa_Value Fa_VM::Fa_dict(int argc, Fa_Value* argv)
{
    Fa_Value ret = m_gc.make_dict();
    if (argc <= 0 || argv == nullptr)
        return ret;

    Fa_ObjDict* dict_obj = Fa_as_dict(ret);
    for (int i = 0; i + 1 < argc; i += 2)
        dict_obj->data[argv[i]] = argv[i + 1];

    return ret;
}

Fa_Value Fa_VM::Fa_split(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Fa_make_nil();
    if (!Fa_is_string(argv[0]) || !Fa_is_string(argv[1]))
        return Fa_make_nil();

    Fa_StringRef src = Fa_as_string(argv[0])->str;
    Fa_StringRef delim = Fa_as_string(argv[1])->str;

    Fa_Value ret = m_gc.make_list();
    Fa_ObjList* list = Fa_as_list(ret);

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

            pos += 1;
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
        return Fa_make_nil();
    if (!Fa_is_list(argv[0]) || !Fa_is_string(argv[1]))
        return Fa_make_nil();

    Fa_ObjList* list = Fa_as_list(argv[0]);
    Fa_StringRef delim = Fa_as_string(argv[1])->str;
    Fa_StringRef out = "";

    for (u32 i = 0; i < list->elements.size(); i += 1) {
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
        return Fa_make_nil();
    }

    Fa_StringRef str = Fa_as_string(argv[0])->str;

    if (UNLIKELY(Fa_is_nil(argv[1]) || Fa_is_nil(argv[2])))
        return Fa_make_nil();

    i64 a = Fa_as_int(argv[1]);
    i64 b = Fa_as_int(argv[2]);

    Fa_StringRef ret = str.substr(a, b);
    return m_gc.make_string(ret);
}

Fa_Value Fa_VM::Fa_contains(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return Fa_make_nil();
    if (!Fa_is_string(argv[0]) || !Fa_is_string(argv[1]))
        return Fa_make_nil();

    Fa_StringRef haystack = Fa_as_string(argv[0])->str;
    Fa_StringRef needle = Fa_as_string(argv[1])->str;
    if (needle.empty())
        return Fa_make_bool(true);

    return Fa_make_bool(haystack.find(needle));
}

Fa_Value Fa_VM::Fa_trim(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return Fa_make_nil();
    if (!Fa_is_string(argv[0]))
        return Fa_make_nil();

    Fa_StringRef str = Fa_as_string(argv[0])->str;
    size_t start = 0;
    size_t end = str.len();

    auto is_trim_space = [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };

    while (start < end && is_trim_space(str[start]))
        start += 1;
    while (end > start && is_trim_space(str[end - 1]))
        end -= 1;

    return m_gc.make_string(str.substr_copy(start, end));
}

Fa_Value Fa_VM::Fa_floor(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::FLOOR_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    if (!Fa_is_number(argv[0])) {
        stdlib_error(StdlibErrorCode::FLOOR_TYPE_ERROR);
        return Fa_make_nil();
    }

    if (Fa_is_int(argv[0]))
        return argv[0];

    return Fa_make_real(std::floor(Fa_as_double(argv[0])));
}

Fa_Value Fa_VM::Fa_ceil(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::CEIL_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    if (!Fa_is_number(argv[0])) {
        stdlib_error(StdlibErrorCode::CEIL_TYPE_ERROR);
        return Fa_make_nil();
    }

    if (Fa_is_int(argv[0]))
        return argv[0];

    return Fa_make_real(std::ceil(Fa_as_double_any(argv[0])));
}

Fa_Value Fa_VM::Fa_round(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::ROUND_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }
    if (!Fa_is_number(argv[0])) {
        stdlib_error(StdlibErrorCode::ROUND_TYPE_ERROR);
        return Fa_make_nil();
    }
    if (Fa_is_int(argv[0]))
        return argv[0];

    return Fa_make_real(std::round(Fa_as_double_any(argv[0])));
}

Fa_Value Fa_VM::Fa_abs(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::ABS_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    if (!Fa_is_number(argv[0])) {
        stdlib_error(StdlibErrorCode::ABS_TYPE_ERROR);
        return Fa_make_nil();
    }

    if (Fa_is_int(argv[0])) {
        i64 v = Fa_as_int(argv[0]);
        if (v == INT64_MIN) {
            stdlib_error(StdlibErrorCode::ABS_OUT_OF_RANGE);
            return Fa_make_nil();
        }
        return Fa_make_int(std::abs(Fa_as_int(argv[0])));
    }
    return Fa_make_real(std::fabs(Fa_as_double(argv[0])));
}

Fa_Value Fa_VM::Fa_min(int argc, Fa_Value* argv)
{
    if (argc < 1 || !argv) {
        stdlib_error(StdlibErrorCode::MIN_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    // Determine mode from argv[0]
    bool all_ints = Fa_is_int(argv[0]);
    bool all_strs = Fa_is_string(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; i += 1) {
        if (!Fa_is_int(argv[i]))
            all_ints = false;
        if (!Fa_is_string(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; i += 1) {
            if (Fa_as_string(argv[i])->str < Fa_as_string(ret)->str)
                ret = argv[i];
        }

        return ret;
    }

    Fa_Value ret = Fa_make_real(Fa_as_double_any(argv[0]));
    for (int i = 1; i < argc; i += 1)
        ret = Fa_make_real(std::fmin(Fa_as_double_any(ret), Fa_as_double_any(argv[i])));

    if (all_ints)
        return Fa_make_int(static_cast<i64>(Fa_as_double_any(ret)));

    return ret;
}

Fa_Value Fa_VM::Fa_max(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::MAX_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    // Determine mode from argv[0]
    bool all_ints = Fa_is_int(argv[0]);
    bool all_strs = Fa_is_string(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; i += 1) {
        if (!Fa_is_int(argv[i]))
            all_ints = false;
        if (!Fa_is_string(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; i += 1) {
            if (Fa_as_string(argv[i])->str > Fa_as_string(ret)->str)
                ret = argv[i];
        }
        return ret;
    }

    Fa_Value ret = Fa_make_real(Fa_as_double_any(argv[0]));
    for (int i = 1; i < argc; i += 1)
        ret = Fa_make_real(std::fmax(Fa_as_double_any(ret), Fa_as_double_any(argv[i])));

    if (all_ints)
        return Fa_make_int(static_cast<i64>(Fa_as_double_any(ret)));

    return ret;
}

Fa_Value Fa_VM::Fa_pow(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::POW_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    Fa_Value base = argv[0];
    Fa_Value exponent = argv[1];

    if (UNLIKELY(!Fa_is_number(base) || !Fa_is_number(exponent))) {
        stdlib_error(StdlibErrorCode::POW_TYPE_ERROR);
        return Fa_make_nil();
    }

    if (Fa_is_int(base) && Fa_is_int(exponent))
        // return an int even if the result may be larger than 48 bit range
        return Fa_make_int(std::pow(Fa_as_int(base), Fa_as_int(exponent)));
    else
        return Fa_make_real(std::pow(Fa_as_double_any(base), Fa_as_double_any(exponent)));

    return Fa_make_nil();
}

Fa_Value Fa_VM::Fa_sqrt(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::SQRT_ARG_COUNT, "got " + std::to_string(argc));
        return Fa_make_nil();
    }

    Fa_Value n = argv[0];

    if (UNLIKELY(!Fa_is_number(n))) {
        stdlib_error(StdlibErrorCode::SQRT_TYPE_ERROR);
        return Fa_make_nil();
    }

    f64 val = Fa_as_double_any(n);
    if (val < 0.0)
        return Fa_make_nil();

    return Fa_make_real(std::sqrt(val));
}

Fa_Value Fa_VM::Fa_assert(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::ASSERT_ARG_COUNT, "got" + std::to_string(argc));
        return Fa_make_nil();
    }

    for (int i = 0; i < argc; i += 1) {
        if (!Fa_is_truthy(argv[i])) // eval entire expr
            stdlib_error(StdlibErrorCode::ASSERT_FAILED);
    }

    return Fa_make_nil(); // success
}

Fa_Value Fa_VM::Fa_open(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::OPEN_ARG_COUNT);
        return Fa_make_nil();
    }

    char const* filename = Fa_as_string(argv[0])->str.data();
    Fa_StringRef mode_arg = Fa_as_string(argv[1])->str;

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
        return Fa_make_nil();
    }

    return m_gc.make_file_handle(fp);
}

Fa_Value Fa_VM::Fa_append_file(int argc, Fa_Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_ARG_COUNT);
        return Fa_make_nil();
    }

    Fa_Value& file = argv[0];
    Fa_Value& content = argv[1];

    if (!Fa_is_file_handle(file)) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_TYPE_ERROR);
        return Fa_make_nil();
    }

    if (!Fa_is_string(content)) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_TYPE_ERROR);
        return Fa_make_nil();
    }

    Fa_ObjFileHandle* file_handle = Fa_as_file_handle(file);
    Fa_ObjString* str_obj = Fa_as_string(content);
    FILE* fp = file_handle->fp;
    Fa_StringRef content_str = str_obj->str;

    if (content_str.empty())
        return Fa_make_bool(true); // nothing to write is trivially successful

    size_t const written = std::fwrite(content_str.data(), 1, content_str.len(), fp);
    // ::fflush(fp);

    if (written != content_str.len()) {
        stdlib_error(StdlibErrorCode::APPEND_FILE_FAILED, std::strerror(errno));
        return Fa_make_bool(false);
    }

    return Fa_make_bool(true);
}

Fa_Value Fa_VM::Fa_close(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        stdlib_error(StdlibErrorCode::CLOSE_ARG_COUNT);
        return Fa_make_bool(false);
    }

    if (!Fa_is_file_handle(argv[0])) {
        stdlib_error(StdlibErrorCode::CLOSE_TYPE_ERROR);
        return Fa_make_bool(false);
    }

    Fa_ObjFileHandle* file_handle = Fa_as_file_handle(argv[0]);
    if (file_handle->close())
        return Fa_make_bool(true);

    return Fa_make_bool(false);
}

Fa_Value Fa_VM::Fa_clock(int /*argc*/, Fa_Value* /*argv*/) { return Fa_make_nil(); }
Fa_Value Fa_VM::Fa_error(int /*argc*/, Fa_Value* /*argv*/) { return Fa_make_nil(); }
Fa_Value Fa_VM::Fa_time(int /*argc*/, Fa_Value* /*argv*/) { return Fa_make_nil(); }

// stdlib helpers
void Fa_VM::Fa_dict_put(Fa_Value* dict_ptr, Fa_Value k, Fa_Value v)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return;

    Fa_ObjDict* as_dict = Fa_as_dict(*dict_ptr);
    as_dict->data[k] = v;
}

Fa_Value Fa_VM::Fa_dict_get(Fa_Value* dict_ptr, Fa_Value k)
{
    if (UNLIKELY(dict_ptr == nullptr))
        return Fa_make_nil();

    Fa_ObjDict* as_dict = Fa_as_dict(*dict_ptr);
    return as_dict->data[k];
}

} // namespace fairuz::runtime
