//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Implements URL parsing and encoding primitives.
//===----------------------------------------------------------------------===//

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include "util.hpp"
#include <string>

namespace fairuz::runtime {

VM::RuntimeResult VM::url_encode(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
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

VM::RuntimeResult VM::url_decode(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
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
                return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
            output.push_back(static_cast<char>((high << 4) | low));
            i += 2;
        } else if (input[i] == '%') {
            return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
        } else {
            output.push_back(input[i]);
        }
    }
    return m_gc.make_string(byte_string(output));
}

VM::RuntimeResult VM::url_parse(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
    static std::regex const pattern(
        R"(^([A-Za-z][A-Za-z0-9+.-]*)://(\[[^\]]+\]|[^/?#:]*)(?::([0-9]+))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?$)");
    std::string input(string_bytes(argv[0]));
    std::smatch match;
    if (!std::regex_match(input, match, pattern) || match[2].str().empty())
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);

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
            return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
        port = Value::from_int(parsed_port, m_gc);
    }
    dict_put(&result, m_gc.make_string("port"), port);
    dict_put(&result, m_gc.make_string("path"), m_gc.make_string(match[4].str().c_str()));
    dict_put(&result, m_gc.make_string("query"), m_gc.make_string(match[5].str().c_str()));
    dict_put(&result, m_gc.make_string("fragment"), m_gc.make_string(match[6].str().c_str()));
    return result;
}

VM::RuntimeResult VM::url_build(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_dict())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
    auto field = [&](char const* name) { return dict_get(&argv[0], m_gc.make_string(name)); };
    Value scheme = field("scheme");
    Value host = field("host");
    Value port = field("port");
    Value path = field("path");
    Value query = field("query");
    Value fragment = field("fragment");
    if (!scheme.is_string() || !host.is_string())
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
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
