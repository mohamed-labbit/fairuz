#include "../fairuz/fformatter.hpp"
#include "../fairuz/fparser.hpp"
#include "fplatform.hpp"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <string>

using namespace fairuz;

namespace {

std::string format_source(std::string const& source)
{
    diagnostic::reset();
    lex::FileManager file;
    file.buffer() = StringRef(source.c_str());
    parser::Parser parser(&file);
    (void)parser.parse_program();
    if (diagnostic::has_errors())
        throw std::runtime_error("Invalid test source");
    auto result = Formatter().format(file.buffer());
    return result.empty() ? "" : std::string(result.data(), result.len());
}

void expect_format(std::string const& source, std::string const& expected)
{
    auto formatted = format_source(source);
    EXPECT_EQ(formatted, expected);
    EXPECT_EQ(format_source(formatted), formatted) << "Formatting must be idempotent";
}

} // namespace

TEST(Formatter, PreservesLoopsAssertionsImportsAndAugmentedAssignment)
{
    expect_format(
        "من رياضيات استورد جذر باسم جذري\n"
        "لكل عنصر في [1,2]:\n"
        "  عنصر+=1\n"
        "  تاكد عنصر>0, 'positive'\n",
        "من رياضيات استورد جذر باسم جذري\n"
        "لكل عنصر في [1، 2]:\n"
        "    عنصر += 1\n"
        "    تاكد عنصر > 0، 'positive'\n");
}

TEST(Formatter, PreservesInlineBlockBodies)
{
    expect_format(
        "اذا صحيح:اكتب(1)\n"
        "غيره:اكتب(2)\n"
        "طالما س<2:س+=1\n"
        "لكل س في [1,2]:اكتب(س)\n"
        "دالة ضعف(س):ارجع س*2\n"
        "نوع علبة:دالة بداية(س):هذا.قيمة:=س\n",
        "اذا صحيح: اكتب(1)\n"
        "غيره: اكتب(2)\n"
        "طالما س < 2: س += 1\n"
        "لكل س في [1، 2]: اكتب(س)\n"
        "دالة ضعف(س): ارجع س * 2\n"
        "نوع علبة: دالة بداية(س): هذا.قيمة := س\n");
}

TEST(Formatter, PreservesCommentsBlankLinesAndCommentOnlyFiles)
{
    expect_format("# header\n\nدالة مثال():\n  # body\n  ارجع 1 # inline\n\n# footer\n",
        "# header\n\nدالة مثال():\n    # body\n    ارجع 1  # inline\n\n# footer\n");
    expect_format("# only a comment", "# only a comment\n");
    expect_format("", "");
    expect_format(" \n\t\n", "");
}

TEST(Formatter, PreservesRawLiteralsAndGrouping)
{
    expect_format(
        "ص:=0.0000001\nك:=100000000000.0\n"
        "ن:=0xFF\nس:='\\u0645 # \\t \\' \\\\'\n"
        "اكتب((1+2)*3, 1-(2-3), -(-2))\n",
        "ص := 0.0000001\nك := 100000000000.0\n"
        "ن := 0xFF\nس := '\\u0645 # \\t \\' \\\\'\n"
        "اكتب((1 + 2) * 3، 1 - (2 - 3)، -(-2))\n");
}

TEST(Formatter, PreservesLongArabicStringsAndExpressionsWithoutUnsafeWrapping)
{
    std::string value;
    for (int i = 0; i < 80; ++i)
        value += "مرحباً";
    auto source = "دالة مثال():\n    ارجع \"" + value + "\"\n";
    expect_format(source, source);
    std::string expression = "ن := 1";
    for (int i = 0; i < 80; ++i)
        expression += " + 1";
    expression += '\n';
    expect_format(expression, expression);
}

TEST(Formatter, WrapsLongCallsAndCollectionsAtSafeCommas)
{
    expect_format(
        "نتيجة:=اجمع(العدد_الأول_الطويل,العدد_الثاني_الطويل,العدد_الثالث_الطويل,العدد_الرابع_الطويل)\n",
        "نتيجة := اجمع(\n"
        "    العدد_الأول_الطويل،\n"
        "    العدد_الثاني_الطويل،\n"
        "    العدد_الثالث_الطويل،\n"
        "    العدد_الرابع_الطويل\n"
        ")\n");
    expect_format(
        "قيم:=[11111111111111111111,22222222222222222222,33333333333333333333,44444444444444444444]\n",
        "قيم := [\n"
        "    11111111111111111111،\n"
        "    22222222222222222222،\n"
        "    33333333333333333333،\n"
        "    44444444444444444444\n"
        "]\n");
}

