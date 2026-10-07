#pragma once

#include "../fobject.hpp"
#include "../fstring.hpp"
#include "../fvalue.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace fairuz::runtime {

static constexpr u32 MAX_RENDER_DEPTH = 128;

static inline StringRef byte_string(std::string_view bytes)
{
    StringRef result(bytes.size(), '\0');
    if (!bytes.empty())
        std::memcpy(result.data(), bytes.data(), bytes.size());
    return result;
}

static inline std::string_view string_bytes(Value value)
{
    StringRef const& text = value.as_string()->str;
    return { text.data(), text.len() };
}

static inline std::string hex_string(u8 const* bytes, size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '\0');
    for (size_t i = 0; i < size; i++) {
        result[i * 2] = digits[bytes[i] >> 4];
        result[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    return result;
}

static inline std::string native_path(Value value)
{
    if (!value.is_string())
        return { };
    StringRef path = value.as_string()->str;
    return std::string(path.data(), path.len());
}

static inline std::string wildcard_regex(std::string const& pattern)
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

static void print_runtime_value_impl(Value v,
    std::unordered_set<ObjHeader*>& active_containers, u32 depth)
{
    if (v.is_nil()) {
        std::cout << "عدم";
        return;
    }

    if (v.is_bool()) {
        std::cout << (v.as_bool() ? "صحيح" : "خطا");
        return;
    }

    if (v.is_int()) {
        std::cout << integer::to_string(v);
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
        case ObjType::INT: {
            auto int_obj = reinterpret_cast<ObjBigInt*>(obj);
            std::cout << integer::to_string(Value::from_obj(&int_obj->obj));
            return;
        }

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

static inline void print_runtime_value(Value v)
{
    std::unordered_set<ObjHeader*> active_containers;
    print_runtime_value_impl(v, active_containers, 0);
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
        out += StringRef(integer::to_string(v).c_str());
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

static inline StringRef value_to_string(Value v)
{
    StringRef out = "";
    std::unordered_set<ObjHeader*> active_containers;
    append_rendered_value(out, v, false, active_containers, 0);
    return out;
}

}
