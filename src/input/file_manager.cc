#include "../../include/input/file_manager.hpp"
#include "../../include/diag/diagnostic.hpp"
#include "../../include/input/error.hpp"

#include <filesystem>
#include <iostream>
#include <span>

namespace fs = std::filesystem;

namespace mylang {
namespace input {

FileManager::FileManager(std::string const& filepath)
    : FilePath_(filepath)
{
    std::ifstream in(filepath, std::ios::binary);
    if (!in)
        diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_OPEN));

    InputBuffer_ = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()).data();
    LastKnownWriteTime_ = fs::last_write_time(filepath);
}

StringRef FileManager::load(std::string const& filepath, bool replace)
{
    if (filepath.empty())
        return "";

    std::ifstream in(filepath, std::ios::binary);
    StringRef ret = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()).data();
    if (!in)
        diagnostic::engine.panic(toString(FileManagerError::FILE_NOT_OPEN));

    if (replace || FilePath_.empty()) {
        InputBuffer_ = ret;
        FilePath_ = filepath;
        LastKnownWriteTime_ = fs::last_write_time(filepath);
    }

    return ret;
}

}
}
