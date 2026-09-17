#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fdiagnostic.hpp"
#include "../fairuz/flexer.hpp"
#include "../fairuz/fopcode.hpp"
#include "../fairuz/fparser.hpp"
#include "../fairuz/fvm.hpp"

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

class EnvironmentGuard {
public:
    explicit EnvironmentGuard(char const* name)
        : m_name(name)
    {
        if (char const* old = std::getenv(name))
            m_old_value = old;
        unsetenv(m_name.c_str());
    }

    EnvironmentGuard(char const* name, std::string const& value)
        : m_name(name)
    {
        if (char const* old = std::getenv(name))
            m_old_value = old;
        setenv(m_name.c_str(), value.c_str(), 1);
    }

    ~EnvironmentGuard()
    {
        if (m_old_value.has_value())
            setenv(m_name.c_str(), m_old_value->c_str(), 1);
        else
            unsetenv(m_name.c_str());
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

    Value run(std::filesystem::path const& path, VM& vm)
    {
        FileManager source(path.string());
        diagnostic::set_source(&source);
        Parser parser(&source);
        auto statements = parser.parse_program();
        EXPECT_FALSE(diagnostic::has_errors());
        Chunk* chunk = Compiler().compile(statements);
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
    FileManager source;
    source.buffer() = "من رياضيات استورد قاسم باسم gcd\n";
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
    auto* expression = AST::as_expr_stmt(statements[0])->get_expr();
    ASSERT_TRUE(AST::is_call(expression));
    auto* call = AST::as_call(expression);
    EXPECT_EQ(AST::as_name(call->get_callee())->get_value(), "تاكد");
    EXPECT_EQ(call->get_args().size(), 2u);
}

TEST(ModuleParser, ParsesImportsAliasesAndParentClass)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "استورد مجموعات باسم c\n"
                      "من نتيجة استورد نجاح باسم ok\n"
                      "نوع طفل(اصل):\n"
                      "    دالة قيمة():\n"
                      "        ارجع 1\n";
    Parser parser(&source);
    auto statements = parser.parse_program();
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(statements.size(), 3u);
    ASSERT_TRUE(AST::is_import(statements[0]));
    EXPECT_EQ(AST::as_import(statements[0])->get_module(), "مجموعات");
    EXPECT_EQ(AST::as_import(statements[0])->get_aliases()[0], "c");
    ASSERT_TRUE(AST::as_import(statements[1])->imports_member());
    EXPECT_EQ(AST::as_import(statements[1])->get_names()[0], "نجاح");
    auto* klass = AST::as_class_def(statements[2]);
    ASSERT_NE(klass->get_parent(), nullptr);
    EXPECT_EQ(AST::as_name(klass->get_parent())->get_value(), "اصل");
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
    Chunk* chunk = Compiler().compile(parser.parse_program());
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
        source.buffer() += StringRef(std::to_string(i).c_str());
        source.buffer() += "\n";
    }
    source.buffer() += "اسم299(0، 1، 1)\n";

    Parser parser(&source);
    Chunk* chunk = Compiler().compile(parser.parse_program());
    ASSERT_NE(chunk, nullptr);
    EXPECT_FALSE(diagnostic::has_errors());
    EXPECT_LT(chunk->local_count, 16u);
}

TEST(ModuleCompiler, RejectsMismatchedImportAliases)
{
    auto rejects = [](Array<StringRef> names, Array<StringRef> aliases) {
        diagnostic::reset();
        diagnostic::set_source(nullptr);
        auto* statement = AST::make_import("example", names, aliases, { });
        Compiler().compile({ statement });
        EXPECT_TRUE(diagnostic::has_errors());
        diagnostic::reset();
    };
    rejects({ }, { });
    rejects({ }, { "first", "second" });
    rejects({ "first", "second" }, { "first" });
    rejects({ "first" }, { "first", "second" });
}

