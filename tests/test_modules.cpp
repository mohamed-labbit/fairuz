#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fdiagnostic.hpp"
#include "../fairuz/flexer.hpp"
#include "../fairuz/fopcode.hpp"
#include "../fairuz/fparser.hpp"
#include "../fairuz/fvm.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::lex;
using namespace fairuz::parser;
using namespace fairuz::runtime;

namespace {

class ModuleFixture : public ::testing::Test {
protected:
    std::filesystem::path directory;

    void SetUp() override
    {
        diagnostic::reset();
        directory = std::filesystem::temp_directory_path()
            / ("fairuz-modules-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
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
        auto path = directory / name;
        std::ofstream out(path, std::ios::binary);
        out << source;
        out.close();
        EXPECT_TRUE(out.good());
        return path;
    }

    Fa_Value run(std::filesystem::path const& path, Fa_VM& vm)
    {
        Fa_FileManager source(path.string());
        diagnostic::set_source(&source);
        Fa_Parser parser(&source);
        auto statements = parser.parse_program();
        EXPECT_FALSE(diagnostic::has_errors());
        Fa_Chunk* chunk = Compiler().compile(statements);
        EXPECT_NE(chunk, nullptr);
        EXPECT_FALSE(diagnostic::has_errors());
        chunk->source_path = path.string();
        return vm.run(chunk);
    }
};

} // namespace

TEST(ModuleLexer, RecognizesImportKeywords)
{
    diagnostic::reset();
    Fa_FileManager source;
    source.buffer() = "من math استورد قاسم باسم gcd\n";
    Fa_Lexer lexer(&source);
    auto tokens = lexer.tokenize();
    ASSERT_GE(tokens.size(), 8u);
    EXPECT_EQ(tokens[1]->type(), tok::Fa_TokenType::KW_FROM);
    EXPECT_EQ(tokens[3]->type(), tok::Fa_TokenType::KW_IMPORT);
    EXPECT_EQ(tokens[5]->type(), tok::Fa_TokenType::KW_AS);
}

TEST(ModuleParser, PythonStyleAssertLowersToNativeCall)
{
    diagnostic::reset();
    Fa_FileManager source;
    source.buffer() = "assert 1 = 1، \"message\"\n";
    Fa_Parser parser(&source);
    auto statements = parser.parse_program();
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(statements.size(), 1u);
    auto* expression = AST::as_expr_stmt(statements[0])->get_expr();
    ASSERT_TRUE(AST::is_call(expression));
    auto* call = AST::as_call(expression);
    EXPECT_EQ(AST::as_name(call->get_callee())->get_value(), "تاكد");
    EXPECT_EQ(call->get_args().size(), 2u);
}

TEST(ModuleParser, ParsesImportsAliasesAndParentClass)
{
    diagnostic::reset();
    Fa_FileManager source;
    source.buffer() =
        "استورد collections باسم c\n"
        "من result استورد نجاح باسم ok\n"
        "نوع طفل(اصل):\n"
        "    دالة قيمة():\n"
        "        ارجع 1\n";
    Fa_Parser parser(&source);
    auto statements = parser.parse_program();
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(statements.size(), 3u);
    ASSERT_TRUE(AST::is_import(statements[0]));
    EXPECT_EQ(AST::as_import(statements[0])->get_module(), "collections");
    EXPECT_EQ(AST::as_import(statements[0])->get_alias(), "c");
    ASSERT_TRUE(AST::as_import(statements[1])->imports_member());
    EXPECT_EQ(AST::as_import(statements[1])->get_name(), "نجاح");
    auto* klass = AST::as_class_def(statements[2]);
    ASSERT_NE(klass->get_parent(), nullptr);
    EXPECT_EQ(AST::as_name(klass->get_parent())->get_value(), "اصل");
}

TEST(ModuleCompiler, EmitsImportAndParentDescriptor)
{
    diagnostic::reset();
    Fa_FileManager source;
    source.buffer() =
        "استورد collections\n"
        "نوع طفل(اصل):\n"
        "    دالة قيمة():\n"
        "        ارجع 1\n";
    Fa_Parser parser(&source);
    Fa_Chunk* chunk = Compiler().compile(parser.parse_program());
    ASSERT_NE(chunk, nullptr);
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_FALSE(chunk->code.empty());
    EXPECT_EQ(Fa_instr_op(chunk->code[0]), Fa_OpCode::IMPORT_MODULE);
    ASSERT_EQ(chunk->class_descriptors.size(), 1u);
    EXPECT_EQ(chunk->class_descriptors[0].parent_name, "اصل");
}

TEST_F(ModuleFixture, ChainedMethodCallsKeepArgumentsContiguous)
{
    auto path = write("chained.fa",
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
        "اختبر()\n");

    Fa_VM vm;
    Fa_Value result = Fa_Value::nil();
    ASSERT_NO_THROW(result = run(path, vm));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST_F(ModuleFixture, LoadsOnceAndSupportsFromImport)
{
    write("counter.fa",
        "قيمة := قائمة()\n"
        "اضف(قيمة، 1)\n"
        "دالة عدد():\n"
        "    ارجع طول(قيمة)\n");
    auto main = write("main.fa",
        "استورد counter باسم اول\n"
        "استورد counter باسم ثان\n"
        "من counter استورد عدد\n"
        "تاكد(اول = ثان)\n"
        "عدد()\n");
    Fa_VM vm;
    Fa_Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 1);
}

TEST_F(ModuleFixture, KeepsModuleGlobalsIsolated)
{
    write("a.fa", "سر := 10\nدالة قيمة():\n    ارجع سر\n");
    write("b.fa", "سر := 20\nدالة قيمة():\n    ارجع سر\n");
    auto main = write("main.fa",
        "استورد a\n"
        "استورد b\n"
        "دالة الناتج():\n"
        "    ارجع a.قيمة() + b.قيمة()\n"
        "الناتج()\n");
    Fa_VM vm;
    Fa_Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 30);
}

