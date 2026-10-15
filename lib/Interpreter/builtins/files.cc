//===----------------------------------------------------------------------===//
//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Builtin file manipulation utilities implementation.
//===----------------------------------------------------------------------===//

#include "Diag/errc.hpp"
#include "Interpreter/vm.hpp"
#include "Platform.hpp"
#include "util.hpp"

#include <fstream>
#include <iostream>

namespace fairuz::runtime {

namespace {

std::filesystem::path temporary_parent(Value value)
{
    if (value.is_nil())
        return std::filesystem::temp_directory_path();
    std::string path = native_path(value);
    return path.empty() ? std::filesystem::temp_directory_path() : platform::path(path);
}

std::filesystem::path unique_temporary_path(std::filesystem::path const& parent,
    std::string const& prefix, std::string const& suffix)
{
    static std::atomic<unsigned long long> counter { 0 };
    auto seed = static_cast<unsigned long long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    for (unsigned attempt = 0; attempt < 1000; ++attempt) {
        auto id = seed ^ (++counter * 0x9E3779B97F4A7C15ULL) ^ attempt;
        auto candidate = parent / platform::path(prefix + std::to_string(id) + suffix);
        if (!std::filesystem::exists(candidate))
            return candidate;
    }
    return { };
}

} // anonymous namespace

VM::RuntimeResult VM::input(int /*argc*/, Value* /*argv*/) // input takes no args for now
{
    // read until user hits ENTER
    StringRef ret_str = "";
    std::string help = ""; // getline only accepts std::string

    if (!std::getline(std::cin, help))
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);

    ret_str = help.data();
    Value ret = m_gc.make_string(ret_str);
    return ret;
}

VM::RuntimeResult VM::file_read_all(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_file_handle())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "read all expects an open file handle");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    std::string output;
    char buffer[8192];
    for (;;) {
        size_t count = std::fread(buffer, 1, sizeof(buffer), handle->fp);
        output.append(buffer, count);
        if (count < sizeof(buffer))
            break;
    }
    if (std::ferror(handle->fp))
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    return m_gc.make_string(byte_string(output));
}

VM::RuntimeResult VM::file_read_line(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_file_handle())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "read line expects an open file handle");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    std::string output;
    int ch = 0;
    while ((ch = std::fgetc(handle->fp)) != EOF) {
        if (ch == '\n')
            break;
        output.push_back(static_cast<char>(ch));
    }
    if (ch == EOF && output.empty()) {
        if (std::ferror(handle->fp))
            return raise_runtime_error(ErrorCode::READ_ERROR);
        return Value::nil();
    }
    if (!output.empty() && output.back() == '\r')
        output.pop_back();
    return m_gc.make_string(byte_string(output));
}

VM::RuntimeResult VM::file_write(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_file_handle() || !argv[1].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file write expects an open handle and string");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return Value::from_bool(false);
    StringRef text = argv[1].as_string()->str;
    size_t written = text.empty() ? 0 : std::fwrite(text.data(), 1, text.len(), handle->fp);
    return Value::from_bool(written == text.len());
}

VM::RuntimeResult VM::file_flush(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_file_handle())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file flush expects an open handle");
    ObjFileHandle* handle = argv[0].as_file_handle();
    return Value::from_bool(handle->is_open && handle->fp != nullptr
        && std::fflush(handle->fp) == 0);
}

VM::RuntimeResult VM::file_open(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file open expects path and mode strings");
    StringRef path = argv[0].as_string()->str;
    StringRef mode = argv[1].as_string()->str;
    if (::memchr(path.data(), '\0', path.len()) != nullptr)
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    char const* native_mode = nullptr;
    if (mode == "قراءة" || mode == "اقرا")
        native_mode = "rb";
    else if (mode == "كتابة" || mode == "اكتب")
        native_mode = "wb";
    else if (mode == "اضافة" || mode == "اضف")
        native_mode = "ab+";
    else
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    FILE* file = platform::open_file(std::string_view(path.data(), path.len()), native_mode);
    return file == nullptr ? Value::nil() : m_gc.make_file_handle(file);
}

VM::RuntimeResult VM::file_read(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_file_handle() || !argv[1].is_int()
        || argv[1].as_int() < 0)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file read expects an open handle and non-negative byte count");
    ObjFileHandle* handle = argv[0].as_file_handle();
    if (!handle->is_open || handle->fp == nullptr)
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    size_t requested = static_cast<size_t>(argv[1].as_int());
    std::string output(requested, '\0');
    size_t count = requested == 0 ? 0 : std::fread(output.data(), 1, requested, handle->fp);
    output.resize(count);
    return m_gc.make_string(byte_string(output));
}

VM::RuntimeResult VM::path_delete(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR, "path delete expects a string");
    std::error_code error;
    auto count = std::filesystem::remove_all(platform::path(native_path(argv[0])), error);
    return Value::from_bool(!error && count > 0);
}

