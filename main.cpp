#include "include/ast_printer.hpp"
#include "include/compiler.hpp"
#include "include/diagnostic.hpp"
#include "include/lexer.hpp"
#include "include/parser.hpp"
#include "include/vm.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

#ifndef MYLANG_VERSION
#define MYLANG_VERSION "0.0.0"
#endif

constexpr char const* kVersion = MYLANG_VERSION;

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
    std::string input_path;
};

void printUsage(std::ostream& out, std::string_view program)
{
    out << "Usage: " << program << " <file> [options]\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help           Show this help message\n"
        << "  -V, --version        Show the language version\n"
        << "  --dump-ast           Print the parsed AST\n"
        << "  --dump-bytecode      Print compiled bytecode\n"
        << "  --time               Print execution time to stderr\n"
        << "  --check              Parse and compile only, do not execute\n"
        << "\n"
        << "Options may appear before or after <file>.\n";
}

bool parseArgs(int argc, char** argv, Options& options)
{
    if (argc <= 1) {
        options.show_help = true;
        return true;
    }

    for (int i = 1; i < argc; ++i) {
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

    return true;
}

void printAst(mylang::Array<mylang::ast::Stmt*> const& statements)
{
    mylang::ast::ASTPrinter printer(true);
    for (uint32_t i = 0; i < statements.size(); ++i)
        printer.print(statements[i]);
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parseArgs(argc, argv, options)) {
        printUsage(std::cerr, argc > 0 ? argv[0] : "mylang");
        return static_cast<int>(ExitCode::Usage);
    }

    if (options.show_help) {
        printUsage(std::cout, argc > 0 ? argv[0] : "mylang");
        return static_cast<int>(ExitCode::Success);
    }

    if (options.show_version) {
        std::cout << "mylang " << kVersion << "\n";
        return static_cast<int>(ExitCode::Success);
    }

    if (options.input_path.empty()) {
        std::cerr << "No input file provided\n";
        printUsage(std::cerr, argc > 0 ? argv[0] : "mylang");
        return static_cast<int>(ExitCode::Usage);
    }

    if (!std::filesystem::exists(options.input_path)) {
        std::cerr << "Input file not found: " << options.input_path << "\n";
        return static_cast<int>(ExitCode::NoInput);
    }

    try {
        mylang::diagnostic::reset();

        mylang::AllocatorContext allocator_context;
        mylang::setContext(&allocator_context);

        mylang::lex::FileManager file_manager(options.input_path);
        mylang::parser::Parser parser(&file_manager);
        mylang::Array<mylang::ast::Stmt*> statements = parser.parseProgram();

        if (mylang::diagnostic::hasErrors())
            return static_cast<int>(ExitCode::DataError);

        if (options.dump_ast)
            printAst(statements);

        mylang::runtime::Compiler compiler;
        mylang::runtime::Chunk* chunk = compiler.compile(statements);
        if (!chunk) {
            std::cerr << "Compilation failed: no bytecode was produced\n";
            return static_cast<int>(ExitCode::Software);
        }

        if (options.dump_bytecode)
            chunk->disassemble();

        if (options.check_only)
            return static_cast<int>(ExitCode::Success);

        mylang::runtime::VM vm;
        auto const start = std::chrono::steady_clock::now();
        vm.run(chunk);
        auto const end = std::chrono::steady_clock::now();

        if (options.print_time) {
            std::chrono::duration<double> elapsed = end - start;
            std::cerr << "time: " << elapsed.count() << "s\n";
        }

        return static_cast<int>(ExitCode::Success);
    } catch (std::exception const& ex) {
        std::cerr << "fatal: " << ex.what() << "\n";
        return static_cast<int>(ExitCode::Software);
    } catch (...) {
        std::cerr << "fatal: unknown exception\n";
        return static_cast<int>(ExitCode::Software);
    }
}