TEST_F(ModuleFixture, BreaksImportCyclesWithPartialModules)
{
    write("a.fa", "قيمة := 7\nاستورد b\n");
    write("b.fa", "استورد a\nدالة اقرا():\n    ارجع a.قيمة\n");
    auto main = write("main.fa", "استورد b\nb.اقرا()\n");
    Fa_VM vm;
    Fa_Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST_F(ModuleFixture, ImportedFunctionsRetainDefiningEnvironment)
{
    write("base.fa", "سر := 40\nدالة زد(قيمة):\n    ارجع قيمة + سر\n");
    auto main = write("main.fa", "من base استورد زد\nسر := 1000\nزد(2)\n");
    Fa_VM vm;
    Fa_Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 42);
}

TEST_F(ModuleFixture, SingleInheritancePreservesFieldsAndOverridesMethods)
{
    write("base.fa",
        "نوع اصل:\n"
        "    دالة بداية(قيمة):\n"
        "        هذا.قيمة := قيمة\n"
        "    دالة احصل():\n"
        "        ارجع هذا.قيمة\n"
        "    دالة وصف():\n"
        "        ارجع 1\n");
    auto main = write("main.fa",
        "من base استورد اصل\n"
        "نوع فرع(اصل):\n"
        "    دالة وصف():\n"
        "        ارجع 2\n"
        "    دالة ضاعف():\n"
        "        ارجع هذا.قيمة * 2\n"
        "كائن := فرع(3)\n"
        "تاكد(كائن.احصل() = 3)\n"
        "تاكد(كائن.وصف() = 2)\n"
        "كائن.ضاعف()\n");
    Fa_VM vm;
    Fa_Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 6);
}

TEST_F(ModuleFixture, InheritedMethodRetainsParentModuleEnvironment)
{
    write("base.fa",
        "سر := 9\n"
        "نوع اصل:\n"
        "    دالة قيمة():\n"
        "        ارجع سر\n");
    auto main = write("main.fa",
        "من base استورد اصل\n"
        "سر := 99\n"
        "نوع فرع(اصل):\n"
        "    دالة اخر():\n"
        "        ارجع سر\n"
        "كائن := فرع()\n"
        "تاكد(كائن.اخر() = 99)\n"
        "كائن.قيمة()\n");
    Fa_VM vm;
    Fa_Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 9);
}
