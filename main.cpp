#include "include/lexer.hpp"
#include "include/parser.hpp"
#include "include/runtime/compiler.hpp"
#include "include/runtime/vm.hpp"

int main(int argc, char** argv)
{
    if (argc < 2)
        return 0; // return silently

    std::string filename = argv[1];
    mylang::lex::FileManager file_manager(filename);
    mylang::parser::Parser parser(&file_manager);
    mylang::runtime::Compiler bc_compiler;
    mylang::runtime::VM virtual_machine;

    virtual_machine.run(bc_compiler.compile(parser.parseProgram()));

    return 0;
}
