//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Builtin regular-expressions utilities implementation.
//===----------------------------------------------------------------------===//

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include "Support/error.hpp"
#include "util.hpp"

namespace fairuz::runtime {

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

} // anonymous namespace

VM::RuntimeResult VM::make_regex_result(std::string const& input, std::smatch const& match, size_t base_offset)
{
    Value result = m_gc.make_dict();
    size_t match_start = base_offset + static_cast<size_t>(match.position(0));
    size_t match_end = match_start + static_cast<size_t>(match.length(0));
    dict_put(&result, m_gc.make_string("start"),
        Value::from_int(utf8_character_offset(input, match_start), m_gc));
    dict_put(&result, m_gc.make_string("end"),
        Value::from_int(utf8_character_offset(input, match_end), m_gc));

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

VM::RuntimeResult VM::regex_compile(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));

    if (!argv[0].is_string() || !argv[1].is_int())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
    try {
        std::regex validation(std::string(string_bytes(argv[0])));
        (void)validation;
    } catch (std::regex_error const&) {
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
    return argv[0];
}

VM::RuntimeResult VM::regex_search(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string()
        || !argv[1].is_string() || !argv[2].is_int())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
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
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
}

VM::RuntimeResult VM::regex_match(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string()
        || !argv[1].is_string() || !argv[2].is_int())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
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
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
}

VM::RuntimeResult VM::regex_fullmatch(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string() || !argv[1].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::string input(string_bytes(argv[1]));
        std::smatch match;
        if (!std::regex_match(input, match, pattern))
            return Value::nil();
        return make_regex_result(input, match, 0);
    } catch (std::regex_error const&) {
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
}

VM::RuntimeResult VM::regex_findall(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string() || !argv[1].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
    try {
        std::regex pattern(std::string(string_bytes(argv[0])));
        std::string input(string_bytes(argv[1]));
        Value results = m_gc.make_list();
        for (std::sregex_iterator it(input.begin(), input.end(), pattern), end; it != end; ++it)
            results.as_list()->elements.push(make_regex_result(input, *it, 0).value());
        return results;
    } catch (std::regex_error const&) {
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
}

VM::RuntimeResult VM::regex_split(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string()
        || !argv[1].is_string() || !argv[2].is_int() || argv[2].as_int() < 0)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
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
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
}

VM::RuntimeResult VM::regex_replace(int argc, Value* argv)
{
    if (argc != 4 || argv == nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_ARG_COUNT,
            "got " + std::to_string(argc));
    if (!argv[0].is_string() || !argv[1].is_string()
        || !argv[2].is_string() || !argv[3].is_int() || argv[3].as_int() < 0)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);
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
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    }
}

} // fairuz::runtime
