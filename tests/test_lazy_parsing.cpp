#include "../fairuz/fASTBuilder.hpp"
#include "../fairuz/fcompiler.hpp"
#include "../fairuz/fparser.hpp"
#include "../fairuz/fvm.hpp"
#include "test_common.h"

#include <gtest/gtest.h>

namespace {

class LazyParsing : public ::testing::Test {
protected:
    void SetUp() override { diagnostic::reset(); }
    void TearDown() override
    {
        diagnostic::reset();
        diagnostic::set_source(nullptr);
    }

    Array<StmtPtr> parse(char const* text, parser::BodyParsing mode = parser::BodyParsing::Lazy)
    {
        lex::FileManager file;
        file.buffer() = text;
        parser::Parser parser(&file, mode);
        return parser.parse_program();
    }
};

TEST_F(LazyParsing, HeadersAreParsedAndUnusedBodiesHaveNoStatementTree)
{
    auto program = parse("دالة جمع(س، ص):\n    ارجع س + ص\nدالة خطأ(): ارجع +\n42\n");
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(program.size(), 3u);
    auto* function = as_function_def(program[0]);
    ASSERT_EQ(function->params.size(), 2u);
    EXPECT_EQ(as_identifier(function->params[0])->spelling, "س");
    ASSERT_TRUE(is_function_stub(function->body));
    auto* stub = as_function_stub(function->body);
    EXPECT_EQ(stub->parsed_body, nullptr);
    EXPECT_NE(std::string(stub->body_view.data(), stub->body_view.len()).find("ارجع س + ص"), std::string::npos);
    ASSERT_NE(stub->source, nullptr);
    EXPECT_EQ(stub->tokens.back()->type(), tok::TokenType::ENDMARKER);
    EXPECT_TRUE(is_function_stub(as_function_def(program[1])->body));
    Chunk* chunk = Compiler().compile(program);
    VM vm;
    EXPECT_NO_THROW(vm.run(chunk));
    EXPECT_EQ(stub->parsed_body, nullptr);
    EXPECT_TRUE(chunk->functions[0]->code.empty());
}

TEST_F(LazyParsing, FirstCallMaterializesOnceAfterParserAndSourceDestruction)
{
    auto program = parse("دالة جمع(س، ص):\n    ارجع س + ص\nجمع(20، 22)\n");
    ASSERT_FALSE(diagnostic::has_errors());
    auto* stub = as_function_stub(as_function_def(program[0])->body);
    Chunk* chunk = Compiler().compile(program);
    EXPECT_EQ(stub->parsed_body, nullptr);
    VM vm;
    EXPECT_EQ(vm.run(chunk).as_int(), 42);
    ASSERT_NE(stub->parsed_body, nullptr);
    auto* body = stub->parsed_body;
    EXPECT_EQ(as_block(body)->stmts[0]->get_location().line, 2u);
    EXPECT_EQ(vm.run(chunk).as_int(), 42);
    EXPECT_EQ(stub->parsed_body, body);
    ASTBuilder builder;
    auto repeated = builder.materialize(*stub);
    ASSERT_TRUE(repeated.has_value());
    EXPECT_EQ(repeated.value(), body);
}

TEST_F(LazyParsing, FailedBodyParsingIsNotCachedAndReportsOriginalSource)
{
    auto program = parse("دالة خطأ():\n    ارجع +\nخطأ()\n");
    ASSERT_FALSE(diagnostic::has_errors());
    auto* stub = as_function_stub(as_function_def(program[0])->body);
    Chunk* chunk = Compiler().compile(program);
    VM vm;
    for (int attempt = 0; attempt < 2; ++attempt) {
        diagnostic::reset();
        EXPECT_THROW(vm.run(chunk), RuntimeHalt);
        EXPECT_TRUE(diagnostic::has_errors());
        EXPECT_EQ(stub->parsed_body, nullptr);
        EXPECT_TRUE(chunk->functions[0]->deferred_body);
        EXPECT_TRUE(chunk->functions[0]->code.empty());
        auto json = diagnostic::engine.to_json();
        EXPECT_NE(json.find("ارجع +"), std::string::npos);
        EXPECT_NE(json.find("\"line\":2"), std::string::npos);
    }
}

TEST_F(LazyParsing, ExplicitValidationFindsUnusedSyntaxErrors)
{
    auto program = parse("دالة خطأ(): ارجع +\n42\n");
    ASSERT_FALSE(diagnostic::has_errors());
    Chunk* chunk = Compiler().compile(program);
    EXPECT_FALSE(Compiler::compile_all(chunk));
    EXPECT_TRUE(diagnostic::has_errors());
}

TEST_F(LazyParsing, EagerModeBuildsBodiesAndReportsSyntaxErrorsImmediately)
{
    auto program = parse("دالة قيمة(): ارجع 42\n", parser::BodyParsing::Eager);
    ASSERT_FALSE(diagnostic::has_errors());
    ASSERT_EQ(program.size(), 1u);
    EXPECT_TRUE(is_block(as_function_def(program[0])->body));
    parse("دالة خطأ(): ارجع +\n", parser::BodyParsing::Eager);
    EXPECT_TRUE(diagnostic::has_errors());
}

TEST_F(LazyParsing, HeadersAndMissingSuitesAreStillValidatedImmediately)
{
    for (auto* text : { "دالة قيمة() ارجع 1\n", "دالة قيمة(1): ارجع 1\n",
             "دالة قيمة():\n42\n", "دالة قيمة():" }) {
        SCOPED_TRACE(text);
        diagnostic::reset();
        parse(text);
        EXPECT_TRUE(diagnostic::has_errors());
    }
}

TEST_F(LazyParsing, InlineAndIndentedSuitesPreserveFollowingDefinitions)
{
    for (auto* text : {
             "دالة قيمة(): ارجع 42\nدالة اخر(): ارجع 0\nقيمة()",
             "دالة قيمة():\n    # comment\n\n    اذا صحيح:\n        ارجع 42\n    غيره:\n        ارجع 0\nدالة اخر(): ارجع 0\nقيمة()\n",
             "دالة قيمة(): اذا صحيح: ارجع 42\nدالة اخر(): ارجع 0\nقيمة()\n",
             "دالة قيمة(): اذا صحيح:\n    ارجع 42\nدالة اخر(): ارجع 0\nقيمة()\n",
             "دالة قيمة(): ارجع (\n    20 +\n    22\n)\nدالة اخر(): ارجع 0\nقيمة()\n" }) {
        SCOPED_TRACE(text);
        diagnostic::reset();
        auto program = parse(text);
        ASSERT_FALSE(diagnostic::has_errors());
        ASSERT_EQ(program.size(), 3u);
        Chunk* chunk = Compiler().compile(program);
        VM vm;
        EXPECT_EQ(vm.run(chunk).as_int(), 42);
        EXPECT_EQ(as_function_stub(as_function_def(program[1])->body)->parsed_body, nullptr);
    }
}

TEST_F(LazyParsing, EndOfFileAndNestedDefinitionsPreserveReplayBoundaries)
{
    for (auto* ending : { "", "\n", "\n\n" }) {
        diagnostic::reset();
        std::string text = "دالة خارج():\n    دالة داخل(): ارجع 42";
        text += ending;
        auto program = parse(text.c_str());
        ASSERT_FALSE(diagnostic::has_errors());
        ASSERT_EQ(program.size(), 1u);
        auto* outer = as_function_stub(as_function_def(program[0])->body);
        ASTBuilder builder;
        auto body = builder.materialize(*outer);
        ASSERT_TRUE(body.has_value());
        ASSERT_EQ(as_block(body.value())->stmts.size(), 1u);
        auto* inner = as_function_stub(as_function_def(as_block(body.value())->stmts[0])->body);
        auto inner_body = builder.materialize(*inner);
        ASSERT_TRUE(inner_body.has_value());
        EXPECT_EQ(as_block(inner_body.value())->stmts[0]->get_location().line, 2u);
        lex::Lexer replay(inner->tokens, inner->source);
        while (!replay.current()->is(tok::TokenType::ENDMARKER))
            replay.next();
        EXPECT_EQ(replay.next(), replay.current());
        EXPECT_EQ(replay.peek(), replay.current());
    }
}

TEST_F(LazyParsing, StubClonesKeepTheirNodeKindAndSource)
{
    auto program = parse("دالة قيمة(): ارجع 42\n");
    auto* function = as_function_def(program[0]);
    auto* copy = function->clone();
    ASSERT_TRUE(is_func(copy));
    ASSERT_TRUE(is_function_stub(copy->body));
    EXPECT_FALSE(is_func(copy->body));
    EXPECT_TRUE(function->body->equals(copy->body));
    EXPECT_FALSE(function->body->equals(function));
    EXPECT_FALSE(function->equals(function->body));
    ASTBuilder builder;
    EXPECT_TRUE(builder.materialize(*as_function_stub(copy->body)).has_value());
    EXPECT_EQ(as_function_stub(function->body)->parsed_body, nullptr);
}

TEST_F(LazyParsing, MethodsRemainEagerForFieldDiscovery)
{
    auto program = parse("نوع صندوق:\n    دالة بداية(ق): .قيمة := ق\n    دالة اقرأ(): ارجع هذا.قيمة\nصندوق(42).اقرأ()\n");
    ASSERT_FALSE(diagnostic::has_errors());
    auto* klass = as_class_def(program[0]);
    ASSERT_EQ(klass->members.size(), 1u);
    EXPECT_TRUE(is_block(as_function_def(klass->methods[0])->body));
    Chunk* chunk = Compiler().compile(program);
    VM vm;
    EXPECT_EQ(vm.run(chunk).as_int(), 42);
}

} // namespace
