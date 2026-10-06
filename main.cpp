#include "fAST.hpp"
#include "fairuz/fAST_printer.hpp"
#include "fairuz/fcompiler.hpp"
#include "fairuz/fdiagnostic.hpp"
#include "fairuz/fformatter.hpp"
#include "fairuz/flexer.hpp"
#include "fairuz/fparser.hpp"
#include "fairuz/fsyntax_highlighter.hpp"
#include "fairuz/fvm.hpp"

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "fairuz/fplatform.hpp"
#include "fairuz/fwindows.hpp"
#include <algorithm>
#ifdef _WIN32
#    include <fcntl.h>
#    include <io.h>
#endif

namespace {

#ifndef fairuz_VERSION
#    define fairuz_VERSION "0.1.0"
#endif

constexpr char const* kVersion = fairuz_VERSION;

enum class ExitCode : int {
    Success = 0,
    NeedsFormatting = 1,
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
    bool semantic_tokens { false };
    bool json_diagnostics { false };
    std::string input_path;
};

void printUsage(std::ostream& out, std::string_view program)
{
    out << "Usage: " << program << " <file> [options]\n"
        << "       " << program << " format [--check] <file-or-directory>\n"
        << "\n"
        << "Options:\n"
        << "  -h, --help           Show this help message\n"
        << "  -V, --version        Show the language version\n"
        << "  --dump-ast           Print the parsed AST\n"
        << "  --dump-bytecode      Print compiled bytecode\n"
        << "  --time               Print execution time to stderr\n"
        << "  --check              Parse and compile only, do not execute\n"
        << "  --diagnostics=json   Write structured diagnostics to stderr\n"
        << "  --semantic-tokens    Emit parser-backed semantic tokens as JSON\n"
        << "  format               Rewrite files with canonical formatting\n"
        << "  format --check       Report files needing formatting without changing them\n"
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
        if (arg == "--semantic-tokens") {
            options.semantic_tokens = true;
            continue;
        }
        if (arg == "--diagnostics=json" || arg == "--diagnostics=text") {
            options.json_diagnostics = arg == "--diagnostics=json";
            continue;
        }
        if (arg == "format") {
            options.format_file = true;
            continue;
        }
        if (arg != "-" && !arg.empty() && arg.front() == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
        if (!options.input_path.empty()) {
            std::cerr << "Only one input file is supported\n";
            return false;
        }
        options.input_path = std::string(arg);
    }

    if (options.format_file && (options.dump_ast || options.dump_bytecode || options.print_time || options.semantic_tokens)) {
        std::cerr << "format cannot be combined with --dump-ast, --dump-bytecode, --time, or --semantic-tokens\n";
        return false;
    }

    return true;
}

void printSemanticTokens(fairuz::syntax::Result const& result)
{
    std::cout << "{\"astValid\":" << (result.ast_valid ? "true" : "false") << ",\"tokens\":[";
    bool first = true;
    for (auto const& token : result.tokens) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << "{\"line\":" << token.line
                  << ",\"start\":" << token.start
                  << ",\"length\":" << token.length
                  << ",\"type\":\"" << token.type << "\""
                  << ",\"declaration\":" << (token.declaration ? "true" : "false") << '}';
    }
    std::cout << "]}\n";
}

void printAst(fairuz::Array<fairuz::AST::StmtPtr> const& stmts)
{
    fairuz::AST::ASTPrinter printer(true);
    for (u32 i = 0; i < stmts.size(); i++)
        printer.print(stmts[i]);
}

} // namespace

ExitCode format_file(std::string filename, fairuz::Formatter& fmter, size_t& file_count, bool check_only)
{
    fairuz::lex::FileManager fm(filename);
    fairuz::diagnostic::set_source(&fm);
    fairuz::parser::Parser parser(&fm, fairuz::parser::BodyParsing::Eager);
    fairuz::Array<fairuz::AST::StmtPtr> stmts = parser.parse_program();
    if (fairuz::diagnostic::has_errors()) {
        fairuz::diagnostic::dump();
        return ExitCode::DataError;
    }
    fairuz::StringRef fmted = fmter.format(fm.buffer());
    char const* data = fmted.empty() ? "" : fmted.data();
    if (data != fm.buffer()) {
        if (check_only) {
            std::cout << filename << '\n';
        } else {
            std::string error;
            if (!fairuz::platform::write_file_atomic(filename, data, fmted.len(), error)) {
                std::cerr << error << "\n";
                return ExitCode::Software;
            }
        }
        file_count++;
    }
    return ExitCode::Success;
}

