//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies syntax parsing against independently constructed expected ASTs.
//===----------------------------------------------------------------------===//

#include "Parser/ast.hpp"
#include "Parser/ast_builder.hpp"
#include "Parser/ast_printer.hpp"
#include "Parser/parser.hpp"
#include "Support/array.hpp"
#include "test_config.h"
#include "test_runtime.hpp"

#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <utility>

using namespace fairuz;
using namespace fairuz::parser;
using namespace fairuz::lex;
using EK = AST::ExprKind;

namespace {

std::filesystem::path parser_test_cases_dir()
{
    static auto const dir = std::filesystem::path(__FILE__).parent_path() / "test_cases";
    return dir;
}

} // namespace

class ParserTest : public ::testing::Test {
protected:
    FileManager file_manager;
    AST::ASTBuilder ast_builder;

    AST::IdentifierExpr* id(StringRef name) { return ast_builder.make_identifier(name, { }); }
    AST::ExprPtr integer(i64 value) { return ast_builder.make_literal_int(value, { }); }
    AST::ExprPtr real(double value) { return ast_builder.make_literal_float(value, { }); }
    AST::ExprPtr text(StringRef value) { return ast_builder.make_literal_string(value, { }); }
    AST::ExprPtr boolean(bool value) { return ast_builder.make_literal_bool(value, { }); }
    AST::ExprPtr nil() { return ast_builder.make_nil({ }); }
    AST::ExprPtr binary(EK op, AST::ExprPtr lhs, AST::ExprPtr rhs)
    {
        return ast_builder.make_binary(op, lhs, rhs, { });
    }
    AST::ExprPtr unary(EK op, AST::ExprPtr operand) { return ast_builder.make_unary(op, operand, { }); }
    AST::ExprPtr list(Array<AST::ExprPtr> elements) { return ast_builder.make_list(std::move(elements), { }); }
    AST::ExprPtr dict(Array<std::pair<AST::ExprPtr, AST::ExprPtr>> entries)
    {
        return ast_builder.make_dict(std::move(entries), { });
    }
    AST::ExprPtr call(AST::ExprPtr callee, Array<AST::ExprPtr> args = { })
    {
        return ast_builder.make_call(callee, std::move(args), { });
    }
    AST::ExprPtr get(AST::ExprPtr object, StringRef member)
    {
        return ast_builder.make_get_expr(object, id(member), { });
    }
    AST::ExprPtr field(StringRef member) { return get(id("__class$instance"), member); }
    AST::ExprPtr index(AST::ExprPtr object, AST::ExprPtr subscript)
    {
        return ast_builder.make_index(object, subscript, { });
    }
    AST::ExprPtr assignment(AST::ExprPtr target, AST::ExprPtr value)
    {
        return ast_builder.make_assignment_expr(target, value, { });
    }
    AST::StmtPtr expr(AST::ExprPtr value) { return ast_builder.make_expr_stmt(value, { }); }
    AST::StmtPtr assign(AST::ExprPtr target, AST::ExprPtr value)
    {
        return ast_builder.make_assignment_stmt(target, value, { });
    }
    AST::StmtPtr augmented(AST::ExprPtr target, EK op, AST::ExprPtr rhs)
    {
        auto* result = ast_builder.make_assignment_expr(target, binary(op, target, rhs), { });
        result->augmented = true;
        return expr(result);
    }
    AST::StmtPtr block(Array<AST::StmtPtr> statements) { return ast_builder.make_block(std::move(statements), { }); }
    AST::StmtPtr branch(AST::ExprPtr condition, AST::StmtPtr then_body, AST::StmtPtr else_body = nullptr)
    {
        return ast_builder.make_if(condition, then_body, { }, else_body);
    }
    AST::StmtPtr while_loop(AST::ExprPtr condition, AST::StmtPtr body)
    {
        return ast_builder.make_while(condition, body, { });
    }
    AST::StmtPtr for_loop(StringRef name, AST::ExprPtr iterable, AST::StmtPtr body)
    {
        return ast_builder.make_for(id(name), iterable, body, { });
    }
    AST::StmtPtr function(StringRef name, Array<AST::ExprPtr> params, AST::StmtPtr body)
    {
        return ast_builder.make_function(id(name), std::move(params), body, { });
    }
    AST::StmtPtr returns(AST::ExprPtr value = nullptr) { return ast_builder.make_return({ }, value); }
    AST::StmtPtr klass(
        StringRef name, Array<AST::ExprPtr> members, Array<AST::StmtPtr> methods, AST::ExprPtr parent = nullptr)
    {
        return ast_builder.make_class_def(id(name), parent, std::move(members), std::move(methods), { });
    }
    AST::StmtPtr import_stmt(StringRef module, Array<StringRef> names, Array<StringRef> aliases)
    {
        return ast_builder.make_import(module, std::move(names), std::move(aliases), { });
    }
    AST::StmtPtr breaks() { return ast_builder.make_break({ }); }
    AST::StmtPtr continues() { return ast_builder.make_continue({ }); }

    // Production function equality means "same name", not identical syntax.
    // Compare compound statements recursively here so parameters, bodies, and
    // absent else branches participate without changing production semantics.
    void expect_stmt(AST::StmtPtr actual, AST::StmtPtr expected)
    {
        if (actual == nullptr || expected == nullptr) {
            ASSERT_EQ(actual, expected);
            return;
        }
        ASSERT_EQ(actual->get_kind(), expected->get_kind());
        switch (expected->get_kind()) {
        case AST::Stmt::Kind::BLOCK: expect_stmts(AST::as_block(actual)->stmts, AST::as_block(expected)->stmts); break;
        case AST::Stmt::Kind::FUNC: {
            auto* a = AST::as_function_def(actual);
            auto* e = AST::as_function_def(expected);
            EXPECT_TRUE(a->name->equals(e->name));
            expect_exprs(a->params, e->params);
            expect_stmt(a->body, e->body);
            break;
        }
        case AST::Stmt::Kind::CLASS_DEF: {
            auto* a = AST::as_class_def(actual);
            auto* e = AST::as_class_def(expected);
            EXPECT_TRUE(a->name->equals(e->name));
            if (a->parent == nullptr || e->parent == nullptr) EXPECT_EQ(a->parent, e->parent);
            else EXPECT_TRUE(a->parent->equals(e->parent));
            expect_exprs(a->members, e->members);
            expect_stmts(a->methods, e->methods);
            break;
        }
        case AST::Stmt::Kind::IF: {
            auto* a = AST::as_if(actual);
            auto* e = AST::as_if(expected);
            EXPECT_TRUE(a->condition->equals(e->condition));
            expect_stmt(a->then_stmt, e->then_stmt);
            expect_stmt(a->else_stmt, e->else_stmt);
            break;
        }
        case AST::Stmt::Kind::WHILE: {
            auto* a = AST::as_while(actual);
            auto* e = AST::as_while(expected);
            EXPECT_TRUE(a->condition->equals(e->condition));
            expect_stmt(a->body, e->body);
            break;
        }
        case AST::Stmt::Kind::FOR: {
            auto* a = AST::as_for(actual);
            auto* e = AST::as_for(expected);
            EXPECT_TRUE(a->container->equals(e->container));
            EXPECT_TRUE(a->iter->equals(e->iter));
            expect_stmt(a->body, e->body);
            break;
        }
        default: EXPECT_TRUE(actual->equals(expected)); break;
        }
    }

