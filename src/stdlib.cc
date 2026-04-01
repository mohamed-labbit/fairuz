/// stdlib.cc

#include "../include/vm.hpp"
#include "diagnostic.hpp"
#include "macros.hpp"
#include "pair.hpp"
#include "value.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fairuz::runtime {

using ErrorCode = diagnostic::errc::runtime::Code;
using StdlibError = diagnostic::errc::stdlib::Code;

static Fa_StringRef value_to_string(Fa_Value v)
{
    if (Fa_IS_NIL(v))
        return "nil";
    if (Fa_IS_BOOL(v))
        return Fa_AS_BOOL(v) ? "true" : "false";
    if (Fa_IS_INTEGER(v))
        return Fa_StringRef(std::to_string(Fa_AS_INTEGER(v)).c_str());
    if (Fa_IS_DOUBLE(v)) {
        std::ostringstream oss;
        oss << std::setprecision(14) << std::noshowpoint << Fa_AS_DOUBLE(v);
        return Fa_StringRef(oss.str().c_str());
    }
    if (Fa_IS_STRING(v))
        return Fa_AS_STRING(v)->str;
    if (Fa_IS_LIST(v))
        return "<list>";
    if (Fa_IS_CLOSURE(v))
        return "<function>";
    if (Fa_IS_NATIVE(v))
        return "<native>";
    if (Fa_IS_FUNCTION(v))
        return "<function>";
    return "";
}