static bool is_fairuz_extension(std::string ext) { return ext == ".fa" || ext == ".ف"; }

ExitCode format_directory(std::string dirpath, fairuz::Formatter& fmter, size_t& file_count, bool check_only)
{
    ExitCode result = ExitCode::Success;
    std::vector<std::filesystem::directory_entry> entries;
    for (auto const& entry : std::filesystem::directory_iterator(fairuz::platform::path(dirpath)))
        entries.push_back(entry);
    std::sort(entries.begin(), entries.end(), [](auto const& left, auto const& right) {
        return left.path() < right.path();
    });
    for (auto const& e : entries) {
        if (e.is_symlink())
            continue;
        if (std::filesystem::is_directory(e)) {
            auto current = format_directory(fairuz::platform::utf8(e.path()), fmter, file_count, check_only);
            if (current != ExitCode::Success)
                result = current;
        } else if (std::filesystem::is_regular_file(e)) {
            auto ext = fairuz::platform::utf8(e.path().extension());
            if (is_fairuz_extension(ext)) {
                auto current = format_file(fairuz::platform::utf8(e.path()), fmter, file_count, check_only);
                if (current != ExitCode::Success)
                    result = current;
            }
        }
    }

    return result;
}

ExitCode format(std::string filename, bool check_only)
{
    size_t file_count = 0;

    fairuz::Formatter fmter;
    fairuz::AllocatorContext allocator_context;
    fairuz::set_context(&allocator_context);
    if (std::filesystem::is_directory(fairuz::platform::path(filename))) {
        auto ret = format_directory(filename, fmter, file_count, check_only);
        if (check_only)
            return ret != ExitCode::Success ? ret : (file_count ? ExitCode::NeedsFormatting : ExitCode::Success);
        if (file_count > 0)
            std::cout << "Formatted " << file_count << (file_count == 1 ? " file" : " files") << '\n';
        else
            std::cout << "All files are formatted!" << '\n';
        return ret;
    }

    auto ext = fairuz::platform::utf8(fairuz::platform::path(filename).extension());
    if (std::filesystem::is_regular_file(fairuz::platform::path(filename)) && is_fairuz_extension(ext)) {
        auto ret = format_file(filename, fmter, file_count, check_only);
        if (check_only)
            return ret != ExitCode::Success ? ret : (file_count ? ExitCode::NeedsFormatting : ExitCode::Success);
        if (file_count > 0)
            std::cout << "Formatted " << file_count << (file_count == 1 ? " file" : " files") << '\n';
        else
            std::cout << "File already formatted!" << '\n';
        return ret;
    }
    return ExitCode::Software;
}