    void expect_exprs(Array<AST::ExprPtr> const& actual, Array<AST::ExprPtr> const& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (u32 i = 0; i < actual.size(); ++i) {
            SCOPED_TRACE("expression " + std::to_string(i));
            ASSERT_NE(actual[i], nullptr);
            ASSERT_NE(expected[i], nullptr);
            EXPECT_TRUE(actual[i]->equals(expected[i]));
        }
    }

    void expect_stmts(Array<AST::StmtPtr> const& actual, Array<AST::StmtPtr> const& expected)
    {
        ASSERT_EQ(actual.size(), expected.size());
        for (u32 i = 0; i < actual.size(); ++i) {
            SCOPED_TRACE("statement " + std::to_string(i));
            expect_stmt(actual[i], expected[i]);
        }
    }

    void compare_program(Array<AST::StmtPtr> const& expected)
    {
        diagnostic::SourceScope source_scope(&file_manager);
        Parser parser(&file_manager, BodyParsing::EAGER);
        auto actual = parser.parse_program();
        ASSERT_FALSE(diagnostic::has_errors());
        if (test_config::print_ast) {
            AST::ASTPrinter printer;
            for (auto* statement : actual) printer.print(statement);
        }
        expect_stmts(actual, expected);
    }

    void test_stmts(char const* filepath, Array<AST::StmtPtr> expected)
    {
        SCOPED_TRACE(filepath);
        diagnostic::reset();
        file_manager.load(parser_test_cases_dir() / filepath, true);
        compare_program(expected);
    }

    void test_source(char const* source, Array<AST::StmtPtr> expected)
    {
        SCOPED_TRACE(source);
        diagnostic::reset();
        file_manager.buffer() = source;
        compare_program(expected);
    }

    void test_error(char const* filepath, ErrorCode code, bool lexical_abort = false)
    {
        SCOPED_TRACE(filepath);
        diagnostic::reset();
        file_manager.load(parser_test_cases_dir() / filepath, true);
        diagnostic::SourceScope source_scope(&file_manager);
        auto parse = [&] {
            Parser parser(&file_manager, BodyParsing::EAGER);
            (void)parser.parse_program();
        };
        if (lexical_abort) EXPECT_THROW(parse(), diagnostic::DiagnosticAbort);
        else EXPECT_NO_THROW(parse());
        auto const* error = diagnostic::engine.first_error();
        ASSERT_NE(error, nullptr);
        EXPECT_EQ(error->err_code, code);
        EXPECT_GT(error->src_loc.line, 0u);
        EXPECT_GT(error->src_loc.column, 0u);
    }

public:
    void SetUp() override
    {
        diagnostic::reset();
        ASSERT_TRUE(std::filesystem::exists(parser_test_cases_dir()));
    }

    void TearDown() override { diagnostic::reset(); }
};

TEST_F(ParserTest, InlineMethodsPreserveClassAndStatementBoundaries)
{
    test_source("نوع اول:\n"
                "    دالة بداية(): هذا.قيمة := 1\n"
                "    # A comment between inline methods.\n\n"
                "    دالة اقرأ(): ارجع هذا.قيمة\n"
                "    دالة متعدد():\n"
                "        ارجع 2\n"
                "    دالة اخير(): ارجع 3\n\n"
                "نوع ثاني:\n"
                "    دالة اقرأ():\n"
                "        ارجع 4\n"
                "دالة مستقل(): ارجع 5\n"
                "نتيجة := مستقل()\n",
        { klass("اول", { id("قيمة") },
              { function("بداية", { }, block({ assign(field("قيمة"), integer(1)) })),
                  function("اقرأ", { }, block({ returns(field("قيمة")) })),
                  function("متعدد", { }, block({ returns(integer(2)) })),
                  function("اخير", { }, block({ returns(integer(3)) })) }),
            klass("ثاني", { }, { function("اقرأ", { }, block({ returns(integer(4)) })) }),
            function("مستقل", { }, block({ returns(integer(5)) })), assign(id("نتيجة"), call(id("مستقل"))) });
}

TEST_F(ParserTest, InlineMethodAtEndOfFilePreservesEnclosingDedents)
{
    for (char const* ending : { "", "\n", "\n\n" }) {
        auto source = std::string("دالة مستقل():\n"
                                  "    نوع مثال:\n"
                                  "        دالة قيمة(): ارجع 1")
            + ending;
        test_source(source.c_str(),
            { function("مستقل", { },
                block({ klass("مثال", { }, { function("قيمة", { }, block({ returns(integer(1)) })) }) })) });
    }
}

TEST_F(ParserTest, InlineBlocksLeaveFollowingStatementsOutside)
{
    struct Case {
        char const* source;
        AST::StmtPtr first;
    };
    for (auto const& test :
        { Case { "اذا صحيح: س := 1\nص := 2\n", branch(boolean(true), block({ assign(id("س"), integer(1)) })) },
            Case { "اذا خطا: س := 1\nغيره اذا صحيح: س := 2\nغيره: س := 3\nص := 2\n",
                branch(boolean(false), block({ assign(id("س"), integer(1)) }),
                    branch(boolean(true), block({ assign(id("س"), integer(2)) }),
                        block({ assign(id("س"), integer(3)) }))) },
            Case { "طالما صحيح: اخرج\nص := 2\n", while_loop(boolean(true), block({ breaks() })) },
            Case { "لكل س في [1]: اكمل\nص := 2\n", for_loop("س", list({ integer(1) }), block({ continues() })) },
            Case { "دالة مثال(): ارجع 1\nص := 2\n", function("مثال", { }, block({ returns(integer(1)) })) },
            Case { "نوع مثال: دالة قيمة(): ارجع 1\nص := 2\n",
                klass("مثال", { }, { function("قيمة", { }, block({ returns(integer(1)) })) }) },
            Case { "نوع مثال: دالة بداية(): .قيمة := 1\nص := 2\n",
                klass("مثال", { id("قيمة") },
                    { function("بداية", { }, block({ assign(field("قيمة"), integer(1)) })) }) } }) {
        test_source(test.source, { test.first, assign(id("ص"), integer(2)) });
    }
}

TEST_F(ParserTest, InlineBlocksRejectMissingBodies)
{
    for (char const* header : { "اذا صحيح:", "طالما صحيح:", "لكل س في [1]:", "دالة مثال():", "نوع مثال:" }) {
        for (char const* ending : { "", " # no body", "\nس := 1\n", "\n    # no body\nس := 1\n" }) {
            auto source = std::string(header) + ending;
            SCOPED_TRACE(source);
            diagnostic::reset();
            file_manager.buffer() = source.c_str();
            Parser parser(&file_manager, BodyParsing::EAGER);
            (void)parser.parse_program();
            EXPECT_TRUE(diagnostic::has_errors());
        }
    }
}

TEST_F(ParserTest, InlineBareReturnAtEndOfFile)
{
    struct Case {
        char const* source;
        AST::StmtPtr expected;
    };
    for (auto const& test : { Case { "دالة مثال(): ارجع", function("مثال", { }, block({ returns() })) },
             Case { "نوع مثال:\n    دالة قيمة(): ارجع",
                 klass("مثال", { }, { function("قيمة", { }, block({ returns() })) }) },
             Case { "دالة مثال():\n    اذا صحيح: ارجع",
                 function("مثال", { }, block({ branch(boolean(true), block({ returns() })) })) },
             Case { "نوع مثال: دالة قيمة(): ارجع",
                 klass("مثال", { }, { function("قيمة", { }, block({ returns() })) }) } }) {
        test_source(test.source, { test.expected });
    }
}