Fa_Value Fa_VM::Fa_len(int argc, Fa_Value* argv)
{
    if (argc == 0 || !argv)
        return NIL_VAL;

    if (argc == 1) {
        if (Fa_IS_STRING(argv[0]))
            return Fa_MAKE_INTEGER(Fa_AS_STRING(argv[0])->str.len());
        if (Fa_IS_LIST(argv[0]))
            return Fa_MAKE_INTEGER(Fa_AS_LIST(argv[0])->m_elements.size());
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

        switch (obj->m_type) {
        case Fa_ObjType::STRING:
            std::cout << static_cast<Fa_ObjString*>(obj)->str;
            return;

        case Fa_ObjType::LIST: {
            auto list = static_cast<Fa_ObjList*>(obj);
            std::cout << '[';
            for (u32 i = 0, n = list->m_elements.size(); i < n; ++i) {
                if (i > 0)
                    std::cout << ", ";
                Fa_Value elem = list->m_elements[i];
                if (Fa_IS_OBJECT(elem) && Fa_AS_OBJECT(elem)->m_type == Fa_ObjType::STRING) {
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
            for (u32 i = 0, n = dict->data.size(); i < n; ++i) {
                if (i > 0)
                    std::cout << ",\n";
                auto [k, v] = dict->data[i];
                
            }
            return;
        }

        case Fa_ObjType::CLOSURE: {
            auto cl = static_cast<Fa_ObjClosure*>(obj);
            std::cout << "<function ";
            if (cl->function && cl->function->m_name)
                std::cout << cl->function->m_name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case Fa_ObjType::NATIVE: {
            auto nat = static_cast<Fa_ObjNative*>(obj);
            std::cout << "<native ";
            if (nat->m_name)
                std::cout << nat->m_name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }

        case Fa_ObjType::FUNCTION: {
            auto fn = static_cast<Fa_ObjFunction*>(obj);
            std::cout << "<function ";
            if (fn->m_name)
                std::cout << fn->m_name->str;
            else
                std::cout << "?";
            std::cout << '>';
            return;
        }
        
        case Fa_ObjType::UPVALUE:
            std::cout << "<upvalue>";
            return;
        }
    }

    f64 d = Fa_AS_DOUBLE(v);
    if (d == std::floor(d) && std::isfinite(d) && std::abs(d) < 1e15) {
        std::cout << static_cast<i64>(d);
    } else {
        std::ostringstream oss;
        oss << std::setprecision(14) << std::noshowpoint << d;
        std::cout << oss.str();
    }
}

Fa_Value Fa_VM::Fa_print(int argc, Fa_Value* argv)
{
    if (argc == 0 || !argv) {
        std::cout << '\n';
        return NIL_VAL;
    }

    for (int i = 0; i < argc; ++i) {
        if (i > 0)
            std::cout << '\t';
        print_runtime_value(argv[i]);
    }
    std::cout << '\n';
    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_type(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    return Fa_MAKE_INTEGER(static_cast<i64>(value_type_tag(argv[0])));
}

Fa_Value Fa_VM::Fa_int(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;
    if (Fa_IS_NUMBER(argv[0]))
        return Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_DOUBLE_ANY(argv[0])));
    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_float(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;

    if (Fa_IS_NUMBER(argv[0]))
        return Fa_MAKE_REAL(Fa_AS_DOUBLE_ANY(argv[0]));

    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_append(int argc, Fa_Value* argv)
{
    if (argc < 2 || !argv) {
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

    for (int i = 1; i < argc; ++i)
        list_obj->m_elements.push(argv[i]);

    return NIL_VAL;
}

Fa_Value Fa_VM::Fa_pop(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv) {
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

    Fa_AS_LIST(list_v)->m_elements.pop();
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

    for (int i = a; i <= b; ++i)
        ret_list->m_elements.push(list_obj->m_elements[i]);

    return ret;
}

Fa_Value Fa_VM::Fa_input(int /*argc*/, Fa_Value* /*argv*/) // input takes no args for now
{
    // read until user hits ENTER
    Fa_StringRef ret_str = "";
    std::string help = ""; // getline only accepts std::string
    if (!std::getline(std::cin, help))
    {
        // don't know what error to report
        return NIL_VAL;
    }
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

    if (argc == 0 || !argv)
        return Fa_MAKE_OBJECT((Fa_MAKE_OBJ_STRING(output))); // return empty on no arg

    /// TODO : stringify a list

    if (Fa_IS_STRING(argv[0]))
        output = Fa_AS_STRING(argv[0])->str;
    else if (Fa_IS_BOOL(argv[0]))
        output = Fa_AS_BOOL(argv[0]) ? "true" : "false";
    else if (Fa_IS_DOUBLE(argv[0]))
        output = std::to_string(Fa_AS_DOUBLE(argv[0])).data();
    else if (Fa_IS_INTEGER(argv[0]))
        output = std::to_string(Fa_AS_INTEGER(argv[0])).data();

    return Fa_MAKE_STRING(output);
}

Fa_Value Fa_VM::Fa_bool(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv) {
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

    for (int i = 0; i < argc; ++i)
        list_obj->m_elements.push(argv[i]);

    return ret;
}

Fa_Value Fa_VM::Fa_dict(int argc, Fa_Value* argv) {
    Fa_Value ret = Fa_MAKE_DICT();
    Fa_ObjDict* dict_obj = Fa_AS_DICT(ret);

    // this is structurally false because it relys on the structure of the dict
    // to have only one single key for each value (1 : 1)
    for (int i = 0; i < argc - 1; i += 2)
        dict_obj->data.push({ argv[i], argv[i + 1] });

    return ret;
}

Fa_Value Fa_VM::Fa_split(int argc, Fa_Value* argv)
{
    if (argc != 2 || !argv)
        return NIL_VAL;
    if (!Fa_IS_STRING(argv[0]) || !Fa_IS_STRING(argv[1]))
        return NIL_VAL;

    Fa_StringRef src = Fa_AS_STRING(argv[0])->str;
    Fa_StringRef delim = Fa_AS_STRING(argv[1])->str;

    Fa_Value ret = Fa_MAKE_LIST();
    Fa_ObjList* list = Fa_AS_LIST(ret);

    if (delim.empty()) {
        list->m_elements.push(Fa_MAKE_STRING(src));
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
            ++pos;
        }

        if (!found) {
            list->m_elements.push(Fa_MAKE_STRING(src.substr(start, src.len())));
            break;
        }

        list->m_elements.push(Fa_MAKE_STRING(src.substr(start, pos)));
        start = pos + delim.len();
    }

    return ret;
}

Fa_Value Fa_VM::Fa_join(int argc, Fa_Value* argv)
{
    if (argc != 2 || !argv)
        return NIL_VAL;
    if (!Fa_IS_LIST(argv[0]) || !Fa_IS_STRING(argv[1]))
        return NIL_VAL;

    Fa_ObjList* list = Fa_AS_LIST(argv[0]);
    Fa_StringRef delim = Fa_AS_STRING(argv[1])->str;
    Fa_StringRef out = "";

    for (u32 i = 0; i < list->m_elements.size(); ++i) {
        if (i > 0)
            out += delim;
        out += value_to_string(list->m_elements[i]);
    }

    return Fa_MAKE_STRING(out);
}

Fa_Value Fa_VM::Fa_substr(int argc, Fa_Value* argv)
{
    if (argc != 3 || !argv) {
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
    if (argc != 2 || !argv)
        return NIL_VAL;
    if (!Fa_IS_STRING(argv[0]) || !Fa_IS_STRING(argv[1]))
        return NIL_VAL;

    return Fa_MAKE_BOOL(Fa_AS_STRING(argv[0])->str.find(Fa_AS_STRING(argv[1])->str));
}

Fa_Value Fa_VM::Fa_trim(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv)
        return NIL_VAL;
    if (!Fa_IS_STRING(argv[0]))
        return NIL_VAL;

    Fa_StringRef out = Fa_AS_STRING(argv[0])->str;
    out.trim_whitespace();
    return Fa_MAKE_STRING(out.substr(0, out.len()));
}

Fa_Value Fa_VM::Fa_floor(int argc, Fa_Value* argv)
{
    if (argc != 1 || !argv) {
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
    if (argc != 1 || !argv) {
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
    if (argc != 1 || !argv) {
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
    if (argc != 1 || !argv) {
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
    for (int i = 1; i < argc; ++i) {
        if (!Fa_IS_INTEGER(argv[i]))
            all_ints = false;
        if (!Fa_IS_STRING(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; ++i) {
            if (Fa_AS_STRING(argv[i])->str < Fa_AS_STRING(ret)->str)
                ret = argv[i];
        }
        return ret;
    }

    Fa_Value ret = Fa_MAKE_REAL(Fa_AS_DOUBLE_ANY(argv[0]));
    for (int i = 1; i < argc; ++i)
        ret = Fa_MAKE_REAL(std::fmin(Fa_AS_DOUBLE_ANY(ret), Fa_AS_DOUBLE_ANY(argv[i])));

    if (all_ints)
        return Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_DOUBLE_ANY(ret)));
    return ret;
}

Fa_Value Fa_VM::Fa_max(int argc, Fa_Value* argv)
{
    if (argc < 1 || !argv) {
        diagnostic::emit(StdlibError::MAX_ARG_COUNT, "got " + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }

    // Determine mode from argv[0]
    bool all_ints = Fa_IS_INTEGER(argv[0]);
    bool all_strs = Fa_IS_STRING(argv[0]);

    // Validate all args match the expected type
    for (int i = 1; i < argc; ++i) {
        if (!Fa_IS_INTEGER(argv[i]))
            all_ints = false;
        if (!Fa_IS_STRING(argv[i]))
            all_strs = false;
    }

    if (all_strs) {
        Fa_Value ret = argv[0];
        for (int i = 1; i < argc; ++i) {
            if (Fa_AS_STRING(argv[i])->str > Fa_AS_STRING(ret)->str)
                ret = argv[i];
        }
        return ret;
    }

    Fa_Value ret = Fa_MAKE_REAL(Fa_AS_DOUBLE_ANY(argv[0]));
    for (int i = 1; i < argc; ++i)
        ret = Fa_MAKE_REAL(std::fmax(Fa_AS_DOUBLE_ANY(ret), Fa_AS_DOUBLE_ANY(argv[i])));

    if (all_ints)
        return Fa_MAKE_INTEGER(static_cast<i64>(Fa_AS_DOUBLE_ANY(ret)));
    return ret;
}

Fa_Value Fa_VM::Fa_pow(int argc, Fa_Value* argv)
{
    if (argc != 2 || !argv) {
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
    if (argc != 1 || !argv) {
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
    if (argc < 1 || !argv)
    {
        diagnostic::emit(StdlibError::ASSERT_ARG_COUNT, "got" + std::to_string(argc));
        diagnostic::runtime_error(ErrorCode::NATIVE_ARG_COUNT);
        return NIL_VAL;
    }
    
    for (int i = 0; i < argc; ++i)
    {
        if (!Fa_IS_TRUTHY(argv[i])) // eval entire expr
        {
            Fa_SourceLocation loc = current_location();
            /// TODO: find a way to print the line code with pretty error
            diagnostic::report(diagnostic::Severity::ERROR, loc.m_line, loc.m_column, ErrorCode::ASSERTION_FAILED);
            diagnostic::runtime_error(ErrorCode::ASSERTION_FAILED);
        }
    }

    return NIL_VAL; // success
}

Fa_Value Fa_VM::Fa_clock(int /*argc*/, Fa_Value* /*argv*/) { return NIL_VAL; }
Fa_Value Fa_VM::Fa_error(int /*argc*/, Fa_Value* /*argv*/) { return NIL_VAL; }
Fa_Value Fa_VM::Fa_time(int /*argc*/, Fa_Value* /*argv*/) { return NIL_VAL; }

} // namespace fairuz::runtime