TEST(ModuleCompiler, HundredsOfMembersInOneImportReuseTemporaryRegisters)
{
    diagnostic::reset();
    FileManager source;
    source.buffer() = "من مجموعات استورد ";
    for (int i = 0; i < 300; ++i) {
        if (i != 0)
            source.buffer() += "، ";
        source.buffer() += "مدى باسم اسم";
        source.buffer() += StringRef(std::to_string(i).c_str());
    }
    source.buffer() += "\nاسم299(0، 1، 1)\n";
    Parser parser(&source);
    Chunk* chunk = Compiler().compile(parser.parse_program());
    ASSERT_NE(chunk, nullptr);
    EXPECT_FALSE(diagnostic::has_errors());
    EXPECT_LT(chunk->local_count, 16u);
}

TEST_F(ModuleFixture, ChainedMethodCallsKeepArgumentsContiguous)
{
    auto path = write("chained.ف",
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

    VM vm;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run(path, vm));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST_F(ModuleFixture, LoadsOnceAndSupportsFromImport)
{
    write("counter.ف",
        "قيمة := قائمة()\n"
        "اضف(قيمة، 1)\n"
        "دالة عدد():\n"
        "    ارجع طول(قيمة)\n");
    auto main = write("main.ف",
        "استورد counter باسم اول\n"
        "استورد counter باسم ثان\n"
        "من counter استورد عدد\n"
        "تاكد(اول = ثان)\n"
        "عدد()\n");
    VM vm;
    Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 1);
}

TEST_F(ModuleFixture, ExplicitStdlibDirectoryWinsOverLocalNameCollision)
{
    auto project = directory / "project";
    auto stdlib = directory / "stdlib";
    std::filesystem::create_directories(project);
    std::filesystem::create_directories(stdlib);

    {
        std::ofstream local(project / "ملفات.ف", std::ios::binary);
        local << "دالة قيمة():\n    ارجع 1\n";
    }
    {
        std::ofstream standard(stdlib / "ملفات.ف", std::ios::binary);
        standard << "دالة قيمة():\n    ارجع 2\n";
    }
    {
        std::ofstream main(project / "main.ف", std::ios::binary);
        main << "من ملفات استورد قيمة\nقيمة()\n";
    }

    EnvironmentGuard configured_stdlib("FAIRUZ_STDLIB", stdlib.string());
    VM vm;
    Value result = run(project / "main.ف", vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST_F(ModuleFixture, BundledStdlibWinsOverLocalNameCollisionWithoutEnvironmentOverride)
{
    auto project = directory / "project";
    std::filesystem::create_directories(project);
    {
        std::ofstream local(project / "ملفات.ف", std::ios::binary);
        local << "دالة ملف(المسار، الوضع):\n    ارجع 1\n";
    }
    {
        std::ofstream main(project / "main.ف", std::ios::binary);
        main << "من ملفات استورد ملف\nملف(\"x\"، \"قراءة\").يعمل()\n";
    }

    EnvironmentGuard no_override("FAIRUZ_STDLIB");
    VM vm;
    Value result = run(project / "main.ف", vm);
    ASSERT_TRUE(result.is_bool());
    EXPECT_FALSE(result.as_bool());
}

TEST_F(ModuleFixture, KeepsModuleGlobalsIsolated)
{
    write("a.ف", "سر := 10\nدالة قيمة():\n    ارجع سر\n");
    write("b.ف", "سر := 20\nدالة قيمة():\n    ارجع سر\n");
    auto main = write("main.ف",
        "استورد a\n"
        "استورد b\n"
        "دالة الناتج():\n"
        "    ارجع a.قيمة() + b.قيمة()\n"
        "الناتج()\n");
    VM vm;
    Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 30);
}

TEST_F(ModuleFixture, BreaksImportCyclesWithPartialModules)
{
    write("a.ف", "قيمة := 7\nاستورد b\n");
    write("b.ف", "استورد a\nدالة اقرا():\n    ارجع a.قيمة\n");
    auto main = write("main.ف", "استورد b\nb.اقرا()\n");
    VM vm;
    Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 7);
}