TEST_F(ParserTest, InlineBodiesRejectExtraStatementsOnTheSameLine)
{
    for (char const* source : { "دالة مثال(): ارجع 1 ارجع 2\n", "طالما صحيح: اخرج اكمل\n", "لكل س في [1]: اكمل اخرج\n",
             "اذا صحيح: تاكد صحيح س := 1\n", "نوع مثال: دالة بداية(): .قيمة := 1 ارجع 2\n" }) {
        SCOPED_TRACE(source);
        diagnostic::reset();
        file_manager.buffer() = source;
        Parser parser(&file_manager, BodyParsing::EAGER);
        (void)parser.parse_program();
        EXPECT_TRUE(diagnostic::has_errors());
    }
}
class ParserAssignmentContext : public ::testing::TestWithParam<char const*> {
protected:
    void SetUp() override { diagnostic::reset(); }
    void TearDown() override { diagnostic::reset(); }
};

TEST_P(ParserAssignmentContext, RejectsAssignmentInValueContext)
{
    SCOPED_TRACE(GetParam());
    FileManager source;
    source.buffer() = GetParam();
    Parser parser(&source, BodyParsing::EAGER);
    auto result = parser.parse_statement();
    // Class/block recovery can return a partial AST; recorded errors still
    // prevent compilation. Check the diagnostic as well as propagated errors.
    ASSERT_TRUE(diagnostic::has_errors());
    EXPECT_NE(diagnostic::engine.to_json().find("Assignment is a statement and cannot be used inside an expression"),
        std::string::npos);
    if (result.has_error()) EXPECT_EQ(result.error().get_code(), ErrorCode::ASSIGNMENT_IN_EXPRESSION);
}

INSTANTIATE_TEST_SUITE_P(NestedAssignment, ParserAssignmentContext,
    testing::Values("س + (س := 2)\n", "(س := 2) + 1\n", "س := (ص := 2)\n", "س := ص += 2\n", "س += ص := 2\n",
        "س += (ص := 2)\n", "(س := 2)\n", "ليس (س := 2)\n", "صحيح او (س := 2)\n", "خطا و (س := 2)\n", "اكتب(س := 2)\n",
        "اكتب((س := 2))\n", "اكتب(1، س := 2)\n", "[س := 2]\n", "(1، س := 2)\n", "{س := 2: 3}\n", "{1: س := 2}\n",
        "ق[س := 2]\n", "ق[(س := 2)] := 3\n", "(س := ق)[0] := 3\n", "(س := ق).حقل := 3\n", "(س := دالة_اخرى)()\n",
        "ارجع س := 2\n",
        "اذا س := 2:\n"
        "    اكتب(س)\n",
        "طالما س := 2:\n"
        "    اخرج\n",
        "لكل س في ق := [1]:\n"
        "    اكتب(س)\n",
        "assert س := 2\n", "assert صحيح، س := 2\n",
        "نوع علبة:\n"
        "    دالة بداية():\n"
        "        .حقل := (س := 2)\n",
        "نوع علبة:\n"
        "    دالة بداية():\n"
        "        .حقل += س := 2\n"));

TEST_F(ParserTest, AssignmentContextExpressionEntryPointRejectsAssignment)
{
    FileManager source;
    source.buffer() = "س := 2";
    Parser parser(&source, BodyParsing::EAGER);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().get_code(), ErrorCode::ASSIGNMENT_IN_EXPRESSION);
    EXPECT_EQ(parser.current_token()->lexeme(), ":=");
}

TEST_F(ParserTest, AssignmentContextPreservesStatementsAndEquality)
{
    test_source("س := 1 + (2 * 3)\n",
        { assign(id("س"), binary(EK::OP_ADD, integer(1), binary(EK::OP_MUL, integer(2), integer(3)))) });
    test_source("س := ص := 5\n", { assign(id("س"), assignment(id("ص"), integer(5))) });
    test_source("ق[فهرس()] := قيمة()\n", { assign(index(id("ق"), call(id("فهرس"))), call(id("قيمة"))) });
    test_source("س.حقل := 2\n", { assign(get(id("س"), "حقل"), integer(2)) });
    test_source("اكتب(س = 2)\n", { expr(call(id("اكتب"), { binary(EK::OP_EQ, id("س"), integer(2)) })) });
    test_source("اذا س = 2:\n    ص := 3\n",
        { branch(binary(EK::OP_EQ, id("س"), integer(2)), block({ assign(id("ص"), integer(3)) })) });
    test_source("دالة مثال():\n    س := 2\n    ارجع س + 1\n",
        { function(
            "مثال", { }, block({ assign(id("س"), integer(2)), returns(binary(EK::OP_ADD, id("س"), integer(1))) })) });
    test_source("نوع علبة:\n    دالة بداية():\n        .حقل := 2\n        .حقل += 3\n",
        { klass("علبة", { id("حقل") },
            { function("بداية", { },
                block({ assign(field("حقل"), integer(2)), augmented(field("حقل"), EK::OP_ADD, integer(3)) })) }) });
}

TEST_F(ParserTest, AssignmentContextAugmentedOperatorsAreStatementOnly)
{
    struct Case {
        char const* spelling;
        EK operation;
    };
    for (auto const& test :
        { Case { "+=", EK::OP_ADD }, Case { "-=", EK::OP_SUB }, Case { "*=", EK::OP_MUL }, Case { "/=", EK::OP_DIV },
            Case { "%=", EK::OP_MOD }, Case { "&=", EK::OP_BITAND }, Case { "|=", EK::OP_BITOR },
            Case { "^=", EK::OP_BITXOR }, Case { "<<=", EK::OP_LSHIFT }, Case { ">>=", EK::OP_RSHIFT } }) {
        auto source = std::string("س ") + test.spelling + " 1";
        test_source(source.c_str(), { augmented(id("س"), test.operation, integer(1)) });

        diagnostic::reset();
        auto nested = "اكتب(" + source + ")";
        file_manager.buffer() = nested.c_str();
        Parser parser(&file_manager, BodyParsing::EAGER);
        auto result = parser.parse_statement();
        ASSERT_TRUE(result.has_error());
        EXPECT_EQ(result.error().get_code(), ErrorCode::ASSIGNMENT_IN_EXPRESSION);
    }
}

TEST_F(ParserTest, ParseLiteral)
{
    test_stmts("number_literal.fa", { expr(integer(42)) });
    test_stmts("string_literal.fa", { expr(text("مزاح")) });
    test_stmts("boolean_literal_true.fa", { expr(boolean(true)) });
    test_stmts("boolean_literal_false.fa", { expr(boolean(false)) });
}

TEST_F(ParserTest, DecimalLiteralsRoundOnce)
{
    struct Case {
        char const* source;
        double expected;
    };
    for (auto const& test : { Case { "9223372036854775808.0", 0x1p63 }, Case { "9_223_372_036_854_775_808.0", 0x1p63 },
             Case { "0.1", 0.1 }, Case { "0.1234567890123456789", 0.1234567890123456789 } }) {
        test_source(test.source, { expr(real(test.expected)) });
    }
}

