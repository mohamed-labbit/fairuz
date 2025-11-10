#include "AST.h"
#include "AdvancedFeatures.h"
#include "Lexer.h"
#include "Optimizer.h"
#include "Parser.h"
#include "SemanticAnalyzer.h"
#include "VirtualMachine.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

// Terminal colors
namespace Color {
const char* RESET = "\033[0m";
const char* BOLD = "\033[1m";
const char* RED = "\033[31m";
const char* GREEN = "\033[32m";
const char* YELLOW = "\033[33m";
const char* BLUE = "\033[34m";
const char* MAGENTA = "\033[35m";
const char* CYAN = "\033[36m";
const char* GRAY = "\033[90m";
}

std::string colorize(const std::string& text, const char* color, bool enabled = true)
{
    if (!enabled)
        return text;
    return std::string(color) + text + Color::RESET;
}

// Advanced AST Printer with stats
class EnhancedASTPrinter
{
   private:
    int indent = 0;
    bool useColor;
    int nodeCount = 0;

    void printIndent()
    {
        for (int i = 0; i < indent; i++)
        {
            std::cout << (useColor ? "│ " : "| ");
        }
    }

   public:
    explicit EnhancedASTPrinter(bool color = true) :
        useColor(color)
    {
    }

    void print(const Expr* expr)
    {
        if (!expr)
            return;
        nodeCount++;

        switch (expr->kind)
        {
        case Expr::Kind::Binary : {
            auto* e = static_cast<const BinaryExpr*>(expr);
            printIndent();
            std::cout << colorize("BinaryOp", Color::BOLD, useColor) << " " << colorize(e->op, Color::YELLOW, useColor)
                      << "\n";
            indent++;
            print(e->left.get());
            print(e->right.get());
            indent--;
            break;
        }
        case Expr::Kind::Unary : {
            auto* e = static_cast<const UnaryExpr*>(expr);
            printIndent();
            std::cout << colorize("UnaryOp", Color::BOLD, useColor) << " " << colorize(e->op, Color::YELLOW, useColor)
                      << "\n";
            indent++;
            print(e->operand.get());
            indent--;
            break;
        }
        case Expr::Kind::Literal : {
            auto* e = static_cast<const LiteralExpr*>(expr);
            printIndent();
            std::cout << colorize("Literal", Color::GREEN, useColor) << ": " << e->value << "\n";
            break;
        }
        case Expr::Kind::Name : {
            auto* e = static_cast<const NameExpr*>(expr);
            printIndent();
            std::cout << colorize("Var", Color::CYAN, useColor) << ": " << e->name << "\n";
            break;
        }
        case Expr::Kind::Call : {
            auto* e = static_cast<const CallExpr*>(expr);
            printIndent();
            std::cout << colorize("Call", Color::MAGENTA, useColor) << " [" << e->args.size() << " args]\n";
            indent++;
            print(e->callee.get());
            for (const auto& arg : e->args)
                print(arg.get());
            indent--;
            break;
        }
        case Expr::Kind::List : {
            auto* e = static_cast<const ListExpr*>(expr);
            printIndent();
            std::cout << colorize("List", Color::BLUE, useColor) << " [" << e->elements.size() << "]\n";
            indent++;
            for (const auto& elem : e->elements)
                print(elem.get());
            indent--;
            break;
        }
        default :
            break;
        }
    }