int run_main(int argc, char** argv)
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

    if (options.input_path != "-" && !std::filesystem::exists(fairuz::platform::path(options.input_path))) {
        std::cerr << "Input file not found: " << options.input_path << "\n";
        return static_cast<int>(ExitCode::NoInput);
    }

    fairuz::diagnostic::reset();
    fairuz::diagnostic::engine.set_json_output(options.json_diagnostics);
    struct DiagnosticOutput {
        bool json;
        ~DiagnosticOutput()
        {
            if (json)
                std::cerr << fairuz::diagnostic::engine.to_json();
        }
    } diagnostic_output { options.json_diagnostics };

    try {
        if (options.format_file) {
            if (std::filesystem::is_symlink(fairuz::platform::path(options.input_path))) {
                std::cerr << "Path provided is not an ordinary file\n";
                return static_cast<int>(ExitCode::Usage);
            }
            return static_cast<int>(format(options.input_path, options.check_only));
        }

        fairuz::AllocatorContext allocator_context;
        fairuz::set_context(&allocator_context);
        if (options.semantic_tokens) {
            std::string input;
            if (options.input_path == "-")
                input.assign(std::istreambuf_iterator<char>(std::cin), std::istreambuf_iterator<char>());
            else {
                std::ifstream stream(fairuz::platform::path(options.input_path), std::ios::binary);
                input.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
            }
            fairuz::StringRef source(input.size(), '\0');
            if (!input.empty())
                std::memcpy(source.data(), input.data(), input.size());
            printSemanticTokens(fairuz::syntax::Highlighter().highlight(source));
            return static_cast<int>(ExitCode::Success);
        }
        fairuz::lex::FileManager fm(options.input_path);
        fairuz::diagnostic::set_source(&fm);
        fairuz::parser::Parser parser(&fm, options.dump_ast
                ? fairuz::parser::BodyParsing::Eager : fairuz::parser::BodyParsing::Lazy);
        fairuz::Array<fairuz::AST::StmtPtr> stmts = parser.parse_program();

        if (fairuz::diagnostic::has_errors()) {
            fairuz::diagnostic::dump();
            return static_cast<int>(ExitCode::DataError);
        }

        if (options.dump_ast)
            printAst(stmts);

        fairuz::runtime::Compiler compiler;
        fairuz::runtime::Chunk* chunk = compiler.compile(stmts);
        if (!chunk) {
            std::cerr << "Compilation failed: no bytecode was produced\n";
            return static_cast<int>(ExitCode::Software);
        }
        if (fairuz::diagnostic::has_errors())
            return static_cast<int>(ExitCode::DataError);
        chunk->source_path = options.input_path;

        if (options.check_only && !fairuz::runtime::Compiler::compile_all(chunk)) {
            fairuz::diagnostic::dump();
            return static_cast<int>(ExitCode::DataError);
        }

        if (options.dump_bytecode)
            chunk->disassemble();

        if (options.check_only)
            return static_cast<int>(ExitCode::Success);

        fairuz::runtime::VM vm;
        auto const start = std::chrono::steady_clock::now();
        vm.run(chunk, &compiler);
        auto const end = std::chrono::steady_clock::now();

        if (options.print_time) {
            std::chrono::duration<f64> elapsed = end - start;
            std::cerr << "time: " << elapsed.count() << "s\n";
        }

        return static_cast<int>(ExitCode::Success);
    } catch (fairuz::runtime::RuntimeHalt const&) {
        return static_cast<int>(ExitCode::DataError);
    } catch (fairuz::diagnostic::DiagnosticAbort const&) {
        return static_cast<int>(ExitCode::DataError);
    } catch (std::bad_alloc const&) {
        fairuz::diagnostic::report(fairuz::diagnostic::Severity::ERROR, { }, fairuz::ErrorCode::ALLOC_FAILED);
        fairuz::diagnostic::dump();
        return static_cast<int>(ExitCode::DataError);
    } catch (std::length_error const&) {
        fairuz::diagnostic::report(fairuz::diagnostic::Severity::ERROR, { }, fairuz::ErrorCode::ALLOC_FAILED);
        fairuz::diagnostic::dump();
        return static_cast<int>(ExitCode::DataError);
    } catch (std::exception const& ex) {
        fairuz::diagnostic::report(fairuz::diagnostic::Severity::ERROR, { }, fairuz::ErrorCode::INTERNAL_ERROR, ex.what());
        fairuz::diagnostic::dump();
        return static_cast<int>(ExitCode::Software);
    } catch (...) {
        fairuz::diagnostic::report(fairuz::diagnostic::Severity::ERROR, { }, fairuz::ErrorCode::INTERNAL_ERROR, "unknown exception");
        fairuz::diagnostic::dump();
        return static_cast<int>(ExitCode::Software);
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    std::vector<std::string> arguments;
    for (int i = 0; i < argc; ++i) {
        auto text = std::filesystem::path(argv[i]).u8string();
        arguments.emplace_back(reinterpret_cast<char const*>(text.data()), text.size());
    }
    std::vector<char*> pointers;
    for (auto& arg : arguments)
        pointers.push_back(arg.data());
    pointers.push_back(nullptr);
    return run_main(argc, pointers.data());
}
#else
int main(int argc, char** argv) { return run_main(argc, argv); }
#endif
