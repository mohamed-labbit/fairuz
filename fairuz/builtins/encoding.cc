#include "../futf8.hpp"
#include "../fvm.hpp"
#include "util.hpp"

namespace fairuz::runtime {

namespace {

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

} // anonymous namespace

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

} // namespace fairuz::runtime
