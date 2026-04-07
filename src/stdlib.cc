/// stdlib.cc

#include "../include/util.hpp"
#include "../include/vm.hpp"
#include "value.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fairuz::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;
using StdlibError = diagnostic::errc::stdlib::Code;

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
    if (Fa_IS_NIL(v)) {
        out += "nil";
        return;
    }
    if (Fa_IS_BOOL(v)) {
        out += Fa_AS_BOOL(v) ? "true" : "false";
        return;
    }
    if (Fa_IS_INTEGER(v)) {
        out += Fa_StringRef(std::to_string(Fa_AS_INTEGER(v)).c_str());
        return;
    }
    if (Fa_IS_DOUBLE(v)) {
        f64 d = Fa_AS_DOUBLE(v);
        if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15) {
            out += Fa_StringRef(std::to_string(static_cast<i64>(d)).c_str());
        } else {
            out += format_double_string(d);
        }
        return;
    }
    if (Fa_IS_STRING(v)) {
        if (quote_strings)
            out += '"';
        out += Fa_AS_STRING(v)->str;
        if (quote_strings)
            out += '"';
        return;
    }
    if (Fa_IS_LIST(v)) {
        Fa_ObjList* list = Fa_AS_LIST(v);
        out += '[';
        for (u32 i = 0, n = list->elements.size(); i < n; i += 1) {
            if (i > 0)
                out += ", ";
            append_rendered_value(out, list->elements[i], true);
        }
        out += ']';
        return;
    }
    if (Fa_IS_DICT(v)) {
        Fa_ObjDict* dict = Fa_AS_DICT(v);
        out += '{';
        for (auto [k, v] : dict->data) {
            append_rendered_value(out, k, Fa_IS_STRING(k));
            out += ": ";
            append_rendered_value(out, v, Fa_IS_STRING(v));
        }
        
        out += '}';
        return;
    }
    if (Fa_IS_CLOSURE(v)) {
        out += "<function>";
        return;
    }
    if (Fa_IS_NATIVE(v)) {
        out += "<native>";
        return;
    }
    if (Fa_IS_FUNCTION(v)) {
        out += "<function>";
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
        return NIL_VAL;

    if (argc == 1) {
        if (Fa_IS_STRING(argv[0])) {
            Fa_StringRef const& str = Fa_AS_STRING(argv[0])->str;
            size_t byte_pos = 0;
            i64 char_count = 0;

            while (byte_pos < str.len()) {
                u64 step = 0;
                util::decode_utf8_at(str, byte_pos, &step);
                byte_pos += step;
                char_count += 1;
            }

            return Fa_MAKE_INTEGER(char_count);
        }

        if (Fa_IS_LIST(argv[0]))
            return Fa_MAKE_INTEGER(Fa_AS_LIST(argv[0])->elements.size());
        if (Fa_IS_DICT(argv[0]))
            return Fa_MAKE_INTEGER(Fa_AS_DICT(argv[0])->data.size());
    }

    /// do not accept multiple args for len
    return NIL_VAL;
}

