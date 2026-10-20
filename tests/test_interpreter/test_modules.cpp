//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies module loading, caching, and recovery.
//===----------------------------------------------------------------------===//

#include "Compiler/compiler.hpp"
#include "Compiler/opcode.hpp"
#include "Diag/diagnostic.hpp"
#include "Interpreter/vm.hpp"
#include "Parser/ast.hpp"
#include "Parser/lexer.hpp"
#include "Parser/parser.hpp"
#include "Platform.hpp"
#include "test_config.h"

#include "test_runtime.hpp"
#include "test_source_program.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>

using namespace fairuz;
using namespace fairuz::lex;
using namespace fairuz::parser;
using namespace fairuz::runtime;

namespace {

void set_environment(std::string const& name, std::string const& value)
{
#ifdef _WIN32
    if (_wputenv_s(platform::path(name).c_str(), platform::path(value).c_str()) != 0) throw std::runtime_error("Failed to update test environment");
#else
    if (setenv(name.c_str(), value.c_str(), 1) != 0) throw std::runtime_error("Failed to update test environment");
#endif
}
void unset_environment(std::string const& name)
{
#ifdef _WIN32
    set_environment(name, "");
#else
    unsetenv(name.c_str());
#endif
}

class EnvironmentGuard {
public:
    explicit EnvironmentGuard(char const* name)
        : m_name(name)
    {
        if (auto old = platform::environment(name); !old.empty()) m_old_value = old;
        unset_environment(m_name);
    }

    EnvironmentGuard(char const* name, std::string const& value)
        : m_name(name)
    {
        if (auto old = platform::environment(name); !old.empty()) m_old_value = old;
        set_environment(m_name, value);
    }

    ~EnvironmentGuard()
    {
        if (m_old_value.has_value()) set_environment(m_name, *m_old_value);
        else unset_environment(m_name);
    }

private:
    std::string m_name;
    std::optional<std::string> m_old_value;
};

class ModuleFixture : public ::testing::Test {
protected:
    std::filesystem::path directory;

    void SetUp() override
    {
        diagnostic::reset();
        directory = std::filesystem::temp_directory_path() / ("fairuz-modules-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        std::filesystem::create_directories(directory);
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
        diagnostic::reset();
    }

    std::filesystem::path write(std::string const& name, std::string const& source)
    {
        auto path = directory / platform::path(name);
        std::ofstream out(path, std::ios::binary);
        out << source;
        out.close();
        EXPECT_TRUE(out.good());
        return path;
    }

    // Only recovery tests intentionally reuse a VM and inspect its Values/errors.
    VM::RuntimeResult run_in_vm(std::filesystem::path const& path, VM& vm)
    {
        FileManager source(platform::utf8(path));
        diagnostic::set_source(&source);
        Parser parser(&source);
        auto statements = parser.parse_program();
        EXPECT_FALSE(diagnostic::has_errors());
        Compiler compiler(diagnostic::engine.source());
        auto compiled = compiler.compile(statements);
        if (compiled.has_error()) return compiled.error();
        Chunk* chunk = run_success(compiled);
        if (test_config::dump_bytecode) chunk->disassemble();
        EXPECT_NE(chunk, nullptr);
        EXPECT_FALSE(diagnostic::has_errors());
        chunk->source_path = platform::utf8(path);
        return vm.run(chunk, &compiler);
    }
};

} // namespace

TEST(ModuleLexer, RecognizesImportKeywords)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "من رياضيات استورد قاسم باسم قاسم_مشترك\n";
    Lexer lexer(&source);
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 8u);
    EXPECT_EQ(tokens[1]->type(), tok::TokenType::KW_FROM);
    EXPECT_EQ(tokens[3]->type(), tok::TokenType::KW_IMPORT);
    EXPECT_EQ(tokens[5]->type(), tok::TokenType::KW_AS);
}

TEST(ModuleParser, PythonStyleAssertLowersToNativeCall)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "assert 1 = 1، \"message\"\n";
    Parser parser(&source);
    auto statements = parser.parse_program();
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(statements.size(), 1u);
    auto* expression = AST::as_expr_stmt(statements[0])->expr;
    ASSERT_TRUE(AST::is_call(expression));
    auto* call = AST::as_call(expression);
    EXPECT_EQ(AST::as_identifier(call->callee)->spelling, "تاكد");
    EXPECT_EQ(call->args.size(), 2u);
}

