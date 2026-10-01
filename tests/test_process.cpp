#include "test_process.hpp"
#include "fwindows.hpp"

#include <atomic>
#include <cerrno>
#include <cwchar>
#include <fstream>
#include <map>
#include <random>
#include <system_error>
#include <thread>

#ifndef _WIN32
#    include <fcntl.h>
#    include <signal.h>
#    include <sys/wait.h>
#    include <unistd.h>
extern char** environ;
#endif

namespace test_process {

unsigned long process_id()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(getpid());
#endif
}

std::filesystem::path temporary_directory()
{
    static std::atomic<unsigned long long> counter { 0 };
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto path = std::filesystem::temp_directory_path() / ("fairuz-test-" + std::to_string(process_id()) + "-" + std::to_string(seed) + "-" + std::to_string(++counter));
        std::error_code error;
        if (std::filesystem::create_directory(path, error))
            return path;
        if (error && error != std::errc::file_exists)
            throw std::system_error(error, "create temporary directory");
    }
    throw std::runtime_error("Could not create a unique temporary directory");
}

namespace {

struct TemporaryDirectory {
    std::filesystem::path path = temporary_directory();
    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
std::string read(std::filesystem::path const& path)
{
    std::ifstream file(path, std::ios::binary);
    return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
}
#ifdef _WIN32
std::wstring quote(std::wstring const& arg)
{
    // Follow the Windows CRT command-line rules, including trailing slashes
    // and literal quotes. No shell is involved.
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t ch : arg) {
        if (ch == L'\\') {
            ++slashes;
            continue;
        }
        result.append(ch == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        result += ch;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
struct EnvironmentOrder {
    bool operator()(std::wstring const& lhs, std::wstring const& rhs) const
    {
        return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_LESS_THAN;
    }
};
#endif

}

Result run(std::filesystem::path const& binary, std::vector<std::string> const& args,
    std::filesystem::path const& cwd, std::chrono::seconds timeout)
{
    TemporaryDirectory capture;
    auto output = capture.path / "stdout";
    auto errors = capture.path / "stderr";
    Result result;
#ifdef _WIN32
    using fairuz::platform::windows_error;
    using fairuz::platform::WinHandle;
    SECURITY_ATTRIBUTES security { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    WinHandle out(CreateFileW(output.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    WinHandle err(CreateFileW(errors.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
        &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    WinHandle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!out || !err || !input)
        throw windows_error("create child descriptors");
    std::wstring command = quote(binary.native());
    for (auto const& arg : args)
        command += L" " + quote(fairuz::platform::path(arg).native());
    std::map<std::wstring, std::wstring, EnvironmentOrder> variables;
    wchar_t* inherited = GetEnvironmentStringsW();
    if (!inherited)
        throw windows_error("read environment");
    for (auto entry = inherited; *entry; entry += std::wcslen(entry) + 1) {
        std::wstring item(entry);
        auto equals = item.find(L'=', 1); // Preserve drive current-directory entries.
        if (equals != std::wstring::npos)
            variables[item.substr(0, equals)] = item.substr(equals + 1);
    }
    FreeEnvironmentStringsW(inherited);
    variables[L"ASAN_OPTIONS"] = L"detect_leaks=0";
    variables[L"NO_COLOR"] = L"1";
    variables[L"FAIRUZ_STDLIB"] = fairuz::platform::path(FAIRUZ_TEST_STDLIB_DIR).native();
    std::vector<wchar_t> environment;
    for (auto const& [key, value] : variables) {
        auto item = key + L"=" + value;
        environment.insert(environment.end(), item.begin(), item.end());
        environment.push_back(0);
    }
    environment.push_back(0);
    STARTUPINFOW startup { };
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.get();
    startup.hStdOutput = out.get();
    startup.hStdError = err.get();
    PROCESS_INFORMATION process { };
    if (!CreateProcessW(binary.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_UNICODE_ENVIRONMENT, environment.data(), cwd.empty() ? nullptr : cwd.c_str(),
            &startup, &process))
        throw windows_error("start interpreter");
    WinHandle child(process.hProcess), thread(process.hThread);
    auto wait = WaitForSingleObject(child.get(), static_cast<DWORD>(timeout.count() * 1000));
    if (wait != WAIT_OBJECT_0) {
        result.timed_out = wait == WAIT_TIMEOUT;
        TerminateProcess(child.get(), 124);
        WaitForSingleObject(child.get(), INFINITE);
        if (!result.timed_out)
            throw windows_error("wait for interpreter");
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(child.get(), &code))
        throw windows_error("read exit code");
    result.crashed = code >= 0xc0000000UL;
    result.exit_code = static_cast<int>(code);
    out.close();
    err.close();
#else
    std::vector<std::string> arguments { binary.string() };
    arguments.insert(arguments.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& arg : arguments)
        argv.push_back(arg.data());
    argv.push_back(nullptr);
    std::map<std::string, std::string> variables;
    for (auto entry = environ; *entry; ++entry) {
        std::string item(*entry);
        auto equals = item.find('=');
        if (equals != std::string::npos)
            variables[item.substr(0, equals)] = item.substr(equals + 1);
    }
    variables["ASAN_OPTIONS"] = "detect_leaks=0";
    variables["NO_COLOR"] = "1";
    variables["FAIRUZ_STDLIB"] = FAIRUZ_TEST_STDLIB_DIR;
    std::vector<std::string> environment;
    for (auto const& [key, value] : variables)
        environment.push_back(key + "=" + value);
    std::vector<char*> envp;
    for (auto& entry : environment)
        envp.push_back(entry.data());
    envp.push_back(nullptr);
    pid_t child = fork();
    if (child < 0)
        throw std::system_error(errno, std::generic_category(), "fork");
    if (child == 0) {
        int input = open("/dev/null", O_RDONLY);
        int out = open(output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int err = open(errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (input < 0 || out < 0 || err < 0 || dup2(input, 0) < 0
            || dup2(out, 1) < 0 || dup2(err, 2) < 0 || (!cwd.empty() && chdir(cwd.c_str()) != 0))
            _exit(126);
        if (input > 2)
            close(input);
        if (out > 2)
            close(out);
        if (err > 2)
            close(err);
        execve(binary.c_str(), argv.data(), envp.data());
        _exit(127);
    }
    int status = 0;
    auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            break;
        if (waited < 0 && errno != EINTR) {
            int error = errno;
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            throw std::system_error(error, std::generic_category(), "waitpid");
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.timed_out = true;
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) { }
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    result.crashed = !WIFEXITED(status);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
    result.out = read(output);
    result.err = read(errors);
    return result;
}

} // namespace test_process