    void print(const Stmt* stmt)
    {
        if (!stmt)
            return;
        nodeCount++;

        switch (stmt->kind)
        {
        case Stmt::Kind::Assignment : {
            auto* s = static_cast<const AssignmentStmt*>(stmt);
            printIndent();
            std::cout << colorize("Assign", Color::BOLD, useColor) << " " << colorize(s->target, Color::CYAN, useColor)
                      << "\n";
            indent++;
            print(s->value.get());
            indent--;
            break;
        }
        case Stmt::Kind::If : {
            auto* s = static_cast<const IfStmt*>(stmt);
            printIndent();
            std::cout << colorize("If", Color::BOLD, useColor) << "\n";
            indent++;
            print(s->condition.get());
            for (const auto& st : s->thenBlock)
                print(st.get());
            indent--;
            break;
        }
        case Stmt::Kind::While : {
            auto* s = static_cast<const WhileStmt*>(stmt);
            printIndent();
            std::cout << colorize("While", Color::BOLD, useColor) << "\n";
            indent++;
            print(s->condition.get());
            for (const auto& st : s->body)
                print(st.get());
            indent--;
            break;
        }
        case Stmt::Kind::For : {
            auto* s = static_cast<const ForStmt*>(stmt);
            printIndent();
            std::cout << colorize("For", Color::BOLD, useColor) << " " << s->target << "\n";
            indent++;
            print(s->iter.get());
            for (const auto& st : s->body)
                print(st.get());
            indent--;
            break;
        }
        case Stmt::Kind::FunctionDef : {
            auto* s = static_cast<const FunctionDef*>(stmt);
            printIndent();
            std::cout << colorize("Function", Color::BOLD, useColor) << " " << colorize(s->name, Color::GREEN, useColor)
                      << "(";
            for (size_t i = 0; i < s->params.size(); i++)
            {
                std::cout << s->params[i];
                if (i + 1 < s->params.size())
                    std::cout << ", ";
            }
            std::cout << ")\n";
            indent++;
            for (const auto& st : s->body)
                print(st.get());
            indent--;
            break;
        }
        case Stmt::Kind::Return : {
            auto* s = static_cast<const ReturnStmt*>(stmt);
            printIndent();
            std::cout << colorize("Return", Color::BOLD, useColor) << "\n";
            indent++;
            print(s->value.get());
            indent--;
            break;
        }
        case Stmt::Kind::Expression : {
            auto* s = static_cast<const ExprStmt*>(stmt);
            print(s->expression.get());
            break;
        }
        default :
            break;
        }
    }

    int getNodeCount() const { return nodeCount; }
};

void printBanner(bool useColor)
{
    std::cout << colorize("╔══════════════════════════════════════════════════════════════╗\n", Color::CYAN, useColor);
    std::cout << colorize("║   ", Color::CYAN, useColor)
              << colorize("Advanced Arabic-Python Compiler & VM", Color::BOLD, useColor)
              << colorize("              ║\n", Color::CYAN, useColor);
    std::cout << colorize("║   ", Color::CYAN, useColor) << "Version 3.0 - Production Ready with JIT         "
              << colorize("║\n", Color::CYAN, useColor);
    std::cout << colorize("╚══════════════════════════════════════════════════════════════╝\n", Color::CYAN, useColor);
}

