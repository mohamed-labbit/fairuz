#include "fairuz/fAST_printer.hpp"
#include "fairuz/fcompiler.hpp"
#include "fairuz/fdiagnostic.hpp"
#include "fairuz/fformatter.hpp"
#include "fairuz/flexer.hpp"
#include "fairuz/fparser.hpp"
#include "fairuz/fvm.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

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

    for (int i = 1; i < argc; i += 1) {
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
    for (u32 i = 0; i < stmts.size(); i += 1)
        printer.print(stmts[i]);
}

bool writeFileAtomic(std::string const& path, char const* data, std::streamsize len, std::string& error_out)
{
    std::filesystem::path target(path);
    std::filesystem::path tmp = target;
    tmp += ".fairuz-fmt-tmp";

    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file) {
            error_out = "Failed to open temporary file for formatting: " + tmp.string();
            return false;
        }
        file.write(data, len);
        if (!file) {
            error_out = "Failed to write formatted output to: " + tmp.string();
            file.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        error_out = "Failed to replace original file with formatted output: " + ec.message();
        std::filesystem::remove(tmp, ec);
        return false;
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

        if (options.format_file) {
            fairuz::Fa_Formatter fmter;
            fairuz::Fa_StringRef fmted = fmter.format(stmts);
            char const* data = fmted.empty() ? "" : fmted.data();
            std::string error;
            if (!writeFileAtomic(options.input_path, data, static_cast<std::streamsize>(fmted.len()), error)) {
                std::cerr << error << "\n";
                return static_cast<int>(ExitCode::Software);
            }
            return static_cast<int>(ExitCode::Success);
        }

        if (fairuz::diagnostic::has_errors())
            return static_cast<int>(ExitCode::DataError);

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
    } catch (std::exception const& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return static_cast<int>(ExitCode::Software);
    } catch (...) {
        std::cerr << "fatal: unknown exception\n";
        return static_cast<int>(ExitCode::Software);
    }
}