TEST_F(ParserTest, OutOfRangeDecimalDoesNotBecomeInteger)
{
    std::string source(400, '9');
    source += ".0";
    file_manager.buffer() = source.c_str();
    Parser parser(&file_manager, BodyParsing::EAGER);
    auto result = parser.parse_expression();
    ASSERT_TRUE(result.has_error());
    EXPECT_EQ(result.error().get_code(), ErrorCode::INVALID_NUMBER_LITERAL);
    EXPECT_TRUE(diagnostic::has_errors());
}

TEST_F(ParserTest, ParseNil) { test_stmts("none_literal.fa", { expr(nil()) }); }

TEST_F(ParserTest, ParseParenthesizedNumberLiteral) { test_stmts("parenthesized_number.fa", { expr(integer(5)) }); }

TEST_F(ParserTest, ParseIdentifier) { test_stmts("identifier.fa", { expr(id("المتنبي")) }); }

TEST_F(ParserTest, ParseCallExpressionNoArgs) { test_stmts("call_expression.fa", { expr(call(id("اطبع"))) }); }

TEST_F(ParserTest, ParseCallExpressionWithOneArg)
{
    test_stmts("call_expression_with_one_argument.fa", { expr(call(id("اطبع"), { id("المتنبي") })) });
}

TEST_F(ParserTest, ParseNestedCallExpression)
{
    test_stmts("nested_call_expression.fa", { expr(call(id("ا"), { call(id("ب"), { id("د") }) })) });
}