TEST_F(ModuleFixture, ImportedFunctionsRetainDefiningEnvironment)
{
    write("base.ف", "سر := 40\nدالة زد(قيمة):\n    ارجع قيمة + سر\n");
    auto main = write("main.ف", "من base استورد زد\nسر := 1000\nزد(2)\n");
    VM vm;
    Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 42);
}

TEST_F(ModuleFixture, SingleInheritancePreservesFieldsAndOverridesMethods)
{
    write("base.ف",
        "نوع اصل:\n"
        "    دالة بداية(قيمة):\n"
        "        هذا.قيمة := قيمة\n"
        "    دالة احصل():\n"
        "        ارجع هذا.قيمة\n"
        "    دالة وصف():\n"
        "        ارجع 1\n");
    auto main = write("main.ف",
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
    VM vm;
    Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 6);
}

TEST_F(ModuleFixture, InheritedMethodRetainsParentModuleEnvironment)
{
    write("base.ف",
        "سر := 9\n"
        "نوع اصل:\n"
        "    دالة قيمة():\n"
        "        ارجع سر\n");
    auto main = write("main.ف",
        "من base استورد اصل\n"
        "سر := 99\n"
        "نوع فرع(اصل):\n"
        "    دالة اخر():\n"
        "        ارجع سر\n"
        "كائن := فرع()\n"
        "تاكد(كائن.اخر() = 99)\n"
        "كائن.قيمة()\n");
    VM vm;
    Value result = run(main, vm);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 9);
}

TEST_F(ModuleFixture, MultipleNamesInOneImportStmt)
{
    write("values.ف",
        "قيمة := 7\n"
        "دالة زد(س):\n"
        "    ارجع س + قيمة\n"
        "دالة ضاعف(س):\n"
        "    ارجع س * 2\n");
    auto main = write("main.ف",
        "من values استورد قيمة باسم عدد، زد، ضاعف باسم مرتين\n"
        "مرتين(زد(عدد))\n");
    VM vm;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run(main, vm));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 28);
}

TEST_F(ModuleFixture, WholeModuleImportsBindDefaultAndExplicitAliases)
{
    write("values.ف",
        "دالة زد(س):\n"
        "    ارجع س + 1\n");
    auto main = write("main.ف",
        "استورد values\n"
        "استورد values باسم قيم\n"
        "قيم.زد(values.زد(40))\n");
    VM vm;
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run(main, vm));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 42);
}

TEST_F(ModuleFixture, FailedModuleExecutionCanBeRetriedInSameVM)
{
    write("broken.ف", "تاكد(خطا)\n");
    auto main = write("main.ف", "استورد broken\nbroken.قيمة()\n");
    VM vm;
    EXPECT_THROW(run(main, vm), RuntimeHalt);
    write("broken.ف", "دالة قيمة():\n    ارجع 42\n");
    diagnostic::reset();
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run(main, vm));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 42);
}

TEST_F(ModuleFixture, FailedModuleParseCanBeRetriedInSameVM)
{
    write("broken.ف", "دالة قيمة():\n    ارجع (\n");
    auto main = write("main.ف", "استورد broken\nbroken.قيمة()\n");
    VM vm;
    EXPECT_THROW(run(main, vm), RuntimeHalt);
    write("broken.ف", "دالة قيمة():\n    ارجع 7\n");
    diagnostic::reset();
    Value result = Value::nil();
    ASSERT_NO_THROW(result = run(main, vm));
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
    EXPECT_THROW(run(main, vm), RuntimeHalt);
    auto json = diagnostic::engine.to_json();
    auto frames = json.substr(json.find("\"traceback\""));
    auto main_position = frames.find(main.string());
    auto outer_position = frames.find(outer.string());
    auto inner_position = frames.find(inner.string());
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
    EXPECT_THROW(run(main, vm), RuntimeHalt);
    auto json = diagnostic::engine.to_json();
    EXPECT_NE(json.find("\"path\":\"" + main.string() + "\""), std::string::npos);
    EXPECT_EQ(json.find("loaded.ف"), std::string::npos);
}

*/