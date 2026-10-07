#include "../fvm.hpp"
#include "util.hpp"

namespace fairuz::runtime {

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
        port = Value::from_int(parsed_port, m_gc);
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

} // fairuz::runtime
