#include "include/ast_printer.hpp"
#include "include/compiler.hpp"
#include "include/lexer.hpp"
#include "include/parser.hpp"
#include "include/vm.hpp"
#include <chrono>

int main(int argc, char** argv)
{
    if (argc < 2)
        return 0; // return silently
    mylang::AllocatorContext g_ctx;
    mylang::setContext(&g_ctx);
    std::string filename = argv[1];
    mylang::lex::FileManager file_manager(filename);
    mylang::parser::Parser parser(&file_manager);
    mylang::ast::ASTPrinter printer;
    auto stmts = parser.parseProgram();
    for (int i = 0; i < stmts.size(); ++i)
        printer.print(stmts[i]);
    mylang::runtime::Compiler bc_compiler;
    mylang::runtime::VM virtual_machine;
    auto bc = bc_compiler.compile(stmts);
    if (bc)
        bc->disassemble();
    auto start = std::chrono::high_resolution_clock::now();
    virtual_machine.run(bc);
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    std::cout << '\n' << duration.count() << std::endl;
    return 0;
}
