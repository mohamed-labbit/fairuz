// Source-backed compiler fixtures: write, load, and parse real Fairuz programs.
#ifndef FAIRUZ_TEST_SOURCE_PROGRAM_HPP
#define FAIRUZ_TEST_SOURCE_PROGRAM_HPP

#include "Compiler/compiler.hpp"
#include "Parser/parser.hpp"
#include "test_process.hpp"
#include "test_runtime.hpp"

#include <atomic>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace test_source {

inline std::string join(fairuz::Array<std::string> const& pieces, std::string const& separator = "")
{
    std::string text;
    for (auto const& piece : pieces) {
        if (!text.empty())
            text += separator;
        text += piece;
    }
    return text;
}

inline std::string indent(std::string text)
{
    if (text.empty())
        text = "عدم\n";
    std::istringstream input(text);
    std::string result, line;
    while (std::getline(input, line))
        result += "    " + line + "\n";
    return result;
}

inline std::string floating(double value)
{
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    auto text = out.str();
    if (text.find_first_of(".eE") == std::string::npos)
        text += ".0";
    return text;
}

inline std::string quote(std::string const& value)
{
    std::ostringstream out;
    out << std::quoted(value);
    return out.str();
}

inline std::filesystem::path write(std::string const& source)
{
    static std::atomic<unsigned long> serial { 0 };
    auto path = std::filesystem::temp_directory_path()
        / ("fairuz_source_" + std::to_string(test_process::process_id())
            + "_" + std::to_string(serial++) + ".fa");
    std::ofstream output(path, std::ios::binary);
    output << source;
    output.close();
    if (!output)
        throw std::runtime_error("Could not write test source: " + path.string());
    return path;
}

struct Program {
    std::filesystem::path path;
    fairuz::SourcePtr source;
    fairuz::Array<fairuz::AST::StmtPtr> statements;

    explicit Program(std::string const& text,
        fairuz::parser::BodyParsing mode = fairuz::parser::BodyParsing::Eager)
        : path(write(text))
    {
        fairuz::FileManager file(fairuz::platform::utf8(path));
        fairuz::diagnostic::reset();
        fairuz::diagnostic::SourceScope scope(&file);
        source = fairuz::diagnostic::engine.source();
        fairuz::parser::Parser parser(&file, mode);
        statements = parser.parse_program();
    }

    ~Program()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    Program(Program const&) = delete;
    Program& operator=(Program const&) = delete;

    auto compile()
    {
        if (fairuz::diagnostic::has_errors())
            throw std::runtime_error("Test source did not parse: " + path.string());
        fairuz::runtime::Compiler compiler(source);
        return compiler.compile(statements);
    }
};

inline fairuz::runtime::Chunk* compile(std::string const& text)
{
    Program program(text);
    return run_success(program.compile());
}

} // namespace test_source
#endif
