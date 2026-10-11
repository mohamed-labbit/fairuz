#ifndef FAIRUZ_WINDOWS_HPP
#define FAIRUZ_WINDOWS_HPP

#ifdef _WIN32
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <system_error>
#    include <windows.h>

namespace fairuz::platform {

class WinHandle {
    HANDLE value_;

public:
    explicit WinHandle(HANDLE value = INVALID_HANDLE_VALUE)
        : value_(value)
    {
    }
    ~WinHandle() { close(); }
    WinHandle(WinHandle const&) = delete;
    WinHandle& operator=(WinHandle const&) = delete;
    HANDLE get() const { return value_; }
    explicit operator bool() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
    void close()
    {
        if (*this)
            CloseHandle(value_);
        value_ = INVALID_HANDLE_VALUE;
    }
};
inline std::system_error windows_error(char const* operation)
{
    return std::system_error(static_cast<int>(GetLastError()), std::system_category(), operation);
}

}
#endif
#endif