TEST(ModuleParser, ParsesImportsAliasesAndParentClass)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "استورد مجموعات باسم ج\n"
                      "من نتيجة استورد نجاح باسم ناجح\n"
                      "نوع طفل(اصل):\n"
                      "    دالة قيمة():\n"
                      "        ارجع 1\n";
    Parser parser(&source);
    auto statements = parser.parse_program();
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(statements.size(), 3u);
    ASSERT_TRUE(AST::is_import(statements[0]));
    EXPECT_EQ(AST::as_import(statements[0])->get_module(), "مجموعات");
    EXPECT_EQ(AST::as_import(statements[0])->get_aliases()[0], "ج");
    ASSERT_TRUE(AST::as_import(statements[1])->imports_member());
    EXPECT_EQ(AST::as_import(statements[1])->get_names()[0], "نجاح");
    auto* klass = AST::as_class_def(statements[2]);
    ASSERT_NE(klass->parent, nullptr);
    EXPECT_EQ(AST::as_identifier(klass->parent)->spelling, "اصل");
}

TEST(ModuleCompiler, EmitsImportAndParentDescriptor)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "استورد مجموعات\n"
                      "نوع طفل(اصل):\n"
                      "    دالة قيمة():\n"
                      "        ارجع 1\n";
    Parser parser(&source);
    Chunk* chunk = run_success(Compiler(diagnostic::engine.source()).compile(parser.parse_program()));
    ASSERT_NE(chunk, nullptr);
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_FALSE(chunk->code.empty());
    EXPECT_EQ(instr_op(chunk->code[0]), OpCode::IMPORT_MODULE);
    ASSERT_EQ(chunk->class_descriptors.size(), 1u);
    EXPECT_EQ(chunk->class_descriptors[0].parent_name, "اصل");
}

TEST(ModuleCompiler, HundredsOfImportsReuseTemporaryRegisters)
{
    diagnostic::reset();
    FileManager source;
    for (int i = 0; i < 300; ++i) {
        source.buffer() += "من مجموعات استورد مدى باسم اسم";
        source.buffer() += std::to_string(i);
        source.buffer() += "\n";
    }
    source.buffer() += "اسم299(0، 1، 1)\n";

    Parser parser(&source);
    Chunk* chunk = run_success(Compiler(diagnostic::engine.source()).compile(parser.parse_program()));
    ASSERT_NE(chunk, nullptr);
    EXPECT_FALSE(diagnostic::has_errors());
    EXPECT_LT(chunk->local_count, 16u);
}

TEST(ModuleCompiler, HundredsOfMembersInOneImportReuseTemporaryRegisters)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "من مجموعات استورد ";
    for (int i = 0; i < 300; ++i) {
        if (i != 0) source.buffer() += "، ";
        source.buffer() += "مدى باسم اسم";
        source.buffer() += std::to_string(i);
    }
    source.buffer() += "\n"
                       "اسم299(0، 1، 1)\n";
    Parser parser(&source);
    Chunk* chunk = run_success(Compiler(diagnostic::engine.source()).compile(parser.parse_program()));
    ASSERT_NE(chunk, nullptr);
    EXPECT_FALSE(diagnostic::has_errors());
    EXPECT_LT(chunk->local_count, 16u);
}