void printHelp(const char* prog)
{
    std::cout << "Usage: " << prog << " <source.py> [options]\n\n";
    std::cout << "Compilation Options:\n";
    std::cout << "  -O0             No optimization\n";
    std::cout << "  -O1             Basic optimization (default)\n";
    std::cout << "  -O2             Advanced optimization\n";
    std::cout << "  -O3             Aggressive optimization\n\n";
    std::cout << "Execution Options:\n";
    std::cout << "  --run           Execute code after compilation\n";
    std::cout << "  --bytecode      Show bytecode\n";
    std::cout << "  --cpp           Generate C++ code\n";
    std::cout << "  --parallel      Use parallel parsing\n\n";
    std::cout << "Output Options:\n";
    std::cout << "  --no-color      Disable colored output\n";
    std::cout << "  --no-ast        Skip AST visualization\n";
    std::cout << "  --stats         Show detailed statistics\n";
    std::cout << "  --profile       Enable performance profiling\n";
    std::cout << "  --json          Output diagnostics as JSON\n\n";
    std::cout << "Analysis Options:\n";
    std::cout << "  --check         Check only (no execution)\n";
    std::cout << "  --strict        Enable strict type checking\n";
    std::cout << "  --warnings      Show all warnings\n\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        printBanner(true);
        printHelp(argv[0]);
        return 1;
    }

    // Parse arguments
    std::string filename = argv[1];
    int optLevel = 1;
    bool useColor = true;
    bool printAST = true;
    bool showStats = false;
    bool enableProfile = false;
    bool runCode = false;
    bool showBytecode = false;
    bool generateCPP = false;
    bool useParallel = false;
    bool jsonOutput = false;
    bool checkOnly = false;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-O0")
            optLevel = 0;
        else if (arg == "-O1")
            optLevel = 1;
        else if (arg == "-O2")
            optLevel = 2;
        else if (arg == "-O3")
            optLevel = 3;
        else if (arg == "--no-color")
            useColor = false;
        else if (arg == "--no-ast")
            printAST = false;
        else if (arg == "--stats")
            showStats = true;
        else if (arg == "--profile")
            enableProfile = true;
        else if (arg == "--run")
            runCode = true;
        else if (arg == "--bytecode")
            showBytecode = true;
        else if (arg == "--cpp")
            generateCPP = true;
        else if (arg == "--parallel")
            useParallel = true;
        else if (arg == "--json")
            jsonOutput = true;
        else if (arg == "--check")
            checkOnly = true;
        else if (arg == "--help" || arg == "-h")
        {
            printBanner(useColor);
            printHelp(argv[0]);
            return 0;
        }
    }

    printBanner(useColor);

    // Read source
    std::ifstream file(filename);
    if (!file)
    {
        std::cerr << colorize("✗ Error: ", Color::RED, useColor) << "Cannot open file: " << filename << "\n";
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    ParserProfiler profiler;
    DiagnosticEngine diagnostics;
    diagnostics.setSource(source);

    auto totalStart = std::chrono::high_resolution_clock::now();

    try
    {
        // PHASE 1: LEXICAL ANALYSIS
        std::cout << "\n" << colorize("▶ Phase 1: Lexical Analysis", Color::CYAN, useColor) << "\n";

        auto lexStart = std::chrono::high_resolution_clock::now();
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        auto lexEnd = std::chrono::high_resolution_clock::now();
        auto lexTime = std::chrono::duration<double, std::milli>(lexEnd - lexStart).count();

        if (enableProfile)
            profiler.recordPhase("Lexing", lexTime);

        std::cout << colorize("  ✓ ", Color::GREEN, useColor) << tokens.size() << " tokens in " << std::fixed
                  << std::setprecision(2) << lexTime << "ms"
                  << colorize(
                       " (" + std::to_string(int(tokens.size() / lexTime * 1000)) + " tok/s)", Color::GRAY, useColor)
                  << "\n";

        // PHASE 2: SYNTAX ANALYSIS
        std::cout << "\n" << colorize("▶ Phase 2: Syntax Analysis", Color::CYAN, useColor) << "\n";

        auto parseStart = std::chrono::high_resolution_clock::now();
        Parser parser(std::move(tokens));
        auto ast = parser.parse();
        auto parseEnd = std::chrono::high_resolution_clock::now();
        auto parseTime = std::chrono::duration<double, std::milli>(parseEnd - parseStart).count();

        if (enableProfile)
            profiler.recordPhase("Parsing", parseTime);

        std::cout << colorize("  ✓ ", Color::GREEN, useColor) << ast.size() << " statements in " << std::fixed
                  << std::setprecision(2) << parseTime << "ms\n";

        // PHASE 3: SEMANTIC ANALYSIS
        std::cout << "\n" << colorize("▶ Phase 3: Semantic Analysis", Color::CYAN, useColor) << "\n";

        auto semanticStart = std::chrono::high_resolution_clock::now();
        SemanticAnalyzer analyzer;
        analyzer.analyze(ast);
        auto semanticEnd = std::chrono::high_resolution_clock::now();
        auto semanticTime = std::chrono::duration<double, std::milli>(semanticEnd - semanticStart).count();

        if (enableProfile)
            profiler.recordPhase("Semantic Analysis", semanticTime);

        const auto& issues = analyzer.getIssues();
        int errors = 0, warnings = 0;
        for (const auto& issue : issues)
        {
            if (issue.severity == SemanticAnalyzer::Issue::Severity::Error)
                errors++;
            else if (issue.severity == SemanticAnalyzer::Issue::Severity::Warning)
                warnings++;
        }

        std::cout << colorize("  ✓ ", Color::GREEN, useColor) << "Analysis complete in " << std::fixed
                  << std::setprecision(2) << semanticTime << "ms\n";
        std::cout << "    " << errors << " errors, " << warnings << " warnings\n";

        if (!issues.empty() && !checkOnly)
        {
            std::cout << "\n" << colorize("▶ Issues Found", Color::YELLOW, useColor) << "\n";
            analyzer.printReport();
        }

        if (errors > 0 && !checkOnly)
        {
            std::cout << "\n" << colorize("✗ Compilation failed due to errors", Color::RED, useColor) << "\n";
            return 1;
        }

        if (checkOnly)
        {
            std::cout << "\n" << colorize("✓ Check complete", Color::GREEN, useColor) << "\n";
            return errors > 0 ? 1 : 0;
        }

        // PHASE 4: OPTIMIZATION
        if (optLevel > 0)
        {
            std::cout << "\n"
                      << colorize("▶ Phase 4: Optimization (O" + std::to_string(optLevel) + ")", Color::CYAN, useColor)
                      << "\n";

            auto optStart = std::chrono::high_resolution_clock::now();
            ASTOptimizer optimizer;
            ast = optimizer.optimize(std::move(ast), optLevel);
            auto optEnd = std::chrono::high_resolution_clock::now();
            auto optTime = std::chrono::duration<double, std::milli>(optEnd - optStart).count();

            if (enableProfile)
                profiler.recordPhase("Optimization", optTime);

            std::cout << colorize("  ✓ ", Color::GREEN, useColor) << "Optimized in " << std::fixed
                      << std::setprecision(2) << optTime << "ms\n";

            const auto& stats = optimizer.getStats();
            int totalOpts = stats.constantFolds + stats.deadCodeEliminations + stats.strengthReductions;
            std::cout << "    " << totalOpts << " optimizations applied\n";

            if (showStats)
            {
                optimizer.printStats();
            }
        }

        // PHASE 5: CODE GENERATION
        BytecodeCompiler::CompiledCode bytecode;

        if (runCode || showBytecode)
        {
            std::cout << "\n" << colorize("▶ Phase 5: Code Generation", Color::CYAN, useColor) << "\n";

            auto codegenStart = std::chrono::high_resolution_clock::now();
            BytecodeCompiler compiler;
            bytecode = compiler.compile(ast);
            auto codegenEnd = std::chrono::high_resolution_clock::now();
            auto codegenTime = std::chrono::duration<double, std::milli>(codegenEnd - codegenStart).count();

            if (enableProfile)
                profiler.recordPhase("Code Generation", codegenTime);

            std::cout << colorize("  ✓ ", Color::GREEN, useColor) << bytecode.instructions.size()
                      << " instructions generated in " << std::fixed << std::setprecision(2) << codegenTime << "ms\n";
        }

        // Generate C++ code
        if (generateCPP)
        {
            std::cout << "\n" << colorize("▶ C++ Code Generation", Color::CYAN, useColor) << "\n";
            CodeGenerator codegen;
            std::string cppCode = codegen.generateCPP(ast);

            std::string outfile = filename.substr(0, filename.rfind('.')) + ".cpp";
            std::ofstream out(outfile);
            out << cppCode;
            out.close();

            std::cout << colorize("  ✓ ", Color::GREEN, useColor) << "Generated " << outfile << "\n";
        }

        // Show AST
        if (printAST)
        {
            std::cout << "\n" << colorize("▶ Abstract Syntax Tree", Color::CYAN, useColor) << "\n\n";
            EnhancedASTPrinter printer(useColor);
            for (const auto& stmt : ast)
            {
                printer.print(stmt.get());
            }
            std::cout << "\n  Total nodes: " << printer.getNodeCount() << "\n";
        }

        // Show bytecode
        if (showBytecode)
        {
            std::cout << "\n" << colorize("▶ Bytecode", Color::CYAN, useColor) << "\n\n";
            for (size_t i = 0; i < bytecode.instructions.size(); i++)
            {
                const auto& instr = bytecode.instructions[i];
                std::cout << "  " << std::setw(4) << i << "  ";

                // Print opcode name
                switch (instr.op)
                {
                case OpCode::LOAD_CONST :
                    std::cout << "LOAD_CONST  " << instr.arg;
                    break;
                case OpCode::LOAD_VAR :
                    std::cout << "LOAD_VAR    " << instr.arg;
                    break;
                case OpCode::STORE_VAR :
                    std::cout << "STORE_VAR   " << instr.arg;
                    break;
                case OpCode::ADD :
                    std::cout << "ADD";
                    break;
                case OpCode::SUB :
                    std::cout << "SUB";
                    break;
                case OpCode::MUL :
                    std::cout << "MUL";
                    break;
                case OpCode::DIV :
                    std::cout << "DIV";
                    break;
                case OpCode::PRINT :
                    std::cout << "PRINT       " << instr.arg;
                    break;
                case OpCode::JUMP :
                    std::cout << "JUMP        " << instr.arg;
                    break;
                case OpCode::JUMP_IF_FALSE :
                    std::cout << "JUMP_IF_FALSE " << instr.arg;
                    break;
                case OpCode::HALT :
                    std::cout << "HALT";
                    break;
                default :
                    std::cout << "???";
                    break;
                }
                std::cout << "\n";
            }
        }

        // PHASE 6: EXECUTION
        if (runCode)
        {
            std::cout << "\n" << colorize("▶ Execution", Color::CYAN, useColor) << "\n";
            std::cout << colorize("Output:", Color::BOLD, useColor) << "\n";
            std::cout << colorize("─────────────────────────────────────\n", Color::GRAY, useColor);

            auto execStart = std::chrono::high_resolution_clock::now();
            VirtualMachine vm;
            vm.execute(bytecode);
            auto execEnd = std::chrono::high_resolution_clock::now();
            auto execTime = std::chrono::duration<double, std::milli>(execEnd - execStart).count();

            if (enableProfile)
                profiler.recordPhase("Execution", execTime);

            std::cout << colorize("─────────────────────────────────────\n", Color::GRAY, useColor);
            std::cout << colorize("  ✓ ", Color::GREEN, useColor) << "Executed in " << std::fixed
                      << std::setprecision(2) << execTime << "ms\n";
        }

        // Summary
        auto totalEnd = std::chrono::high_resolution_clock::now();
        auto totalTime = std::chrono::duration<double, std::milli>(totalEnd - totalStart).count();

        std::cout << "\n" << colorize("═══ Summary ═══", Color::BOLD, useColor) << "\n";
        std::cout << colorize("✓ ", Color::GREEN, useColor) << "Total time: " << std::fixed << std::setprecision(2)
                  << totalTime << "ms\n";
        std::cout << colorize("✓ ", Color::GREEN, useColor)
                  << "Lines processed: " << std::count(source.begin(), source.end(), '\n') + 1 << "\n";
        std::cout << colorize("✓ ", Color::GREEN, useColor) << "Compilation successful!\n";

        // Performance profile
        if (enableProfile)
        {
            std::cout << "\n";
            profiler.printReport();
        }

    } catch (const std::exception& e)
    {
        std::cerr << "\n" << colorize("✗ Fatal Error: ", Color::RED, useColor) << e.what() << "\n";
        return 1;
    }

    return 0;
}