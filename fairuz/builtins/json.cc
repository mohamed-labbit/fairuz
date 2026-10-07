#include "../futf8.hpp"
#include "../fvm.hpp"
#include "util.hpp"

namespace fairuz::runtime {

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
    return m_gc.make_string(byte_string(output));
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
            Value decoded = m_gc.make_string(byte_string(output));
            result.as_list()->elements.push(decoded);
            result.as_list()->elements.push(Value::from_int(cp_pos + 1, m_gc));
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

} // fairuz::runtime