VM::RuntimeResult VM::path_glob(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string() || !argv[1].is_bool())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "glob expects a pattern and recursive flag");
    std::filesystem::path pattern(platform::path(native_path(argv[0])));
    std::filesystem::path parent = pattern.parent_path();
    if (parent.empty())
        parent = ".";
    std::regex matcher(wildcard_regex(platform::utf8(pattern.filename())));
    Value result = m_gc.make_list();
    std::error_code error;
    if (argv[1].as_bool()) {
        for (std::filesystem::recursive_directory_iterator it(parent, error), end; !error && it != end; it.increment(error)) {
            if (std::regex_match(platform::utf8(it->path().filename()), matcher))
                result.as_list()->elements.push(m_gc.make_string(platform::utf8(it->path()).c_str()));
        }
    } else {
        for (std::filesystem::directory_iterator it(parent, error), end; !error && it != end; it.increment(error)) {
            if (std::regex_match(platform::utf8(it->path().filename()), matcher))
                result.as_list()->elements.push(m_gc.make_string(platform::utf8(it->path()).c_str()));
        }
    }
    return result;
}

VM::RuntimeResult VM::temp_file(int argc, Value* argv)
{
    if (argc != 3 || argv == nullptr || !argv[0].is_string() || !argv[1].is_string()
        || !(argv[2].is_nil() || argv[2].is_string()))
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "temporary file expects prefix, suffix, and optional directory");
    std::filesystem::path path = unique_temporary_path(temporary_parent(argv[2]),
        native_path(argv[0]), native_path(argv[1]));
    if (path.empty())
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    std::ofstream created(path, std::ios::binary | std::ios::trunc);
    if (!created)
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    created.close();
    Value result = m_gc.make_dict();
    result.as_dict()->set(m_gc.make_string("path"), m_gc.make_string(platform::utf8(path).c_str()));
    return result;
}

VM::RuntimeResult VM::temp_directory(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr || !argv[0].is_string()
        || !(argv[1].is_nil() || argv[1].is_string()))
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "temporary directory expects prefix and optional parent");
    std::filesystem::path path = unique_temporary_path(temporary_parent(argv[1]),
        native_path(argv[0]), "");
    std::error_code error;
    if (path.empty() || !std::filesystem::create_directory(path, error) || error)
        return raise_runtime_error(ErrorCode::RUNTIME_ERROR);
    return m_gc.make_string(platform::utf8(path).c_str());
}

VM::RuntimeResult VM::remove_tree(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr || !argv[0].is_string())
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "tree removal expects a path string");
    std::error_code error;
    std::filesystem::remove_all(platform::path(native_path(argv[0])), error);
    return Value::from_bool(!error);
}

VM::RuntimeResult VM::open(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::OPEN_ARG_COUNT);

    if (UNLIKELY(!argv[0].is_string() || !argv[1].is_string()))
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "open expects (string, string)");

    StringRef const& filename_arg = argv[0].as_string()->str;
    if (::memchr(filename_arg.data(), '\0', filename_arg.len()) != nullptr)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR,
            "file path contains a NUL byte");

    char const* filename = filename_arg.data();
    StringRef mode_arg = argv[1].as_string()->str;

    StringRef fmode;
    if (mode_arg == "اضف")
        fmode = "a";
    else if (mode_arg == "اقرا")
        fmode = "r";
    else if (mode_arg == "اكتب")
        fmode = "w";
    else
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);

    FILE* fp = platform::open_file(filename, fmode.data());
    if (fp == NULL)
        return raise_runtime_error(ErrorCode::NATIVE_TYPE_ERROR);

    return m_gc.make_file_handle(fp);
}

VM::RuntimeResult VM::append_file(int argc, Value* argv)
{
    if (argc != 2 || argv == nullptr)
        return raise_runtime_error(ErrorCode::APPEND_FILE_ARG_COUNT);

    Value& file = argv[0];
    Value& content = argv[1];

    if (!file.is_file_handle())
        return raise_runtime_error(ErrorCode::APPEND_FILE_TYPE_ERROR);

    if (!content.is_string())
        return raise_runtime_error(ErrorCode::APPEND_FILE_TYPE_ERROR);

    ObjFileHandle* file_handle = file.as_file_handle();
    if (!file_handle->is_open || file_handle->fp == nullptr)
        return raise_runtime_error(ErrorCode::APPEND_FILE_TYPE_ERROR,
            "file handle is closed");

    ObjString* str_obj = content.as_string();
    FILE* fp = file_handle->fp;
    StringRef content_str = str_obj->str;

    if (content_str.empty())
        return Value::from_bool(true); // nothing to write is trivially successful

    size_t const written = std::fwrite(content_str.data(), 1, content_str.len(), fp);
    // ::fflush(fp);

    if (written != content_str.len())
        return raise_runtime_error(ErrorCode::APPEND_FILE_FAILED,
            std::strerror(errno));

    return Value::from_bool(true);
}

VM::RuntimeResult VM::close(int argc, Value* argv)
{
    if (argc != 1 || argv == nullptr)
        return raise_runtime_error(ErrorCode::CLOSE_ARG_COUNT);

    if (!argv[0].is_file_handle())
        return raise_runtime_error(ErrorCode::CLOSE_TYPE_ERROR);

    ObjFileHandle* file_handle = argv[0].as_file_handle();
    if (file_handle->close())
        return Value::from_bool(true);

    return raise_runtime_error(/**/ ErrorCode::CLOSE_TYPE_ERROR);
}

} // fairuz::runtime
