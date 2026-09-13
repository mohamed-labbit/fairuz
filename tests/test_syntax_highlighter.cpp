#include "../fairuz/fsyntax_highlighter.hpp"
#include "../fairuz/farena.hpp"

#include <gtest/gtest.h>

using namespace fairuz;
using namespace fairuz::syntax;

namespace {

Token const* find_token(Result const& result, u32 line, u32 start, std::string const& type)
{
    for (Token const& token : result.tokens) {
        if (token.line == line && token.start == start && token.type == type)
            return &token;
    }
    return nullptr;
}

class SyntaxHighlighterTest : public ::testing::Test {
protected:
    Fa_AllocatorContext allocator;
    Fa_AllocatorContext* previous_allocator { nullptr };

    void SetUp() override
    {
        previous_allocator = g_context;
        set_context(&allocator);
    }

    void TearDown() override { set_context(previous_allocator); }
};

} // namespace

TEST_F(SyntaxHighlighterTest, AstClassifiesDeclarationsCallsAndMembers)
{
    Fa_StringRef source =
        "# نموذج\n"
        "نوع حيوان:\n"
        "    دالة صوت(الاسم):\n"
        "        رسالة := \"مرحبا\"\n"
        "        ارجع الاسم\n"
        "دالة ناد(القيمة):\n"
        "    ارجع القيمة.صوت(\"فيروز\")\n";
    Result result = Highlighter().highlight(source);
    ASSERT_TRUE(result.ast_valid);
    EXPECT_NE(find_token(result, 0, 0, "comment"), nullptr);
    auto* klass = find_token(result, 1, 4, "class");
    ASSERT_NE(klass, nullptr);
    EXPECT_TRUE(klass->declaration);
    auto* method = find_token(result, 2, 9, "method");
    ASSERT_NE(method, nullptr);
    EXPECT_TRUE(method->declaration);
    EXPECT_NE(find_token(result, 2, 13, "parameter"), nullptr);
    EXPECT_NE(find_token(result, 3, 8, "variable"), nullptr);
    EXPECT_NE(find_token(result, 6, 16, "method"), nullptr);
}

TEST_F(SyntaxHighlighterTest, UsesUtf16ColumnsAndLengths)
{
    Fa_StringRef source = "قيمة := \"😀\" + 2 # شرح\n";
    Result result = Highlighter().highlight(source);
    ASSERT_TRUE(result.ast_valid);
    auto* string = find_token(result, 0, 8, "string");
    ASSERT_NE(string, nullptr);
    EXPECT_EQ(string->length, 4u); // quotes plus one supplementary Unicode scalar
    EXPECT_NE(find_token(result, 0, 15, "number"), nullptr);
    EXPECT_NE(find_token(result, 0, 17, "comment"), nullptr);
}

TEST_F(SyntaxHighlighterTest, RecognizesEveryNumericLiteralForm)
{
    Fa_StringRef source = "س := 0xFF + 0o17 + 0b10 + ١٢ + 3.14\n";
    Result result = Highlighter().highlight(source);
    EXPECT_NE(find_token(result, 0, 5, "number"), nullptr);
    EXPECT_NE(find_token(result, 0, 12, "number"), nullptr);
    EXPECT_NE(find_token(result, 0, 19, "number"), nullptr);
    EXPECT_NE(find_token(result, 0, 26, "number"), nullptr);
    EXPECT_NE(find_token(result, 0, 31, "number"), nullptr);
}

TEST_F(SyntaxHighlighterTest, ClassifiesModuleMemberAndAliasImports)
{
    Fa_StringRef source =
        "استورد math.util باسم حساب\n"
        "من runtime استورد هو_عدد باسم عدد\n";
    Result result = Highlighter().highlight(source);
    ASSERT_TRUE(result.ast_valid);
    auto* module = find_token(result, 0, 7, "namespace");
    ASSERT_NE(module, nullptr);
    EXPECT_FALSE(module->declaration);
    auto* alias = find_token(result, 0, 22, "namespace");
    ASSERT_NE(alias, nullptr);
    EXPECT_TRUE(alias->declaration);
    auto* member = find_token(result, 1, 18, "variable");
    ASSERT_NE(member, nullptr);
    EXPECT_TRUE(member->declaration);
    auto* member_alias = find_token(result, 1, 30, "variable");
    ASSERT_NE(member_alias, nullptr);
    EXPECT_TRUE(member_alias->declaration);
}

TEST_F(SyntaxHighlighterTest, KeepsLexicalHighlightingForIncompleteEdits)
{
    Fa_StringRef source = "دالة غير_مكتملة(القيمة:\n    ارجع \"نص";
    Result result;
    EXPECT_NO_THROW(result = Highlighter().highlight(source));
    EXPECT_FALSE(result.ast_valid);
    EXPECT_NE(find_token(result, 0, 0, "keyword"), nullptr);
    EXPECT_NE(find_token(result, 1, 9, "string"), nullptr);
}