TEST_F(ParserTest, ParseSimpleAddition)
{
    test_stmts("simple_addition.fa", { expr(binary(EK::OP_ADD, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseSimpleMultiplication)
{
    test_stmts("simple_multiplication.fa", { expr(binary(EK::OP_MUL, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseSimpleSubtraction)
{
    test_stmts("simple_subtraction.fa", { expr(binary(EK::OP_SUB, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseSimpleDivision)
{
    test_stmts("simple_division.fa", { expr(binary(EK::OP_DIV, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseComplexExpression)
{
    test_stmts(
        "complex_expression.fa", { expr(binary(EK::OP_ADD, integer(2), binary(EK::OP_MUL, integer(3), integer(4)))) });
}

TEST_F(ParserTest, BinaryOperatorsAssociateLeft)
{
    struct Case {
        char const* source;
        EK outer;
        EK inner;
        i64 first;
        i64 second;
        i64 third;
    };
    for (auto const& test :
        { Case { "8 - 3 - 1", EK::OP_SUB, EK::OP_SUB, 8, 3, 1 }, Case { "8 + 3 - 1", EK::OP_SUB, EK::OP_ADD, 8, 3, 1 },
            Case { "8 / 4 / 2", EK::OP_DIV, EK::OP_DIV, 8, 4, 2 },
            Case { "8 / 4 * 2", EK::OP_MUL, EK::OP_DIV, 8, 4, 2 },
            Case { "8 >> 2 << 1", EK::OP_LSHIFT, EK::OP_RSHIFT, 8, 2, 1 } }) {
        test_source(test.source,
            { expr(binary(
                test.outer, binary(test.inner, integer(test.first), integer(test.second)), integer(test.third))) });
    }
}

TEST_F(ParserTest, PowerAssociatesRightAndBindsInsideUnary)
{
    test_source("-2 ** 3 ** -4",
        { expr(unary(EK::OP_NEG,
            binary(EK::OP_POW, integer(2), binary(EK::OP_POW, integer(3), unary(EK::OP_NEG, integer(4)))))) });
}

TEST_F(ParserTest, ParseNestedParentheses)
{
    test_stmts(
        "nested_parens.fa", { expr(binary(EK::OP_MUL, binary(EK::OP_ADD, integer(2), integer(3)), integer(4))) });
}

TEST_F(ParserTest, ParseLogicalExpression)
{
    test_stmts("logical_expression.fa", { expr(binary(EK::OP_OR, binary(EK::OP_AND, id("ا"), id("ب")), id("ت"))) });
}

TEST_F(ParserTest, ParseUnaryChain)
{
    test_stmts("unary_chain.fa", { expr(unary(EK::OP_NEG, unary(EK::OP_NEG, id("ب")))) });
}

TEST_F(ParserTest, ParseComplexFunctionCall)
{
    test_stmts("complex_function_call.fa",
        { expr(call(id("علم"), { binary(EK::OP_ADD, id("ا"), id("ب")), binary(EK::OP_MUL, id("ت"), id("ث")) })) });
}

TEST_F(ParserTest, ParseUnmatchedParenthesis)
{
    file_manager.load(parser_test_cases_dir() / "unmatched_paren.fa", true);
    Parser parser(&file_manager, BodyParsing::EAGER);
    EXPECT_TRUE(parser.parse().has_error());
}

TEST_F(ParserTest, ParseExtraClosingParenthesis)
{
    file_manager.load(parser_test_cases_dir() / "extra_paren.fa", true);
    Parser parser(&file_manager, BodyParsing::EAGER);
    auto actual = run_success(parser.parse());
    EXPECT_TRUE(actual->equals(binary(EK::OP_ADD, id("ا"), id("ب"))));
    EXPECT_FALSE(parser.we_done());
    EXPECT_TRUE(parser.check(tok::TokenType::RPAREN));
}

TEST_F(ParserTest, ParseInvalidOperatorSequence)
{
    // Unary plus is legal: a ++ b means a + (+b).
    test_stmts("invalid_operator_seq.fa", { expr(binary(EK::OP_ADD, id("ا"), unary(EK::OP_PLUS, id("ب")))) });
}

TEST_F(ParserTest, ParseSingleIdentifier) { test_stmts("single_identifier.fa", { expr(id("ا")) }); }

TEST_F(ParserTest, ParseVeryLongIdentifier)
{
    std::string name;
    for (unsigned i = 0; i < 1000; ++i) name += "ا";
    test_stmts("long_identifier.fa", { expr(id(StringRef(name.c_str()))) });
}

TEST_F(ParserTest, ParseUnicodeIdentifiers)
{
    test_stmts("unicode_identifiers.fa", { expr(binary(EK::OP_ADD, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseEmptyList) { test_stmts("empty_list.fa", { expr(list({ })) }); }

TEST_F(ParserTest, ParseEmptyTuple) { test_stmts("empty_tuple.fa", { expr(list({ })) }); }

TEST_F(ParserTest, ParseListWithTrailingComma)
{
    test_stmts("list_trailing_comma.fa", { expr(list({ integer(1), integer(2), integer(3) })) });
}

TEST_F(ParserTest, ParseNestedLists)
{
    test_stmts(
        "nested_lists.fa", { expr(list({ list({ integer(1), integer(2) }), list({ integer(3), integer(4) }) })) });
}

TEST_F(ParserTest, ParseAssignment) { test_stmts("assignment.fa", { assign(id("ا"), integer(42)) }); }

TEST_F(ParserTest, ParseChainedAssignment)
{
    test_stmts("chained_assignment.fa", { assign(id("ا"), assignment(id("ب"), integer(5))) });
}

TEST_F(ParserTest, ParseChainedAssignmentWithExpr)
{
    test_stmts("chained_assignment_with_expression.fa",
        { assign(id("ا"), assignment(id("ب"), binary(EK::OP_ADD, id("م"), id("ل")))) });
}

TEST_F(ParserTest, ParseDeeplyNestedExpression) { test_stmts("deeply_nested.fa", { expr(id("ا")) }); }

TEST_F(ParserTest, ParseWhileLoop)
{
    test_stmts("while_loop.fa",
        { while_loop(binary(EK::OP_EQ, id("شيء"), boolean(true)), block({ assign(id("بسبسمياو"), boolean(false)) })) });
}

TEST_F(ParserTest, ParseForLoop)
{
    test_stmts("for_loop.fa", { for_loop("عنصر", id("عناصر"), block({ assign(id("اجمع"), id("عنصر")) })) });
}

TEST_F(ParserTest, ParseBreakStatement) { test_stmts("break_stmt.fa", { breaks() }); }

TEST_F(ParserTest, ParseContinueStatement) { test_stmts("continue_stmt.fa", { continues() }); }

TEST_F(ParserTest, ParseComplexeIfStatement)
{
    test_stmts("complexe_if_statement.fa",
        { branch(binary(EK::OP_NEQ, id("شيء"), boolean(true)),
            block({ while_loop(
                binary(EK::OP_EQ, id("شيء"), boolean(true)), block({ assign(id("بسبسمياو"), boolean(false)) })) })) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentPlus)
{
    test_stmts("augmented_assign_plus.fa", { augmented(id("ا"), EK::OP_ADD, id("ب")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentMinus)
{
    test_stmts("augmented_assign_minus.fa", { augmented(id("ا"), EK::OP_SUB, id("ب")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentTimes)
{
    test_stmts("augmented_assign_times.fa", { augmented(id("ا"), EK::OP_MUL, id("ب")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentDiv)
{
    test_stmts("augmented_assign_div.fa", { augmented(id("ا"), EK::OP_DIV, id("ب")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentMod)
{
    test_stmts("augmented_assign_mod.fa", { augmented(id("ا"), EK::OP_MOD, id("ب")) });
}

TEST_F(ParserTest, ParseArabicNumercLiterals)
{
    AST::Stmt* int_assign = ast_builder.make_assignment_stmt(
        ast_builder.make_identifier("عدد_صحيح", { }), ast_builder.make_literal_int(123, { }), { });
    AST::Stmt* float_assign = ast_builder.make_assignment_stmt(
        ast_builder.make_identifier("عدد_حقيقي", { }), ast_builder.make_literal_float(12.5, { }), { });
    test_stmts("arabic_numeric_literals.fa", { int_assign, float_assign });
}

TEST_F(ParserTest, ParseAssertEnglishMessage)
{
    test_stmts("assert_english_message.fa", { expr(call(id("تاكد"), { binary(EK::OP_EQ, id("س"), integer(1)) })) });
}

TEST_F(ParserTest, ParseAssertStatement)
{
    test_stmts("assert_statement.fa", { expr(call(id("تاكد"), { boolean(true) })) });
}

TEST_F(ParserTest, ParseAssertMessage)
{
    test_stmts("assert_message.fa",
        { expr(call(id("تاكد"), { binary(EK::OP_GT, id("س"), integer(0)), text("قيمة موجبة مطلوبة") })) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentArabicPercent)
{
    test_stmts("augmented_assign_arabic_percent.fa", { augmented(id("قيمة"), EK::OP_MOD, integer(3)) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentBitAnd)
{
    test_stmts("augmented_assign_bitand.fa", { augmented(id("قيمة"), EK::OP_BITAND, id("قناع")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentBitOr)
{
    test_stmts("augmented_assign_bitor.fa", { augmented(id("قيمة"), EK::OP_BITOR, id("قناع")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentBitXor)
{
    test_stmts("augmented_assign_bitxor.fa", { augmented(id("قيمة"), EK::OP_BITXOR, id("قناع")) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentLeftShift)
{
    test_stmts("augmented_assign_lshift.fa", { augmented(id("قيمة"), EK::OP_LSHIFT, integer(2)) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentRightShift)
{
    test_stmts("augmented_assign_rshift.fa", { augmented(id("قيمة"), EK::OP_RSHIFT, integer(1)) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentIndex)
{
    test_stmts("augmented_assign_index.fa", { augmented(index(id("قيم"), call(id("فهرس"))), EK::OP_ADD, integer(5)) });
}

TEST_F(ParserTest, ParseAugmentedAssignmentMember)
{
    test_stmts("augmented_assign_member.fa", { augmented(get(id("علبة"), "قيمة"), EK::OP_MUL, integer(2)) });
}

TEST_F(ParserTest, ParseBinaryLiteral) { test_stmts("binary_literal.fa", { assign(id("عدد"), integer(42)) }); }

TEST_F(ParserTest, ParseHexLiteral) { test_stmts("hex_literal.fa", { assign(id("عدد"), integer(42)) }); }

TEST_F(ParserTest, ParseOctalLiteral) { test_stmts("octal_literal.fa", { assign(id("عدد"), integer(42)) }); }

TEST_F(ParserTest, ParseFloatLiteral) { test_stmts("float_literal.fa", { assign(id("عدد"), real(123.456)) }); }

TEST_F(ParserTest, ParseNumericSeparators)
{
    test_stmts(
        "numeric_separators.fa", { assign(id("عدد_صحيح"), integer(1234567)), assign(id("عدد_حقيقي"), real(1234.56)) });
}

TEST_F(ParserTest, ParseSingleQuotedString)
{
    test_stmts("single_quoted_string.fa", { assign(id("نص"), text("مرحبا بالعالم")) });
}

TEST_F(ParserTest, ParseStringEscapes)
{
    test_stmts("string_escapes.fa",
        { assign(id("نص"), text("سطر\nثان\t\r\b\f/\\\"'م")), assign(id("اقتباس"), text("اقتباس: ' ونص: \"")) });
}

TEST_F(ParserTest, ParseBitwiseExpression)
{
    test_stmts("bitwise_expression.fa",
        { assign(id("تقاطع"), binary(EK::OP_BITAND, id("ا"), id("ب"))),
            assign(id("اتحاد"), binary(EK::OP_BITOR, id("ا"), id("ب"))),
            assign(id("اختلاف"), binary(EK::OP_BITXOR, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseBitwiseNot)
{
    test_stmts("bitwise_not.fa",
        { assign(id("قيمة"), unary(EK::OP_BITNOT, id("عدد"))),
            assign(id("نتيجة"), unary(EK::OP_BITNOT, binary(EK::OP_BITOR, id("ا"), id("ب")))) });
}

TEST_F(ParserTest, ParseLogicalNot)
{
    test_stmts("logical_not.fa",
        { assign(id("قيمة"), unary(EK::OP_NOT, boolean(true))),
            assign(id("نتيجة"), binary(EK::OP_OR, binary(EK::OP_AND, unary(EK::OP_NOT, id("ا")), id("ب")), id("ت"))) });
}

TEST_F(ParserTest, ParseUnaryPlus)
{
    test_stmts("unary_plus.fa",
        { assign(id("قيمة"), unary(EK::OP_PLUS, integer(5))),
            assign(id("نتيجة"), unary(EK::OP_PLUS, binary(EK::OP_ADD, id("ا"), id("ب")))) });
}

TEST_F(ParserTest, ParseModuloExpression)
{
    test_stmts("modulo_expression.fa",
        { assign(id("باقي"), binary(EK::OP_MOD, integer(17), integer(5))),
            assign(id("باقي_عربي"), binary(EK::OP_MOD, integer(17), integer(5))) });
}

TEST_F(ParserTest, ParseShiftExpression)
{
    test_stmts("shift_expression.fa",
        { assign(id("يسار"), binary(EK::OP_LSHIFT, id("قيمة"), integer(2))),
            assign(id("يمين"), binary(EK::OP_RSHIFT, id("قيمة"), integer(1))) });
}

TEST_F(ParserTest, ParseComparisonGreaterEqual)
{
    test_stmts("comparison_greater_equal.fa", { assign(id("نتيجة"), binary(EK::OP_GTE, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseComparisonLessEqual)
{
    test_stmts("comparison_less_equal.fa", { assign(id("نتيجة"), binary(EK::OP_LTE, id("ا"), id("ب"))) });
}

TEST_F(ParserTest, ParseChainedComparison)
{
    test_stmts("chained_comparison.fa", { expr(binary(EK::OP_LT, binary(EK::OP_LT, id("ا"), id("ب")), id("ت"))) });
}

TEST_F(ParserTest, ParseOperatorPrecedence)
{
    // Equality binds more tightly than &, followed by ^, |, and, then or.
    auto arithmetic = binary(EK::OP_ADD, id("ا"), binary(EK::OP_MUL, id("ب"), id("ت")));
    auto shifted = binary(EK::OP_LSHIFT, arithmetic, integer(2));
    auto compared = binary(EK::OP_EQ, id("ح"), id("خ"));
    auto bitwise
        = binary(EK::OP_BITOR, binary(EK::OP_BITXOR, binary(EK::OP_BITAND, shifted, id("ث")), id("ج")), compared);
    test_stmts("operator_precedence.fa",
        { assign(id("نتيجة"), binary(EK::OP_OR, binary(EK::OP_AND, bitwise, id("د")), id("ذ"))),
            assign(id("فرق"), binary(EK::OP_SUB, binary(EK::OP_SUB, id("ا"), id("ب")), id("ت"))),
            assign(id("خارج"), binary(EK::OP_DIV, binary(EK::OP_DIV, id("ا"), id("ب")), id("ت"))) });
}

TEST_F(ParserTest, ParsePowerExpression)
{
    test_stmts("power_expression.fa", { assign(id("قيمة"), binary(EK::OP_POW, integer(2), integer(10))) });
}

TEST_F(ParserTest, ParsePowerRightAssociative)
{
    test_stmts("power_right_associative.fa",
        { assign(id("قيمة"), binary(EK::OP_POW, integer(2), binary(EK::OP_POW, integer(3), integer(2)))) });
}

TEST_F(ParserTest, ParsePowerUnaryPrecedence)
{
    test_stmts("power_unary_precedence.fa",
        { assign(id("سالب"), unary(EK::OP_NEG, binary(EK::OP_POW, integer(2), integer(2)))),
            assign(id("اس_سالب"), binary(EK::OP_POW, integer(2), unary(EK::OP_NEG, integer(3)))),
            assign(id("اساس_سالب"), binary(EK::OP_POW, unary(EK::OP_NEG, integer(2)), integer(2))) });
}

TEST_F(ParserTest, ParseCallMultiline)
{
    test_stmts("call_multiline.fa", { assign(id("نتيجة"), call(id("اجمع"), { integer(1), integer(2) })) });
}

TEST_F(ParserTest, ParseCallTrailingComma)
{
    test_stmts("call_trailing_comma.fa", { assign(id("نتيجة"), call(id("اجمع"), { integer(1), integer(2) })) });
}

TEST_F(ParserTest, ParseIndexRead)
{
    test_stmts("index_read.fa", { assign(id("قيمة"), index(id("قيم"), integer(0))) });
}

TEST_F(ParserTest, ParseIndexExpression)
{
    test_stmts(
        "index_expression.fa", { assign(id("قيمة"), index(id("قيم"), binary(EK::OP_ADD, id("فهرس"), integer(1)))) });
}

TEST_F(ParserTest, ParseIndexAssignment)
{
    test_stmts("index_assignment.fa", { assign(index(id("قيم"), integer(0)), integer(42)) });
}

TEST_F(ParserTest, ParseIndexNested)
{
    test_stmts("index_nested.fa",
        { assign(index(index(id("قيم"), integer(0)), integer(1)), index(index(id("قيم"), integer(1)), integer(0))) });
}

TEST_F(ParserTest, ParseMemberRead) { test_stmts("member_read.fa", { assign(id("قيمة"), get(id("علبة"), "قيمة")) }); }

TEST_F(ParserTest, ParseMemberAssignment)
{
    test_stmts("member_assignment.fa", { assign(get(id("علبة"), "قيمة"), integer(42)) });
}

TEST_F(ParserTest, ParseMethodCall)
{
    test_stmts("method_call.fa", { assign(id("نتيجة"), call(get(id("علبة"), "اجمع"), { integer(1), integer(2) })) });
}

TEST_F(ParserTest, ParsePostfixChain)
{
    test_stmts("postfix_chain.fa",
        { assign(id("عنصر"),
              index(call(get(get(index(call(id("مصنع")), id("فهرس")), "عنصر"), "اجمع"), { integer(1), integer(2) }),
                  integer(0))),
            assign(id("نتيجة"), call(call(id("مصنع")))) });
}

TEST_F(ParserTest, ParseListMultiline)
{
    test_stmts("list_multiline.fa", { assign(id("قيم"), list({ integer(1), integer(2), integer(3) })) });
}

TEST_F(ParserTest, ParseParenthesizedSequence)
{
    test_stmts("parenthesized_sequence.fa", { assign(id("قيم"), list({ integer(1), integer(2), integer(3) })) });
}

TEST_F(ParserTest, ParseParenthesizedSingleton)
{
    test_stmts("parenthesized_singleton.fa", { assign(id("قيم"), list({ integer(1) })) });
}

TEST_F(ParserTest, ParseDictionaryEmpty) { test_stmts("dictionary_empty.fa", { assign(id("قاموس"), dict({ })) }); }

TEST_F(ParserTest, ParseDictionaryLiteral)
{
    test_stmts("dictionary_literal.fa",
        { assign(id("قاموس"), dict({ { text("اول"), integer(1) }, { text("ثان"), integer(2) } })) });
}

TEST_F(ParserTest, ParseDictionaryMultiline)
{
    test_stmts("dictionary_multiline.fa",
        { assign(id("قاموس"), dict({ { text("اول"), integer(1) }, { text("ثان"), integer(2) } })) });
}

TEST_F(ParserTest, ParseDictionaryTrailingComma)
{
    test_stmts("dictionary_trailing_comma.fa", { assign(id("قاموس"), dict({ { text("قيمة"), integer(1) } })) });
}

TEST_F(ParserTest, ParseDictionaryComputedKeys)
{
    test_stmts("dictionary_computed_keys.fa",
        { assign(id("قاموس"),
            dict({ { binary(EK::OP_ADD, integer(1), integer(2)), integer(3) },
                { binary(EK::OP_ADD, id("اسم"), text("لاحقة")), integer(4) } })) });
}

TEST_F(ParserTest, ParseDictionaryNested)
{
    test_stmts("dictionary_nested.fa",
        { assign(id("قاموس"),
            dict({ { text("داخلي"), dict({ { text("قيمة"), integer(1) } }) },
                { text("قائمة"), list({ integer(1), dict({ { text("قيمة"), integer(2) } }) }) } })) });
}

TEST_F(ParserTest, ParseFunctionNoParameters)
{
    test_stmts("function_no_parameters.fa", { function("قيمة", { }, block({ returns(integer(42)) })) });
}

TEST_F(ParserTest, ParseFunctionMultipleParameters)
{
    test_stmts("function_multiple_parameters.fa",
        { function("اجمع", { id("اول"), id("ثان"), id("ثالث") },
            block({ returns(binary(EK::OP_ADD, binary(EK::OP_ADD, id("اول"), id("ثان")), id("ثالث"))) })) });
}

TEST_F(ParserTest, ParseFunctionNested)
{
    test_stmts("function_nested.fa",
        { function("خارج", { id("س") },
            block({ function("داخل", { id("ص") }, block({ returns(binary(EK::OP_ADD, id("ص"), integer(1))) })),
                returns(call(id("داخل"), { id("س") })) })) });
}

TEST_F(ParserTest, ParseParametersMultiline)
{
    test_stmts("parameters_multiline.fa",
        { function("اجمع", { id("اول"), id("ثان") }, block({ returns(binary(EK::OP_ADD, id("اول"), id("ثان"))) })) });
}

TEST_F(ParserTest, ParseParametersTrailingComma)
{
    test_stmts("parameters_trailing_comma.fa",
        { function("اجمع", { id("اول"), id("ثان") }, block({ returns(binary(EK::OP_ADD, id("اول"), id("ثان"))) })) });
}

TEST_F(ParserTest, ParseReturnBare) { test_stmts("return_bare.fa", { function("توقف", { }, block({ returns() })) }); }

TEST_F(ParserTest, ParseReturnValue)
{
    test_stmts("return_value.fa",
        { function("قيمة", { id("س") }, block({ returns(binary(EK::OP_MUL, id("س"), integer(2))) })) });
}

TEST_F(ParserTest, ParseClassDefinition)
{
    test_stmts(
        "class_definition.fa", { klass("علبة", { }, { function("قيمة", { }, block({ returns(integer(1)) })) }) });
}

TEST_F(ParserTest, ParseClassConstructorThis)
{
    test_stmts("class_constructor_this.fa",
        { klass("علبة", { id("قيمة") },
            { function("بداية", { id("قيمة") }, block({ assign(field("قيمة"), id("قيمة")) })),
                function("اقرأ", { }, block({ returns(field("قيمة")) })) }) });
}

TEST_F(ParserTest, ParseClassFieldShorthand)
{
    test_stmts("class_field_shorthand.fa",
        { klass("علبة", { id("قيمة") },
            { function("بداية", { id("قيمة") }, block({ assign(field("قيمة"), id("قيمة")) })),
                function("اقرأ", { }, block({ returns(field("قيمة")) })) }) });
}

TEST_F(ParserTest, ParseClassFieldShorthandAugmented)
{
    test_stmts("class_field_shorthand_augmented.fa",
        { klass("علبة", { id("قيمة") },
            { function("بداية", { },
                block({ assign(field("قيمة"), integer(1)), augmented(field("قيمة"), EK::OP_ADD, integer(2)) })) }) });
}

TEST_F(ParserTest, ParseClassInheritance)
{
    test_stmts("class_inheritance.fa",
        { klass("اصل", { }, { function("قيمة", { }, block({ returns(integer(1)) })) }),
            klass("طفل", { }, { function("قيمة", { }, block({ returns(integer(2)) })) }, id("اصل")) });
}

TEST_F(ParserTest, ParseClassMultipleMethods)
{
    test_stmts("class_multiple_methods.fa",
        { klass("علبة", { },
            { function("اول", { }, block({ returns(integer(1)) })),
                function("ثان", { }, block({ returns(integer(2)) })),
                function("ثالث", { }, block({ returns(integer(3)) })) }) });
}

TEST_F(ParserTest, ParseClassOperatorMethods)
{
    test_stmts("class_operator_methods.fa",
        { klass("علبة", { },
            { function("عملية+", { id("اخر") }, block({ returns(integer(1)) })),
                function("عملية-", { id("اخر") }, block({ returns(integer(2)) })),
                function("عملية*", { id("اخر") }, block({ returns(integer(3)) })),
                function("عملية/", { id("اخر") }, block({ returns(integer(4)) })),
                function("عملية%", { id("اخر") }, block({ returns(integer(5)) })) }) });
}

TEST_F(ParserTest, ParseClassOperatorArabicPercent)
{
    test_stmts("class_operator_arabic_percent.fa",
        { klass("علبة", { }, { function("عملية٪", { id("اخر") }, block({ returns(integer(1)) })) }) });
}

TEST_F(ParserTest, ParseIfElse)
{
    test_stmts("if_else.fa",
        { branch(
            boolean(true), block({ assign(id("قيمة"), integer(1)) }), block({ assign(id("قيمة"), integer(2)) })) });
}

TEST_F(ParserTest, ParseIfElseIf)
{
    test_stmts("if_else_if.fa",
        { branch(boolean(false), block({ assign(id("قيمة"), integer(1)) }),
            branch(boolean(true), block({ assign(id("قيمة"), integer(2)) }),
                block({ assign(id("قيمة"), integer(3)) }))) });
}

TEST_F(ParserTest, ParseNestedIfElse)
{
    test_stmts("nested_if_else.fa",
        { branch(boolean(true),
            block({ branch(boolean(false), block({ assign(id("قيمة"), integer(1)) }),
                block({ assign(id("قيمة"), integer(2)) })) }),
            block({ assign(id("قيمة"), integer(3)) })) });
}

TEST_F(ParserTest, ParseInlineIf)
{
    test_stmts("inline_if.fa",
        { branch(boolean(true), block({ assign(id("قيمة"), integer(1)) }), block({ assign(id("قيمة"), integer(2)) })),
            assign(id("بعد"), integer(3)) });
}

TEST_F(ParserTest, ParseInlineElseBinding)
{
    test_stmts("inline_else_binding.fa",
        { branch(boolean(true), block({ branch(boolean(false), block({ assign(id("قيمة"), integer(1)) })) }),
              block({ assign(id("قيمة"), integer(2)) })),
            assign(id("بعد"), integer(3)) });
}

TEST_F(ParserTest, ParseInlineWhile)
{
    test_stmts("inline_while.fa", { while_loop(boolean(true), block({ breaks() })), assign(id("بعد"), integer(1)) });
}

TEST_F(ParserTest, ParseInlineFor)
{
    test_stmts("inline_for.fa",
        { for_loop("عنصر", list({ integer(1), integer(2) }), block({ continues() })), assign(id("بعد"), integer(1)) });
}

TEST_F(ParserTest, ParseInlineFunction)
{
    test_stmts(
        "inline_function.fa", { function("قيمة", { }, block({ returns(integer(1)) })), assign(id("بعد"), integer(2)) });
}

TEST_F(ParserTest, ParseInlineClass)
{
    test_stmts("inline_class.fa",
        { klass("علبة", { }, { function("قيمة", { }, block({ returns(integer(1)) })) }),
            assign(id("بعد"), integer(2)) });
}

TEST_F(ParserTest, ParseComments)
{
    test_stmts("comments.fa",
        { assign(id("قيمة"), integer(1)),
            branch(boolean(true), block({ augmented(id("قيمة"), EK::OP_ADD, integer(1)) })) });
}

TEST_F(ParserTest, ParseContinuationIndentation)
{
    test_stmts("continuation_indentation.fa",
        { branch(boolean(true),
              block({ assign(id("قيم"), list({ integer(1), integer(2) })),
                  assign(id("نتيجة"),
                      call(id("اجمع"), { index(id("قيم"), integer(0)), index(id("قيم"), integer(1)) })) })),
            assign(id("بعد"), integer(3)) });
}

TEST_F(ParserTest, ParseImportModule)
{
    test_stmts("import_module.fa", { import_stmt("رياضيات", { }, { "رياضيات" }) });
}

TEST_F(ParserTest, ParseImportModuleAlias)
{
    test_stmts("import_module_alias.fa", { import_stmt("رياضيات", { }, { "ر" }) });
}

TEST_F(ParserTest, ParseImportDottedModule)
{
    test_stmts(
        "import_dotted_module.fa", { import_stmt("حزمة.فرع", { }, { "فرع" }), import_stmt("حزمة.فرع", { }, { "ف" }) });
}

TEST_F(ParserTest, ParseImportFromMember)
{
    test_stmts("import_from_member.fa", { import_stmt("رياضيات", { "عاملي" }, { "عاملي" }) });
}

TEST_F(ParserTest, ParseImportFromAlias)
{
    test_stmts("import_from_alias.fa", { import_stmt("رياضيات", { "عاملي" }, { "احسب" }) });
}

TEST_F(ParserTest, ParseImportFromMultiple)
{
    test_stmts("import_from_multiple.fa", { import_stmt("حزمة", { "اول", "ثان", "ثالث" }, { "ا", "ثان", "ت" }) });
}

TEST_F(ParserTest, ParseImportFromMultiline)
{
    test_stmts("import_from_multiline.fa", { import_stmt("حزمة.فرع", { "اول", "ثان" }, { "ا", "ثان" }) });
}

TEST_F(ParserTest, ParseEmptyPrograms)
{
    test_stmts("empty_input.fa", { });
    test_stmts("test_indentation.fa", { });
    test_stmts("whitespace_only.fa", { });
}

TEST_F(ParserTest, ParseUnaryMinus) { test_stmts("unary_minus.fa", { expr(unary(EK::OP_NEG, integer(5))) }); }

TEST_F(ParserTest, ParseLargeFile)
{
    Array<AST::StmtPtr> expected;
    for (int i = 0; i < 5000; ++i) {
        auto name = std::string("س") + std::to_string(i);
        expected.push(
            expr(binary(EK::OP_EQ, id(StringRef(name.c_str())), binary(EK::OP_ADD, integer(i), integer(i + 1)))));
    }
    test_stmts("large_file.fa", std::move(expected));
}

TEST_F(ParserTest, RejectInvalidFixtureSyntax)
{
    struct Case {
        char const* path;
        ErrorCode code;
        bool lexical_abort = false;
    };
    for (auto const& test : { Case { "invalid_assignment_in_expression.fa", ErrorCode::ASSIGNMENT_IN_EXPRESSION },
             Case { "invalid_assignment_target.fa", ErrorCode::INVALID_ASSIGN_TARGET },
             Case { "invalid_binary_digit.fa", ErrorCode::INVALID_BINARY_DIGIT, true },
             Case { "invalid_class_missing_colon.fa", ErrorCode::EXPECTED_COLON_CLASS },
             Case { "invalid_dictionary_closing_brace.fa", ErrorCode::UNCLOSED_DELIMITER },
             Case { "invalid_dictionary_colon.fa", ErrorCode::EXPECTED_COLON_DICT },
             Case { "invalid_function_body.fa", ErrorCode::UNEXPECTED_TOKEN },
             Case { "invalid_function_missing_colon.fa", ErrorCode::EXPECTED_COLON_FN },
             Case { "invalid_function_name.fa", ErrorCode::EXPECTED_FN_NAME },
             Case { "invalid_hex_literal.fa", ErrorCode::INVALID_BASE_LITERAL, true },
             Case { "invalid_import_alias.fa", ErrorCode::EXPECTED_ALIAS_NAME },
             Case { "invalid_import_member.fa", ErrorCode::EXPECTED_IMPORT_NAME },
             Case { "invalid_import_module.fa", ErrorCode::EXPECTED_MODULE_NAME },
             Case { "invalid_import_parenthesis.fa", ErrorCode::UNCLOSED_DELIMITER },
             Case { "invalid_index_expression.fa", ErrorCode::UNEXPECTED_TOKEN },
             Case { "invalid_inline_extra_statement.fa", ErrorCode::UNEXPECTED_TOKEN },
             Case { "invalid_member_name.fa", ErrorCode::EXPECTED_MEMBER_NAME },
             Case { "invalid_mismatched_delimiter.fa", ErrorCode::MISMATCHED_DELIMITER },
             Case { "invalid_missing_block.fa", ErrorCode::EXPECTED_INDENT },
             Case { "invalid_octal_digit.fa", ErrorCode::INVALID_OCTAL_DIGIT, true },
             Case { "invalid_parameter_name.fa", ErrorCode::EXPECTED_PARAM_NAME },
             Case { "invalid_parent_class.fa", ErrorCode::EXPECTED_CLASS_NAME },
             Case { "invalid_string_escape.fa", ErrorCode::INVALID_ESCAPE_SEQUENCE, true },
             Case { "invalid_unterminated_string.fa", ErrorCode::UNTERMINATED_STRING, true },
             Case { "invalid_syntax.fa", ErrorCode::UNEXPECTED_EOF },
             Case { "missing_operand.fa", ErrorCode::UNEXPECTED_EOF },
             Case { "unexpected_eof.fa", ErrorCode::UNEXPECTED_EOF } }) {
        test_error(test.path, test.code, test.lexical_abort);
    }
}

TEST_F(ParserTest, ScientificNotationIsRejected)
{
    // This fixture documents unsupported syntax, rather than deriving an
    // expected numeric value from a second invocation of the parser.
    test_error("scientific_literal.fa", ErrorCode::UNEXPECTED_TOKEN);
}

TEST_F(ParserTest, RecoveryRetainsCompleteStatementsInOrder)
{
    file_manager.load(parser_test_cases_dir() / "recovery_multiple_errors.fa", true);
    diagnostic::SourceScope source_scope(&file_manager);
    Parser parser(&file_manager, BodyParsing::EAGER);
    auto actual = parser.parse_program();

    ASSERT_EQ(diagnostic::error_count(), 2u);
    auto const* first = diagnostic::engine.first_error();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->err_code, ErrorCode::UNEXPECTED_TOKEN);
    EXPECT_EQ(first->src_loc.line, 1u);
    expect_stmts(actual, { assign(id("وسط"), integer(2)), assign(id("نهاية"), integer(4)) });
}

TEST_F(ParserTest, ObsoleteReturnSpellingReportsErrorsAndPreservesValidStatements)
{
    file_manager.load(parser_test_cases_dir() / "test_file.fa", true);
    diagnostic::SourceScope source_scope(&file_manager);
    Parser parser(&file_manager, BodyParsing::EAGER);
    auto actual = parser.parse_program();

    ASSERT_EQ(diagnostic::error_count(), 2u);
    auto const* first = diagnostic::engine.first_error();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->err_code, ErrorCode::UNEXPECTED_TOKEN);
    expect_stmts(actual,
        { function("فاكتريال", { id("س") },
            block({ branch(binary(EK::OP_EQ, id("س"), integer(0)), block({ })), assign(id("م"), integer(1)),
                while_loop(binary(EK::OP_GT, id("س"), integer(1)),
                    block({ assign(id("م"), binary(EK::OP_MUL, id("م"), id("س"))),
                        assign(id("س"), binary(EK::OP_SUB, id("س"), integer(1))) })) })) });
}