TEST(Formatter, HonorsFormatControlComments)
{
    expect_format(
        "س:=\"# fmt: off\"\n"
        "# fmt: off\n"
        "ا  :=[ 1,2 ]\n"
        "# fmt: on\n"
        "ب:=[3,4] # fmt: skip\n"
        "ج:=[5,6]\n",
        "س := \"# fmt: off\"\n"
        "# fmt: off\n"
        "ا  :=[ 1,2 ]\n"
        "# fmt: on\n"
        "ب:=[3,4] # fmt: skip\n"
        "ج := [5، 6]\n");
}

TEST(Formatter, WrapsLongNestedCallInsideExistingMultilineGroup)
{
    expect_format(
        "نتيجة := خارج(\n"
        "    داخل(الوسيط_الأول_الطويل_للغاية، الوسيط_الثاني_الطويل_للغاية، الوسيط_الثالث_الطويل_للغاية)،\n"
        "    ١\n"
        ")\n",
        "نتيجة := خارج(\n"
        "    داخل(\n"
        "        الوسيط_الأول_الطويل_للغاية،\n"
        "        الوسيط_الثاني_الطويل_للغاية،\n"
        "        الوسيط_الثالث_الطويل_للغاية\n"
        "    )،\n"
        "    ١\n"
        ")\n");
}

TEST(Formatter, PreservesTrailingCommasAndSuffixesWhenWrapping)
{
    expect_format(
        "دالة احسب(الوسيط_الأول_الطويل_للغاية,الوسيط_الثاني_الطويل_للغاية,الوسيط_الثالث_الطويل_للغاية,):\n"
        "  ارجع الوسيط_الأول_الطويل_للغاية\n",
        "دالة احسب(\n"
        "    الوسيط_الأول_الطويل_للغاية،\n"
        "    الوسيط_الثاني_الطويل_للغاية،\n"
        "    الوسيط_الثالث_الطويل_للغاية،\n"
        "):\n"
        "    ارجع الوسيط_الأول_الطويل_للغاية\n");
}

TEST(Formatter, CountsUnicodeCharactersInsteadOfUtf8BytesForLineWidth)
{
    std::string source = "نتيجة := اجمع(";
    for (int i = 0; i < 3; ++i) {
        if (i)
            source += "، ";
        source += "الوسيط_العربي_الطويل";
    }
    source += ")\n";
    expect_format(source, source);
}

TEST(Formatter, FormatsMultilineContainersAndNestedComments)
{
    expect_format(
        "دالة مثال():\n  ن:={\n'س':[\n1, # first\n# second\n2\n],\n'ع':3\n}\n  ارجع ن\n",
        "دالة مثال():\n    ن := {\n        'س': [\n            1،  # first\n            # second\n            2\n        ]،\n        'ع': 3\n    }\n    ارجع ن\n");
}

TEST(Formatter, PreservesClassesFieldsAndMethodCalls)
{
    expect_format("نوع مثال:\n  دالة بداية(ن):\n    هذا.قيمة:=ن\n  دالة خذ():\n    ارجع هذا.قيمة\n",
        "نوع مثال:\n    دالة بداية(ن):\n        هذا.قيمة := ن\n    دالة خذ():\n        ارجع هذا.قيمة\n");
}

TEST(Formatter, PreservesCRLFAndNormalizesTabs)
{
    expect_format("اذا صحيح:\r\n\tاكتب( 1,2 )  # hi\r\n",
        "اذا صحيح:\r\n    اكتب(1، 2)  # hi\r\n");
    expect_format("ن:=1", "ن := 1\n");
    expect_format("اذا صحيح:\n  ن:=1", "اذا صحيح:\n    ن := 1\n");
}

TEST(Formatter, FormatsEveryLibraryAndExampleIdempotently)
{
    auto root = std::filesystem::path(__FILE__).parent_path().parent_path();
    size_t count = 0;
    for (auto directory : { "stdlib", "examples" }) {
        for (auto const& entry : std::filesystem::recursive_directory_iterator(root / directory)) {
            if (!entry.is_regular_file() || entry.path().extension() != platform::path(".ف"))
                continue;
            SCOPED_TRACE(platform::utf8(entry.path()));
            std::ifstream input(entry.path(), std::ios::binary);
            std::string source { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
            auto once = format_source(source);
            EXPECT_EQ(format_source(once), once);
            ++count;
        }
    }
    EXPECT_GT(count, 50u);
}