static void print_runtime_value(Fa_Value v, int depth = 0)
{
    if (Fa_IS_NIL(v)) {
        std::cout << "nil";
        return;
    }

    if (Fa_IS_BOOL(v)) {
        std::cout << (Fa_AS_BOOL(v) ? "true" : "false");
        return;
    }

    if (Fa_IS_INTEGER(v)) {
        std::cout << Fa_AS_INTEGER(v);
        return;
    }

    if (Fa_IS_OBJECT(v)) {
        Fa_ObjHeader* obj = Fa_AS_OBJECT(v);

        switch (obj->type) {
        case Fa_ObjType::STRING:
            std::cout << static_cast<Fa_ObjString*>(obj)->str;
            return;

        case Fa_ObjType::LIST: {
            auto list = static_cast<Fa_ObjList*>(obj);
            std::cout << '[';
            for (u32 i = 0, n = list->elements.size(); i < n; i += 1) {
                if (i > 0)
                    std::cout << ", ";
                Fa_Value elem = list->elements[i];
                if (Fa_IS_OBJECT(elem) && Fa_AS_OBJECT(elem)->type == Fa_ObjType::STRING) {
                    std::cout << '"';
                    std::cout << Fa_AS_STRING((elem))->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(elem, depth + 1);
                }
            }
            std::cout << ']';
            return;
        }

        case Fa_ObjType::DICT: {
            auto dict = static_cast<Fa_ObjDict*>(obj);
            std::cout << '{';
            for (auto [k, v] : dict->data) {
                if (Fa_IS_STRING(k)) {
                    std::cout << '"';
                    std::cout << Fa_AS_STRING(k)->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(k, depth + 1);
                }
                std::cout << ": ";
                if (Fa_IS_STRING(v)) {
                    std::cout << '"';
                    std::cout << Fa_AS_STRING(v)->str;
                    std::cout << '"';
                } else {
                    print_runtime_value(v, depth + 1);
                }
                std::cout << ", ";
            }
            std::cout << '}';
            return;
        }

        case Fa_ObjType::CLOSURE: {
            auto cl = static_cast<Fa_ObjClosure*>(obj);
            std::cout << "<function ";
            if (cl->function && cl->function->name)
                std::cout << cl->function->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case Fa_ObjType::NATIVE: {
            auto nat = static_cast<Fa_ObjNative*>(obj);
            std::cout << "<native ";
            if (nat->name)
                std::cout << nat->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case Fa_ObjType::FUNCTION: {
            auto fn = static_cast<Fa_ObjFunction*>(obj);
            std::cout << "<function ";
            if (fn->name)
                std::cout << fn->name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }
        }
    }

    f64 d = Fa_AS_DOUBLE(v);
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
        return NIL_VAL;
    }

    for (int i = 0; i < argc; i += 1) {
        if (i > 0)
            std::cout << '\t';
        print_runtime_value(argv[i]);
    }
    std::cout << '\n';
    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_type(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return NIL_VAL;

    return Fa_MAKE_INTEGER(static_cast<i64>(value_type_tag(argv[0])));
}

Fa_Value Fa_VM::Fa_int(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return NIL_VAL;
    if (Fa_IS_NUMBER(argv[0]))
        return Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_DOUBLE_ANY(argv[0])));
    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_float(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return NIL_VAL;

    if (Fa_IS_NUMBER(argv[0]))
        return Fa_MAKE_REAL(Fa_AS_DOUBLE_ANY(argv[0]));

    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_append(int argc, Fa_Value* argv)
{
    if (argc < 2 || argv == nullptr) {
        diagnostic::emit(StdlibError::APPEND_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_Value& list_v = argv[0];
    if (!Fa_IS_LIST(list_v)) {
        diagnostic::emit(StdlibError::APPEND_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    Fa_ObjList* list_obj = Fa_AS_LIST(list_v);

    for (int i = 1; i < argc; i += 1)
        list_obj->elements.push(argv[i]);

    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_pop(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::POP_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_Value& list_v = argv[0];
    if (!Fa_IS_LIST(list_v)) {
        diagnostic::emit(StdlibError::POP_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    Fa_AS_LIST(list_v)->elements.pop();
    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_slice(int argc, Fa_Value* argv)
{
    /// cut a copy of a list, with inclusive indices
    /// accept [list, a, b]
    /// a, b are the indices
    /// if b is null then cut [a:]

    if (argc < 2) {
        diagnostic::emit(StdlibError::SLICE_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_ObjList* list_obj = Fa_AS_LIST(argv[0]);
    Fa_Value ret = Fa_MAKE_LIST();
    Fa_ObjList* ret_list = Fa_AS_LIST(ret);
    /// Expects indices to be ints
    u32 a = Fa_AS_INTEGER(argv[1]);
    u32 b = argc == 3 ? Fa_AS_INTEGER(argv[2]) : list_obj->size() - 1;

    for (int i = a; i <= b; i += 1)
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
        return NIL_VAL;

    ret_str = help.data();
    Fa_Value ret = Fa_MAKE_STRING(ret_str);
    return ret;
}

Fa_Value Fa_VM::Fa_str(int argc, Fa_Value* argv)
{
    if (argc > 1) {
        diagnostic::emit(StdlibError::STR_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_StringRef output = "";

    if (argc == 0 || argv == nullptr)
        return Fa_MAKE_OBJECT((Fa_MAKE_OBJ_STRING(output))); // return empty on no arg

    if (Fa_IS_STRING(argv[0]))
        return Fa_MAKE_STRING(Fa_AS_STRING(argv[0])->str);

    Fa_StringRef rendered = value_to_string(argv[0]);
    return Fa_MAKE_STRING(rendered);
}

Fa_Value Fa_VM::Fa_bool(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::BOOL_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    return Fa_IS_TRUTHY(argv[0]) ? Fa_MAKE_BOOL(true) : Fa_MAKE_BOOL(false);
}

Fa_Value Fa_VM::Fa_list(int argc, Fa_Value* argv)
{
    Fa_Value ret = Fa_MAKE_LIST();
    Fa_ObjList* list_obj = Fa_AS_LIST(ret);

    for (int i = 0; i < argc; i += 1)
        list_obj->elements.push(argv[i]);

    return ret;
}

Fa_Value Fa_VM::Fa_dict(int argc, Fa_Value* argv) { return Fa_MAKE_DICT(); }

Fa_Value Fa_VM::Fa_split(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return NIL_VAL;
    if (!Fa_IS_STRING(argv[0]) || !Fa_IS_STRING(argv[1]))
        return NIL_VAL;

    Fa_StringRef src = Fa_AS_STRING(argv[0])->str;
    Fa_StringRef delim = Fa_AS_STRING(argv[1])->str;

    Fa_Value ret = Fa_MAKE_LIST();
    Fa_ObjList* list = Fa_AS_LIST(ret);

    if (delim.empty()) {
        list->elements.push(Fa_MAKE_STRING(src));
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
            list->elements.push(Fa_MAKE_STRING(src.substr(start, src.len())));
            break;
        }

        list->elements.push(Fa_MAKE_STRING(src.substr(start, pos)));
        start = pos + delim.len();
    }

    return ret;
}

Fa_Value Fa_VM::Fa_join(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return NIL_VAL;
    if (!Fa_IS_LIST(argv[0]) || !Fa_IS_STRING(argv[1]))
        return NIL_VAL;

    Fa_ObjList* list = Fa_AS_LIST(argv[0]);
    Fa_StringRef delim = Fa_AS_STRING(argv[1])->str;
    Fa_StringRef out = "";

    for (u32 i = 0; i < list->elements.size(); i += 1) {
        if (i > 0)
            out += delim;

        out += value_to_string(list->elements[i]);
    }

    return Fa_MAKE_STRING(out);
}

Fa_Value Fa_VM::Fa_substr(int argc, Fa_Value* argv)
{
    if (argc != 3 || argv == nullptr) {
        diagnostic::emit(StdlibError::SUBSTR_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_StringRef str = Fa_AS_STRING(argv[0])->str;
    Fa_Value a = Fa_AS_INTEGER(argv[1]);
    Fa_Value b = Fa_AS_INTEGER(argv[2]);
    /// TODO: check a, b types

    Fa_StringRef ret = str.substr(a, b);
    return Fa_MAKE_STRING(ret);
}

Fa_Value Fa_VM::Fa_contains(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return NIL_VAL;
    if (!Fa_IS_STRING(argv[0]) || !Fa_IS_STRING(argv[1]))
        return NIL_VAL;

    Fa_StringRef haystack = Fa_AS_STRING(argv[0])->str;
    Fa_StringRef needle = Fa_AS_STRING(argv[1])->str;
    if (needle.empty())
        return Fa_MAKE_BOOL(true);

    return Fa_MAKE_BOOL(haystack.find(needle));
}

Fa_Value Fa_VM::Fa_trim(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return NIL_VAL;
    if (!Fa_IS_STRING(argv[0]))
        return NIL_VAL;

    Fa_StringRef str = Fa_AS_STRING(argv[0])->str;
    size_t start = 0;
    size_t end = str.len();

    auto is_trim_space = [](char ch) {
        return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
    };

    while (start < end && is_trim_space(str[start]))
        start += 1;
    while (end > start && is_trim_space(str[end - 1]))
        end -= 1;

    return Fa_MAKE_STRING(str.substr_copy(start, end));
}

Fa_Value Fa_VM::Fa_floor(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::FLOOR_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    if (!Fa_IS_NUMBER(argv[0])) {
        diagnostic::emit(StdlibError::FLOOR_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    if (Fa_IS_INTEGER(argv[0]))
        return argv[0];

    return Fa_MAKE_REAL(std::floor(Fa_AS_DOUBLE(argv[0])));
}

Fa_Value Fa_VM::Fa_ceil(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::CEIL_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    if (!Fa_IS_NUMBER(argv[0])) {
        diagnostic::emit(StdlibError::CEIL_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    if (Fa_IS_INTEGER(argv[0]))
        return argv[0];

    return Fa_MAKE_REAL(std::ceil(Fa_AS_DOUBLE_ANY(argv[0])));
}

Fa_Value Fa_VM::Fa_round(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::ROUND_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }
    if (!Fa_IS_NUMBER(argv[0])) {
        diagnostic::emit(StdlibError::ROUND_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }
    if (Fa_IS_INTEGER(argv[0]))
        return argv[0];

    return Fa_MAKE_REAL(std::round(Fa_AS_DOUBLE_ANY(argv[0])));
}

Fa_Value Fa_VM::Fa_abs(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::ABS_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    if (!Fa_IS_NUMBER(argv[0])) {
        diagnostic::emit(StdlibError::ABS_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    if (Fa_IS_INTEGER(argv[0])) {
        i64 v = Fa_AS_INTEGER(argv[0]);
        if (v == INT64_MIN) {
            diagnostic::emit(StdlibError::ABS_OUT_OF_RANGE);
            return NIL_VAL;
        }
        return Fa_MAKE_INTEGER(std::abs(Fa_AS_INTEGER(argv[0])));
    }
    return Fa_MAKE_REAL(std::fabs(Fa_AS_DOUBLE(argv[0])));
}

Fa_Value Fa_VM::Fa_min(int argc, Fa_Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit(StdlibError::MIN_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    // Determine mode from argv[0]
    bool all_ints = Fa_IS_INTEGER(argv[0]);
    bool all_strs = Fa_IS_STRING(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; i += 1) {
        if (!Fa_IS_INTEGER(argv[i]))
            all_ints = false;
        if (!Fa_IS_STRING(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; i += 1) {
            if (Fa_AS_STRING(argv[i])->str < Fa_AS_STRING(ret)->str)
                ret = argv[i];
        }

        return ret;
    }

    Fa_Value ret = Fa_MAKE_REAL(Fa_AS_DOUBLE_ANY(argv[0]));
    for (int i = 1; i < argc; i += 1)
        ret = Fa_MAKE_REAL(std::fmin(Fa_AS_DOUBLE_ANY(ret), Fa_AS_DOUBLE_ANY(argv[i])));

    if (all_ints)
        return Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_DOUBLE_ANY(ret)));

    return ret;
}

Fa_Value Fa_VM::Fa_max(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::MAX_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    // Determine mode from argv[0]
    bool all_ints = Fa_IS_INTEGER(argv[0]);
    bool all_strs = Fa_IS_STRING(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; i += 1) {
        if (!Fa_IS_INTEGER(argv[i]))
            all_ints = false;
        if (!Fa_IS_STRING(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; i += 1) {
            if (Fa_AS_STRING(argv[i])->str > Fa_AS_STRING(ret)->str)
                ret = argv[i];
        }
        return ret;
    }

    Fa_Value ret = Fa_MAKE_REAL(Fa_AS_DOUBLE_ANY(argv[0]));
    for (int i = 1; i < argc; i += 1)
        ret = Fa_MAKE_REAL(std::fmax(Fa_AS_DOUBLE_ANY(ret), Fa_AS_DOUBLE_ANY(argv[i])));

    if (all_ints)
        return Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_DOUBLE_ANY(ret)));

    return ret;
}

Fa_Value Fa_VM::Fa_pow(int argc, Fa_Value* argv)
{
    if (argc != 2 || argv == nullptr) {
        diagnostic::emit(StdlibError::POW_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_Value base = argv[0];
    Fa_Value exponent = argv[1];

    if (UNLIKELY(!Fa_IS_NUMBER(base) || !Fa_IS_NUMBER(exponent))) {
        diagnostic::emit(StdlibError::POW_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    if (Fa_IS_INTEGER(base) && Fa_IS_INTEGER(exponent))
        // return an int even if the result may be larger than 48 bit range
        return Fa_MAKE_INTEGER(std::pow(Fa_AS_INTEGER(base), Fa_AS_INTEGER(exponent)));
    else
        return Fa_MAKE_REAL(std::pow(Fa_AS_DOUBLE_ANY(base), Fa_AS_DOUBLE_ANY(exponent)));

    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_sqrt(int argc, Fa_Value* argv)
{
    if (argc != 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::SQRT_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    Fa_Value n = argv[0];

    if (UNLIKELY(!Fa_IS_NUMBER(n))) {
        diagnostic::emit(StdlibError::SQRT_TYPE_ERROR);
        diagnostic::runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
        return NIL_VAL;
    }

    f64 val = Fa_AS_DOUBLE_ANY(n);
    if (val < 0.0)
        return NIL_VAL;

    return Fa_MAKE_REAL(std::sqrt(val));
}

Fa_Value Fa_VM::Fa_assert(int argc, Fa_Value* argv)
{
    if (argc < 1 || argv == nullptr) {
        diagnostic::emit(StdlibError::ASSERT_ARG_COUNT, "got" + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    for (int i = 0; i < argc; i += 1) {
        if (!Fa_IS_TRUTHY(argv[i])) // eval entire expr
        {
            Fa_SourceLocation loc = current_location();
            /// TODO: find a way to print the line code with pretty error
            diagnostic::report(diagnostic::Severity::ERROR, loc.line, loc.column, ErrorCode::ASSERTION_FAILED);
            diagnostic::runtime_error(ErrorCode::ASSERTION_FAILED);
        }
    }

    return NIL_VAL; // success
}

Fa_Value Fa_VM::Fa_clock(int /*argc*/, Fa_Value* /*argv*/) { return NIL_VAL; }
Fa_Value Fa_VM::Fa_error(int /*argc*/, Fa_Value* /*argv*/) { return NIL_VAL; }
Fa_Value Fa_VM::Fa_time(int /*argc*/, Fa_Value* /*argv*/) { return NIL_VAL; }

// stdlib helpers
void Fa_VM::Fa_dict_put(Fa_Value* dict_ptr, Fa_Value k, Fa_Value v) {
    if (UNLIKELY(dict_ptr == nullptr))
        return;
    
    Fa_ObjDict* as_dict = Fa_AS_DICT(*dict_ptr);
    as_dict->data[k] = v;
}

Fa_Value Fa_VM::Fa_dict_get(Fa_Value* dict_ptr, Fa_Value k) {
    if (UNLIKELY(dict_ptr == nullptr))
        return NIL_VAL;
    
    Fa_ObjDict* as_dict = Fa_AS_DICT(*dict_ptr);
    return as_dict->data[k];
}

} // namespace fairuz::runtime
