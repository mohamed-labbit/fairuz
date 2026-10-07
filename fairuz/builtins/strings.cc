#include "../futf8.hpp"
#include "../fvm.hpp"
#include "util.hpp"

namespace fairuz::runtime {

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

} // namespace fairuz::runtime