TEST_F(ModuleFixture, ChainedMethodCallsKeepArgumentsContiguous)
{
    auto path = write("متسلسل_الاستيراد.ف",
        "نوع صندوق:\n"
        "    دالة بداية():\n"
        "        هذا.قيم := قائمة()\n"
        "    دالة اضف_قيمة(قيمة):\n"
        "        اضف(هذا.قيم، قيمة)\n"
        "        ارجع هذا\n"
        "    دالة اخر():\n"
        "        ارجع هذا.قيم[طول(هذا.قيم) - 1]\n"
        "دالة اختبر():\n"
        "    صندوق_اختبار := صندوق()\n"
        "    صندوق_اختبار.اضف_قيمة(1).اضف_قيمة(2)\n"
        "    ارجع صندوق_اختبار.اخر()\n"
        "نتيجة_الاختبار := اختبر()\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 2)\n");

    EXPECT_EQ(test_source::run_file(path), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, LoadsOnceAndSupportsFromImport)
{
    write("العداد.ف",
        "قيمة := قائمة()\n"
        "اضف(قيمة، 1)\n"
        "دالة عدد():\n"
        "    ارجع طول(قيمة)\n");
    auto main = write("رئيسي.ف",
        "استورد العداد باسم اول\n"
        "استورد العداد باسم ثان\n"
        "من العداد استورد عدد\n"
        "تاكد(اول = ثان)\n"
        "نتيجة_الاختبار := عدد()\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 1)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, ExplicitStdlibDirectoryWinsOverLocalNameCollision)
{
    auto project = directory / "project";
    auto stdlib = directory / "stdlib";
    std::filesystem::create_directories(project);
    std::filesystem::create_directories(stdlib);

    std::ofstream local(project / platform::path("ملفات.ف"), std::ios::binary);
    local << "دالة قيمة():\n"
             "    ارجع 1\n";
    std::ofstream standard(stdlib / platform::path("ملفات.ف"), std::ios::binary);
    standard << "دالة قيمة():\n"
                "    ارجع 2\n";
    std::ofstream main(project / platform::path("رئيسي.ف"), std::ios::binary);
    main << "من ملفات استورد قيمة\n"
            "نتيجة_الاختبار := قيمة()\n"
            "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
            "تاكد(نتيجة_الاختبار = 2)\n";
    local.close();
    standard.close();
    main.close();

    EnvironmentGuard configured_stdlib("FAIRUZ_STDLIB", platform::utf8(stdlib));
    EXPECT_EQ(test_source::run_file(project / platform::path("رئيسي.ف")), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, BundledStdlibWinsOverLocalNameCollisionWithoutEnvironmentOverride)
{
    auto project = directory / "project";
    std::filesystem::create_directories(project);
    {
        std::ofstream local(project / platform::path("ملفات.ف"), std::ios::binary);
        local << "دالة ملف(المسار، الوضع):\n"
                 "    ارجع 1\n";
    }
    {
        std::ofstream main(project / platform::path("رئيسي.ف"), std::ios::binary);
        main << "من ملفات استورد ملف\n"
                "نتيجة_الاختبار := ملف(\"x\"، \"قراءة\").يعمل()\n"
                "تاكد(صنف(نتيجة_الاختبار) = \"منطقي\")\n"
                "تاكد(نتيجة_الاختبار = خطا)\n";
    }

    EnvironmentGuard no_override("FAIRUZ_STDLIB");
    EXPECT_EQ(test_source::run_file(project / platform::path("رئيسي.ف")), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, KeepsModuleGlobalsIsolated)
{
    write("ا.ف",
        "سر := 10\n"
        "دالة قيمة():\n"
        "    ارجع سر\n");
    write("ب.ف",
        "سر := 20\n"
        "دالة قيمة():\n"
        "    ارجع سر\n");
    auto main = write("رئيسي.ف",
        "استورد ا\n"
        "استورد ب\n"
        "دالة الناتج():\n"
        "    ارجع ا.قيمة() + ب.قيمة()\n"
        "نتيجة_الاختبار := الناتج()\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 30)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, BreaksImportCyclesWithPartialModules)
{
    write("ا.ف",
        "قيمة := 7\n"
        "استورد ب\n");
    write("ب.ف",
        "استورد ا\n"
        "دالة اقرا():\n"
        "    ارجع ا.قيمة\n");
    auto main = write("رئيسي.ف",
        "استورد ب\n"
        "نتيجة_الاختبار := ب.اقرا()\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 7)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, ImportedFunctionsRetainDefiningEnvironment)
{
    write("اساس.ف",
        "سر := 40\n"
        "دالة زد(قيمة):\n"
        "    ارجع قيمة + سر\n");
    auto main = write("رئيسي.ف",
        "من اساس استورد زد\n"
        "سر := 1000\n"
        "نتيجة_الاختبار := زد(2)\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 42)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, LazyFunctionsWithSameNameKeepTheirModuleContext)
{
    write("وحدة_يسرى.ف",
        "سر := 10\n"
        "دالة قيمة():\n"
        "    ارجع سر\n"
        "دالة غير_مستعملة():\n"
        "    اخرج\n");
    write("وحدة_يمنى.ف",
        "سر := 32\n"
        "دالة قيمة():\n"
        "    ارجع سر\n");
    auto main = write("رئيسي.ف",
        "من وحدة_يسرى استورد قيمة باسم اول\n"
        "استورد وحدة_يمنى باسم ثان\n"
        "دالة قيمة():\n"
        "    ارجع 1000\n"
        "نتيجة_الاختبار := طبيعي(اول() + ثان.قيمة())\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 42)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, SingleInheritancePreservesFieldsAndOverridesMethods)
{
    write("اساس.ف",
        "نوع اصل:\n"
        "    دالة بداية(قيمة):\n"
        "        هذا.قيمة := قيمة\n"
        "    دالة احصل():\n"
        "        ارجع هذا.قيمة\n"
        "    دالة وصف():\n"
        "        ارجع 1\n");
    auto main = write("رئيسي.ف",
        "من اساس استورد اصل\n"
        "نوع فرع(اصل):\n"
        "    دالة وصف():\n"
        "        ارجع 2\n"
        "    دالة ضاعف():\n"
        "        ارجع هذا.قيمة * 2\n"
        "كائن := فرع(3)\n"
        "تاكد(كائن.احصل() = 3)\n"
        "تاكد(كائن.وصف() = 2)\n"
        "نتيجة_الاختبار := كائن.ضاعف()\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 6)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, InheritedMethodRetainsParentModuleEnvironment)
{
    write("اساس.ف",
        "سر := 9\n"
        "نوع اصل:\n"
        "    دالة قيمة():\n"
        "        ارجع سر\n");
    auto main = write("رئيسي.ف",
        "من اساس استورد اصل\n"
        "سر := 99\n"
        "نوع فرع(اصل):\n"
        "    دالة اخر():\n"
        "        ارجع سر\n"
        "كائن := فرع()\n"
        "تاكد(كائن.اخر() = 99)\n"
        "نتيجة_الاختبار := كائن.قيمة()\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 9)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, MultipleNamesInOneImportStmt)
{
    write("قيم_الوحدة.ف",
        "قيمة := 7\n"
        "دالة زد(س):\n"
        "    ارجع س + قيمة\n"
        "دالة ضاعف(س):\n"
        "    ارجع س * 2\n");
    auto main = write("رئيسي.ف",
        "من قيم_الوحدة استورد قيمة باسم عدد، زد، ضاعف باسم مرتين\n"
        "نتيجة_الاختبار := مرتين(زد(عدد))\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 28)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, WholeModuleImportsBindDefaultAndExplicitAliases)
{
    write("قيم_الوحدة.ف",
        "دالة زد(س):\n"
        "    ارجع س + 1\n");
    auto main = write("رئيسي.ف",
        "استورد قيم_الوحدة\n"
        "استورد قيم_الوحدة باسم قيم\n"
        "نتيجة_الاختبار := قيم.زد(قيم_الوحدة.زد(40))\n"
        "تاكد(صنف(نتيجة_الاختبار) = \"طبيعي\")\n"
        "تاكد(نتيجة_الاختبار = 42)\n");
    EXPECT_EQ(test_source::run_file(main), ExitStatus::SUCCESS);
}

TEST_F(ModuleFixture, FailedModuleExecutionCanBeRetriedInSameVM)
{
    write("معطوب.ف", "تاكد(خطا)\n");
    auto main = write("رئيسي.ف",
        "استورد معطوب\n"
        "معطوب.قيمة()\n");
    VM vm;
    run_failed(run_in_vm(main, vm));
    write("معطوب.ف",
        "دالة قيمة():\n"
        "    ارجع 42\n");
    diagnostic::reset();
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(run_in_vm(main, vm)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 42);
}

TEST_F(ModuleFixture, FailedModuleParseCanBeRetriedInSameVM)
{
    write("معطوب.ف",
        "دالة قيمة():\n"
        "    ارجع (\n");
    auto main = write("رئيسي.ف",
        "استورد معطوب\n"
        "معطوب.قيمة()\n");
    VM vm;
    run_failed(run_in_vm(main, vm));
    write("معطوب.ف",
        "دالة قيمة():\n"
        "    ارجع 7\n");
    diagnostic::reset();
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run_success(run_in_vm(main, vm)));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

/*
TEST_F(ModuleFixture, RuntimeTracebackRetainsEachDefiningSource)
{
    auto inner = write("inner.ف", "دالة افشل():\n    تاكد(خطا، \"inner failure\")\n");
    auto outer = write("outer.ف", "من inner استورد افشل\nدالة نفذ():\n    افشل()\n");
    auto main = write("main.ف", "من outer استورد نفذ\nنفذ()\n");
    VM vm;
    run_failed(run_in_vm(main, vm));
    auto json = diagnostic::engine.to_json();
    auto frames = json.substr(json.find("\"traceback\""));
    auto main_position = frames.find(platform::utf8(main));
    auto outer_position = frames.find(platform::utf8(outer));
    auto inner_position = frames.find(platform::utf8(inner));
    ASSERT_NE(main_position, std::string::npos);
    ASSERT_NE(outer_position, std::string::npos);
    ASSERT_NE(inner_position, std::string::npos);
    EXPECT_LT(main_position, outer_position);
    EXPECT_LT(outer_position, inner_position);
    EXPECT_NE(json.find("\"type\":\"AssertionError\""), std::string::npos);
    EXPECT_NE(frames.find("inner failure"), std::string::npos);
}

TEST_F(ModuleFixture, ErrorAfterImportUsesMainSource)
{
    write("loaded.ف", "قيمة := 1\n");
    auto main = write("main.ف", "استورد loaded\nتاكد(خطا، \"main failure\")\n");
    VM vm;
    run_failed(run_in_vm(main, vm));
    auto json = diagnostic::engine.to_json();
    EXPECT_NE(json.find("\"path\":\"" + platform::utf8(main) + "\""), std::string::npos);
    EXPECT_EQ(json.find("loaded.ف"), std::string::npos);
}

*/
