#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#endif

int probe(std::vector<std::string> const& args)
{
    if (args.size() == 1 && args[0] == "sleep") {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        return 0;
    }
    for (auto const& arg : args)
        std::cout << arg.size() << ':' << arg << '\n';
    std::cerr << "probe stderr\n";
    return 23;
}
#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        auto text = std::filesystem::path(argv[i]).u8string();
        args.emplace_back(reinterpret_cast<char const*>(text.data()), text.size());
    }
    return probe(args);
}
#else
int main(int argc, char** argv) { return probe({ argv + 1, argv + argc }); }
#endif
