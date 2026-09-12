#include "fairuz/fAST_printer.hpp"
#include "fairuz/fcompiler.hpp"
#include "fairuz/fdiagnostic.hpp"
#include "fairuz/fformatter.hpp"
#include "fairuz/flexer.hpp"
#include "fairuz/fparser.hpp"
#include "fairuz/fvm.hpp"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

#ifndef fairuz_VERSION
#    define fairuz_VERSION "0.1.0"
#endif

constexpr char const* kVersion = fairuz_VERSION;

enum class ExitCode : int {
    Success = 0,
    Usage = 64,
    DataError = 65,
    NoInput = 66,
    Software = 70,
};

struct Options {
    bool dump_ast { false };
    bool dump_bytecode { false };
    bool print_time { false };
    bool check_only { false };
    bool show_help { false };
    bool show_version { false };
    bool format_file { false };
    std::string input_path;
};

void printUsage(std::ostream& out, std::string_view program)
{
    out << "Usage: " << program << " <file> [options]\n"
        << "       " << program << " format <file>\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help           Show this help message\n"
        << "  -V, --version        Show the language version\n"
        << "  --dump-ast           Print the parsed AST\n"
        << "  --dump-bytecode      Print compiled bytecode\n"
        << "  --time               Print execution time to stderr\n"
        << "  --check              Parse and compile only, do not execute\n"
        << "  format               Rewrite the input file with canonical formatting\n"
        << "\n"
        << "Options may appear before or after <file>.\n";
}

bool parseArgs(int argc, char** argv, Options& options)
{
    if (argc <= 1) {
        options.show_help = true;
        return true;
    }

    for (int i = 1; i < argc; i++) {
        std::string_view arg(argv[i]);

        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
            continue;
        }
        if (arg == "-V" || arg == "--version") {
            options.show_version = true;
            continue;
        }
        if (arg == "--dump-ast") {
            options.dump_ast = true;
            continue;
        }
        if (arg == "--dump-bytecode") {
            options.dump_bytecode = true;
            continue;
        }
        if (arg == "--time") {
            options.print_time = true;
            continue;
        }
        if (arg == "--check") {
            options.check_only = true;
            continue;
        }
        if (arg == "format") {
            options.format_file = true;
            continue;
        }
        if (!arg.empty() && arg.front() == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
        if (!options.input_path.empty()) {
            std::cerr << "Only one input file is supported\n";
            return false;
        }
        options.input_path = std::string(arg);
    }

    if (options.format_file && (options.dump_ast || options.dump_bytecode || options.print_time || options.check_only)) {
        std::cerr << "format cannot be combined with --dump-ast, --dump-bytecode, --time, or --check\n";
        return false;
    }

    return true;
}

void printAst(fairuz::Fa_Array<fairuz::AST::Fa_Stmt*> const& stmts)
{
    fairuz::AST::ASTPrinter printer(true);
    for (u32 i = 0; i < stmts.size(); i++)
        printer.print(stmts[i]);
}

bool writeFileAtomic(std::string const& path, char const* data, size_t len, std::string& error_out)
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
    std::string template_path =
        (parent / (target.filename().string() + ".fairuz-fmt-XXXXXX")).string();
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
#ifdef O_DIRECTORY
    directory_flags |= O_DIRECTORY;
#endif
    int directory_fd = ::open(parent.c_str(), directory_flags);
    if (directory_fd >= 0) {
        (void)::fsync(directory_fd);
        (void)::close(directory_fd);
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArgs(argc, argv, options)) {
        printUsage(std::cerr, argc > 0 ? argv[0] : "fairuz");
        return static_cast<int>(ExitCode::Usage);
    }

    if (options.show_help) {
        printUsage(std::cout, argc > 0 ? argv[0] : "fairuz");
        return static_cast<int>(ExitCode::Success);
    }

    if (options.show_version) {
        std::cout << "fairuz " << kVersion << "\n";
        return static_cast<int>(ExitCode::Success);
    }

    if (options.input_path.empty()) {
        std::cerr << "No input file provided\n";
        printUsage(std::cerr, argc > 0 ? argv[0] : "fairuz");
        return static_cast<int>(ExitCode::Usage);
    }

    if (!std::filesystem::exists(options.input_path)) {
        std::cerr << "Input file not found: " << options.input_path << "\n";
        return static_cast<int>(ExitCode::NoInput);
    }

    try {
        fairuz::diagnostic::reset();

        fairuz::Fa_AllocatorContext allocator_context;
        fairuz::set_context(&allocator_context);
        fairuz::lex::Fa_FileManager fm(options.input_path);
        fairuz::diagnostic::set_source(&fm);
        fairuz::parser::Fa_Parser parser(&fm);
        fairuz::Fa_Array<fairuz::AST::Fa_Stmt*> stmts = parser.parse_program();

        if (fairuz::diagnostic::has_errors()) {
            fairuz::diagnostic::dump();
            return static_cast<int>(ExitCode::DataError);
        }

        if (options.format_file) {
            fairuz::Fa_Formatter fmter;
            fairuz::Fa_StringRef fmted = fmter.format(stmts);
            char const* data = fmted.empty() ? "" : fmted.data();
            std::string error;
            if (!writeFileAtomic(options.input_path, data, fmted.len(), error)) {
                std::cerr << error << "\n";
                return static_cast<int>(ExitCode::Software);
            }
            return static_cast<int>(ExitCode::Success);
        }

        if (options.dump_ast)
            printAst(stmts);

        fairuz::runtime::Compiler compiler;
        fairuz::runtime::Fa_Chunk* chunk = compiler.compile(stmts);
        if (!chunk) {
            std::cerr << "Compilation failed: no bytecode was produced\n";
            return static_cast<int>(ExitCode::Software);
        }
        if (fairuz::diagnostic::has_errors())
            return static_cast<int>(ExitCode::DataError);
        chunk->source_path = options.input_path;

        if (options.dump_bytecode)
            chunk->disassemble();

        if (options.check_only)
            return static_cast<int>(ExitCode::Success);

        fairuz::runtime::Fa_VM vm;
        auto const start = std::chrono::steady_clock::now();
        vm.run(chunk);
        auto const end = std::chrono::steady_clock::now();

        if (options.print_time) {
            std::chrono::duration<f64> elapsed = end - start;
            std::cerr << "time: " << elapsed.count() << "s\n";
        }

        return static_cast<int>(ExitCode::Success);
    } catch (fairuz::runtime::Fa_RuntimeHalt const&) {
        return static_cast<int>(ExitCode::DataError);
    } catch (fairuz::diagnostic::Fa_DiagnosticAbort const&) {
        return static_cast<int>(ExitCode::DataError);
    } catch (std::exception const& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return static_cast<int>(ExitCode::Software);
    } catch (...) {
        std::cerr << "fatal: unknown exception\n";
        return static_cast<int>(ExitCode::Software);
    }
}
