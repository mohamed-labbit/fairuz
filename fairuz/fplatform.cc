#include "fplatform.hpp"
#include "fwindows.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifndef _WIN32
#    include <fcntl.h>
#    include <sys/stat.h>
#    include <unistd.h>
#endif

namespace fairuz::platform {

FILE* open_file(std::string_view filename, char const* mode)
{
#ifdef _WIN32
    std::wstring wide_mode(mode, mode + std::strlen(mode));
    return _wfopen(path(filename).c_str(), wide_mode.c_str());
#else
    return std::fopen(std::string(filename).c_str(), mode);
#endif
}

std::string environment(char const* name)
{
#ifdef _WIN32
    std::wstring wide_name(name, name + std::strlen(name));
    auto value = _wgetenv(wide_name.c_str());
    return value ? utf8(std::filesystem::path(value)) : std::string();
#else
    auto value = std::getenv(name);
    return value ? value : "";
#endif
}

#ifndef _WIN32
bool write_file_atomic(std::string const& path, char const* data, size_t len, std::string& error_out)
{
    std::filesystem::path target(path);
    struct stat original { };
    if (::lstat(path.c_str(), &original) != 0) {
        error_out = "Failed to inspect input file: " + std::string(std::strerror(errno));
        return false;
    }
    if (S_ISLNK(original.st_mode)) {
        error_out = "Refusing to format a symbolic link";
        return false;
    }
    if (!S_ISREG(original.st_mode)) {
        error_out = "Refusing to format a non-regular file";
        return false;
    }
    if (data == nullptr && len != 0) {
        error_out = "No formatted data was provided";
        return false;
    }

    std::filesystem::path parent = target.parent_path();
    if (parent.empty())
        parent = ".";
    std::string template_path = (parent / (target.filename().string() + ".fairuz-fmt-XXXXXX")).string();
    std::vector<char> writable_template(template_path.begin(), template_path.end());
    writable_template.push_back('\0');

    int fd = ::mkstemp(writable_template.data());
    if (fd < 0) {
        error_out = "Failed to create a temporary file for formatting: "
            + std::string(std::strerror(errno));
        return false;
    }

    std::string tmp_path(writable_template.data());
    auto fail = [&](std::string const& prefix) {
        int saved_errno = errno;
        if (fd >= 0)
            ::close(fd);
        ::unlink(tmp_path.c_str());
        error_out = prefix + ": " + std::string(std::strerror(saved_errno));
        return false;
    };

    if (::fchmod(fd, original.st_mode & 07777) != 0)
        return fail("Failed to preserve input file permissions");

    size_t written = 0;
    while (written < len) {
        ssize_t result = ::write(fd, data + written, len - written);
        if (result < 0) {
            if (errno == EINTR)
                continue;
            return fail("Failed to write formatted output");
        }
        if (result == 0) {
            errno = EIO;
            return fail("Failed to write formatted output");
        }
        written += static_cast<size_t>(result);
    }

    if (::fsync(fd) != 0)
        return fail("Failed to flush formatted output");
    if (::close(fd) != 0) {
        fd = -1;
        return fail("Failed to close formatted output");
    }
    fd = -1;

    // Do not replace a file that changed while it was being formatted.
    struct stat current { };
    if (::lstat(path.c_str(), &current) != 0)
        return fail("Failed to re-inspect input file");
    if (!S_ISREG(current.st_mode) || current.st_dev != original.st_dev
        || current.st_ino != original.st_ino) {
        errno = EBUSY;
        return fail("Input file changed while formatting");
    }

    if (::rename(tmp_path.c_str(), path.c_str()) != 0)
        return fail("Failed to replace the original file");

    // Persist the directory entry where the platform supports directory
    // fsync. The file itself has already been atomically replaced.
    int directory_flags = O_RDONLY;
#    ifdef O_DIRECTORY
    directory_flags |= O_DIRECTORY;
#    endif
    int directory_fd = ::open(parent.c_str(), directory_flags);
    if (directory_fd >= 0) {
        (void)::fsync(directory_fd);
        (void)::close(directory_fd);
    }

    return true;
}

#else
bool write_file_atomic(std::string const& filename, char const* data, size_t len, std::string& error)
{
    auto target = path(filename);
    WinHandle original(CreateFileW(target.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION before { };
    if (!original || !GetFileInformationByHandle(original.get(), &before)) {
        error = windows_error("Failed to inspect input file").what();
        return false;
    }
    if (before.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) {
        error = "Refusing to format a symbolic link or non-regular file";
        return false;
    }
    if (!data && len) {
        error = "No formatted data was provided";
        return false;
    }
    static std::atomic<unsigned long long> counter { 0 };
    std::filesystem::path temporary;
    HANDLE created = INVALID_HANDLE_VALUE;
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
        temporary = target;
        temporary += L".fairuz-fmt-" + std::to_wstring(GetCurrentProcessId())
            + L"-" + std::to_wstring(++counter);
        created = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (created != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS)
            break;
    }
    WinHandle output(created);
    if (!output) {
        error = windows_error("Failed to create formatting temporary file").what();
        return false;
    }
    auto fail = [&](char const* operation) {
        error = windows_error(operation).what();
        output.close();
        DeleteFileW(temporary.c_str());
        return false;
    };
    size_t written = 0;
    while (written < len) {
        DWORD count = 0;
        DWORD chunk = static_cast<DWORD>(std::min<size_t>(len - written, 0x7fffffff));
        if (!WriteFile(output.get(), data + written, chunk, &count, nullptr) || count == 0)
            return fail("Failed to write formatted output");
        written += count;
    }
    if (!FlushFileBuffers(output.get()))
        return fail("Failed to flush formatted output");
    output.close();
    WinHandle current(CreateFileW(target.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION after { };
    if (!current || !GetFileInformationByHandle(current.get(), &after))
        return fail("Failed to re-inspect input file");
    if ((after.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))
        || before.dwVolumeSerialNumber != after.dwVolumeSerialNumber
        || before.nFileIndexHigh != after.nFileIndexHigh || before.nFileIndexLow != after.nFileIndexLow) {
        SetLastError(ERROR_FILE_INVALID);
        return fail("Input file changed while formatting");
    }
    // ReplaceFile preserves the original file's ACLs and other metadata.
    if (!ReplaceFileW(target.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr))
        return fail("Failed to replace the original file");
    return true;
}
#endif

} // namespace fairuz::platform
