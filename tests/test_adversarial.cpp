#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char** environ;

namespace {

enum class Expectation { Output,
    AssignmentError,
    Error,
    NoCrash };

struct AdversarialCase {
    std::string name;
    std::string source;
    std::string output;
    Expectation expectation = Expectation::Output;
    bool check_only = false;
    bool require_empty_stdout = false;
};

// Each source runs in a child process: a VM crash must fail its own test,
// not prevent the remaining adversarial GoogleTests from running.
class AdversarialInterpreter : public ::testing::TestWithParam<AdversarialCase> {
protected:
    std::filesystem::path directory;

    void SetUp() override
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "fairuz-adversarial-XXXXXX").string();
        char* path = mkdtemp(pattern.data());
        ASSERT_NE(path, nullptr);
        directory = path;
    }

    void TearDown() override
    {
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
    }

    static std::string read(std::filesystem::path const& path)
    {
        std::ifstream file(path);
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }
};

TEST_P(AdversarialInterpreter, PreservesSemanticsAndReportsErrorsWithoutCrashing)
{
    auto const& test = GetParam();
    SCOPED_TRACE(test.name + "\nFairuz source:\n" + test.source);
    auto input = directory / "program.ف";
    auto output = directory / "stdout.txt";
    auto errors = directory / "stderr.txt";
    {
        std::ofstream file(input, std::ios::binary);
        file << test.source;
        ASSERT_TRUE(file.good());
    }
    std::string binary = (std::filesystem::path(__FILE__).parent_path().parent_path() / "build/fairuz").string();
    ASSERT_TRUE(std::filesystem::exists(binary)) << "Build the fairuz target before running these tests";
    std::string input_string = input.string();
    char check_flag[] = "--check";
    char* argv[] = { binary.data(), input_string.data(), test.check_only ? check_flag : nullptr, nullptr };
    posix_spawn_file_actions_t actions;
    ASSERT_EQ(posix_spawn_file_actions_init(&actions), 0);
    int setup_error = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (setup_error == 0)
        setup_error = posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, errors.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (setup_error == 0)
        setup_error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (setup_error != 0) {
        posix_spawn_file_actions_destroy(&actions);
        FAIL() << "Could not configure child descriptors: " << setup_error;
    }
    pid_t child = -1;
    int spawned = posix_spawn(&child, binary.c_str(), &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    ASSERT_EQ(spawned, 0);
    int status = 0;
    bool timed_out = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            break;
        if (waited < 0) {
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            FAIL() << "waitpid failed";
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            timed_out = true;
            kill(child, SIGKILL);
            waitpid(child, &status, 0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto out = read(output);
    auto err = read(errors);
    if (test.require_empty_stdout)
        EXPECT_TRUE(out.empty()) << "Execution continued past the expected error: " << out;
    ASSERT_FALSE(timed_out) << "Interpreter exceeded five seconds\n"
                            << err;
    ASSERT_TRUE(WIFEXITED(status)) << "Interpreter terminated by signal "
                                   << (WIFSIGNALED(status) ? WTERMSIG(status) : 0) << "\n"
                                   << err;
    EXPECT_EQ(err.find("AddressSanitizer"), std::string::npos) << err;
    EXPECT_EQ(err.find("UndefinedBehaviorSanitizer"), std::string::npos) << err;
    int exit_code = WEXITSTATUS(status);
    switch (test.expectation) {
    case Expectation::Output:
        ASSERT_EQ(exit_code, 0) << err;
        EXPECT_EQ(out, test.output);
        break;
    case Expectation::AssignmentError:
        EXPECT_NE(err.find("Assignment is a statement and cannot be used inside an expression"), std::string::npos) << err;
        EXPECT_TRUE(out.empty()) << "Rejected program executed: " << out;
        [[fallthrough]];
    case Expectation::Error:
        EXPECT_EQ(exit_code, 65) << "Expected a language diagnostic\nstdout: " << out << "\nstderr: " << err;
        break;
    case Expectation::NoCrash:
        EXPECT_TRUE(exit_code == 0 || exit_code == 65) << err;
        break;
    }
}

// These earlier probes now contain invalid syntax: assignment has no value.
void expect_statement_only_assignments(std::vector<AdversarialCase>& cases)
{
    for (auto& test : cases) {
        if (test.name == "IndexedAssignmentResultLifetime"
            || test.name == "LeftOperandSnapshot"
            || test.name == "IndexReadCapturesContainer"
            || test.name == "IndexWriteCapturesIndex"
            || test.name == "IndexWriteCapturesContainer"
            || test.name == "MemberWriteCapturesReceiver"
            || test.name == "IndexedAssignmentResultSurvivesCall"
            || test.name == "FunctionArgumentsCaptureEarlierValues"
            || test.name == "CallCapturesCalleeBeforeArguments"
            || test.name == "ListCapturesEarlierElements"
            || test.name == "DictionaryCapturesKeyBeforeValue"
            || test.name == "MethodReceiverCapturedBeforeArgumentRebinding"
            || test.name.starts_with("OperandSnapshot")) {
            test.expectation = Expectation::AssignmentError;
            test.output.clear();
        }
    }
}

std::vector<AdversarialCase> semantic_cases()
{
    std::vector<AdversarialCase> result {
        { "SmallIntegerSmoke", "اكتب(1 + 2)\n", "3\n" },
        { "LargeIntegerPrint", "اكتب(140737488355328)\n", "140737488355328\n" },
        { "LargeIntegerEquality", "ا := 140737488355328\nب := 140737488355328\nاكتب(ا = ب)\n", "صحيح\n" },
        { "LargeIntegerCancellation", "ا := 140737488355328\nاكتب(ا - ا)\n", "0\n" },
        { "LargeLiteralDoesNotEqualZero", "اكتب(281474976710656 = 0)\n", "خطا\n" },
        { "LargeLiteralAddition", "اكتب(281474976710656 + 1)\n", "281474976710657\n" },
        { "LargeLiteralNegation", "اكتب(-281474976710657)\n", "-281474976710657\n" },
        { "ConstantModuloCanIndexList", "ا := [10، 20، 30]\nاكتب(ا[5 % 3])\n", "30\n" },
        { "RuntimeModuloCanIndexList", "ا := [10، 20، 30]\nب := 5\nاكتب(ا[ب % 3])\n", "30\n" },
        { "ModuloTypesAgree", "ا := 5\nاكتب(صنف(5 % 3) = صنف(ا % 3))\n", "صحيح\n" },
        // Boxed integers extend the immediate range to signed 64-bit;
        // results outside that range must still be rejected.
        { "ConstantMultiplyOverflow", "اكتب(4294967296 * 4294967296)\n", "", Expectation::Error },
        { "RuntimeMultiplyOverflow", "ا := 4294967296\nاكتب(ا * ا)\n", "", Expectation::Error },
        { "ConstantDivideOverflow", "اكتب((-9223372036854775807 - 1) / -1)\n", "", Expectation::Error },
        { "RuntimeDivideOverflow", "ا := -9223372036854775807 - 1\nاكتب(ا / -1)\n", "", Expectation::Error },
        { "ConstantShiftOverflow", "اكتب(4 << 62)\n", "", Expectation::Error },
        { "RuntimeShiftOverflow", "ا := 4\nاكتب(ا << 62)\n", "", Expectation::Error },
        { "NegativePower", "اكتب(قوة(2، -1))\n", "0.5\n" },
        { "IntegerConversionDoesNotWrap", "اكتب(طبيعي(281474976710656.0))\n", "281474976710656\n" },
        { "NonfiniteIntegerConversion", "اكتب(طبيعي(1e300 * 1e300))\n", "", Expectation::Error },
        { "PowerNanDoesNotCrash", "اكتب(قوة(-1.0، 0.5))\n", "", Expectation::NoCrash },
        { "OperatorNanDoesNotCrash", "ا := -1.0\nاكتب(ا ** 0.5)\n", "", Expectation::NoCrash },
        { "InfinityDifferenceDoesNotCrash", "ا := 1e300 * 1e300\nاكتب(ا - ا)\n", "", Expectation::NoCrash },
        { "ExplicitErrorStopsExecution", "عطل(\"stop\")\nاكتب(\"continued\")\n", "", Expectation::Error },
        { "ShortCircuitSkipsUndefinedName", "اكتب(خطا و مفقود)\nاكتب(صحيح او مفقود)\n", "خطا\nصحيح\n" },
        { "ShortCircuitReturnsOperands", "اكتب(0 او 7)\nاكتب(2 و 9)\n", "7\n9\n" },
        { "NegativeZeroTruthAndEquality", "ا := -0.0\nاكتب(ليس ا)\nاكتب(ا = 0)\n", "صحيح\nصحيح\n" },
        { "DictionaryNumericKey", "د := {1: 10}\nد[1.0] := 20\nاكتب(طول(د))\nاكتب(د[1])\n", "1\n20\n" },
        { "DictionaryNegativeZeroKey", "د := {-0.0: 10}\nاكتب(د[0.0])\nاكتب(د[0])\n", "10\n10\n" },
        { "DictionaryBooleanKeyIsDistinct", "د := {1: 10، صحيح: 20}\nاكتب(طول(د))\nاكتب(د[1])\n", "2\n10\n" },
        { "NestedCollectionEquality", "اكتب([1، [2]] = [1.0، [2.0]])\nاكتب({\"a\": [1]} = {\"a\": [1]})\n", "صحيح\nصحيح\n" },
        { "CyclicCollectionEquality", "ا := []\nب := []\nاضف(ا، ا)\nاضف(ب، ب)\nاكتب(ا = ب)\nاضف(ب، 1)\nاكتب(ا = ب)\n", "صحيح\nخطا\n" },
        { "UnicodeIndexing", "ا := \"أ🙂ب\"\nاكتب(طول(ا))\nاكتب(ا[1])\n", "3\n🙂\n" },
        { "NestedCallRegisters", "دالة جمع(ا، ب، ج):\n    ارجع ا * 100 + ب * 10 + ج\nدالة هو(ا):\n    ارجع ا\nاكتب(جمع(هو(1)، هو(2)، هو(3)))\n", "123\n" },
        { "RecursiveLocals", "دالة مجموع(ن):\n    اذا ن = 0:\n        ارجع 0\n    س := ن\n    ارجع مجموع(ن - 1) + س\nاكتب(مجموع(100))\n", "5050\n" },
        { "ListEvaluationOrder", "سجل := []\nدالة علم(ن):\n    اضف(سجل، ن)\n    ارجع ن\nا := [علم(1)، علم(2)، علم(3)]\nاكتب(سجل)\n", "[1, 2, 3]\n" },
        { "GreaterThanEvaluationOrder", "سجل := []\nدالة علم(ن):\n    اضف(سجل، ن)\n    ارجع ن\nا := علم(1) > علم(2)\nاكتب(سجل)\n", "[1, 2]\n" },
        { "AugmentedIndexEvaluatedOnce", "عدد := [0]\nق := [10]\nدالة فهرس():\n    عدد[0] += 1\n    ارجع 0\nق[فهرس()] += 5\nاكتب(عدد[0])\nاكتب(ق[0])\n", "1\n15\n" },
        { "IndexedAssignmentResultLifetime", "دالة مثال():\n    ق := [0]\n    ارجع (ق[0] := 7) + (2 * 3)\nاكتب(مثال())\n", "13\n" },
        { "LeftOperandSnapshot", "دالة مثال():\n    س := 1\n    ارجع س + (س := 2)\nاكتب(مثال())\n", "3\n" },
        { "NestedBreakAndContinue", "س := 0\nلكل ا في [1، 2، 3، 4]:\n    اذا ا = 2:\n        اكمل\n    لكل ب في [10، 20، 30]:\n        اذا ب = 30:\n            اخرج\n        س += ا + ب\nاكتب(س)\n", "106\n" },
        { "EmptyLoopPreservesState", "س := 9\nلكل ا في []:\n    س := 0\nاكتب(س)\n", "9\n" },
        { "ReturnInsideLoop", "دالة مثال():\n    لكل ا في [1، 2، 3]:\n        اذا ا = 2:\n            ارجع ا\n    ارجع 0\nاكتب(مثال())\n", "2\n" },
        { "GreaterThanOverload", "نوع علبة:\n    دالة بداية(ق):\n        هذا.ق := ق\n    دالة اكبر_من(اخر):\n        ارجع هذا.ق > اخر.ق\nاكتب(علبة(9) > علبة(7))\n", "صحيح\n" },
        { "DivideByZero", "ا := 1\nاكتب(ا / 0)\n", "", Expectation::Error },
        { "ModuloByZero", "ا := 1\nاكتب(ا % 0)\n", "", Expectation::Error },
        { "NegativeShift", "ا := 1\nاكتب(ا << -1)\n", "", Expectation::Error },
        { "OversizedShift", "ا := 1\nاكتب(ا >> 64)\n", "", Expectation::Error },
        { "ListReadPastEnd", "اكتب([1][1])\n", "", Expectation::Error },
        { "ListReadNegativePastEnd", "اكتب([1][-2])\n", "", Expectation::Error },
        { "ListFloatIndex", "اكتب([1][0.0])\n", "", Expectation::Error },
        { "ListWritePastEnd", "ا := [1]\nا[1] := 2\n", "", Expectation::Error },
        { "EmptyPop", "احذف([])\n", "", Expectation::Error },
        { "InvalidNativeArgument", "اضف(1، 2)\n", "", Expectation::Error },
        { "CallNonCallable", "ا := 7\nا()\n", "", Expectation::Error },
        { "WrongFunctionArity", "دالة مثال(ا):\n    ارجع ا\nمثال()\n", "", Expectation::Error },
        { "UnsupportedNestedFunction", "دالة ا():\n    دالة ب():\n        ارجع 1\n    ارجع ب()\n", "", Expectation::Error },
        { "BreakOutsideLoop", "اخرج\n", "", Expectation::Error },
        { "ContinueOutsideLoop", "اكمل\n", "", Expectation::Error },
        { "UndefinedTimesZero", "اكتب(مفقود * 0)\n", "", Expectation::Error },
        { "FailedAssertion", "تاكد(خطا)\n", "", Expectation::Error },
    };
    expect_statement_only_assignments(result);
    return result;
}

// Follow-up scenarios are intentionally unverified. Each combines features
// that should preserve evaluation order, value identity, or dynamic dispatch.
std::vector<AdversarialCase> interaction_cases()
{
    std::vector<AdversarialCase> result {
        // Reading an object expression must capture that object before an
        // index expression rebinds the local that originally referred to it.
        { "IndexReadCapturesContainer", R"fa(دالة مثال():
    ق := [10]
    ارجع ق[(ق := [20])[0] - 20]
اكتب(مثال())
)fa",
            "10\n" },
        // The compiler documents object -> index -> value evaluation order.
        { "IndexWriteCapturesIndex", R"fa(دالة مثال():
    ق := [0، 0]
    س := 0
    ق[س] := (س := 1)
    ارجع ق
اكتب(مثال())
)fa",
            "[1, 0]\n" },
        { "IndexWriteCapturesContainer", R"fa(دالة مثال():
    ق := [10]
    قديم := ق
    ق[0] := (ق := [20])[0] + 1
    ارجع [قديم[0]، ق[0]]
اكتب(مثال())
)fa",
            "[21, 20]\n" },
        { "MemberWriteCapturesReceiver", R"fa(نوع علبة:
    دالة بداية(ق):
        هذا.ق := ق
دالة مثال():
    س := علبة(10)
    قديم := س
    س.ق := (س := علبة(20)).ق + 1
    ارجع [قديم.ق، س.ق]
اكتب(مثال())
)fa",
            "[21, 20]\n" },
        // Counting effects distinguishes evaluating an lvalue once from
        // expanding it into two independently evaluated expressions.
        { "AugmentedContainerCallEvaluatedOnce", R"fa(عدد := [0]
ق := [10]
دالة حاوية():
    عدد[0] += 1
    ارجع ق
حاوية()[0] += 5
اكتب(عدد[0])
اكتب(ق[0])
)fa",
            "1\n15\n" },
        { "AugmentedIndexUsesOriginalElement", R"fa(عدد := [0]
ق := [10، 100]
دالة التالي():
    س := عدد[0]
    عدد[0] += 1
    ارجع س
ق[التالي()] += 5
اكتب(ق)
اكتب(عدد[0])
)fa",
            "[15, 100]\n1\n" },
        { "AugmentedMemberReceiverEvaluatedOnce", R"fa(نوع علبة:
    دالة بداية(ق):
        هذا.ق := ق
عدد := [0]
حاويات := [علبة(10)]
دالة خذ():
    عدد[0] += 1
    ارجع حاويات[0]
خذ().ق += 5
اكتب(عدد[0])
اكتب(حاويات[0].ق)
)fa",
            "1\n15\n" },
        { "IndexedAssignmentResultSurvivesCall", R"fa(دالة هو(س):
    ارجع س
دالة مثال():
    ق := [0، 0]
    ق[0] := (ق[1] := 7) + هو(3)
    ارجع ق
اكتب(مثال())
)fa",
            "[10, 7]\n" },
        // These are controls for the operand-snapshot probes: calls and
        // collection construction also evaluate multiple subexpressions.
        { "FunctionArgumentsCaptureEarlierValues", R"fa(دالة زوج(ا، ب):
    ارجع [ا، ب]
دالة مثال():
    س := 1
    ارجع زوج(س، س := 2)
اكتب(مثال())
)fa",
            "[1, 2]\n" },
        { "CallCapturesCalleeBeforeArguments", R"fa(دالة اول(س):
    ارجع 10
دالة ثان(س):
    ارجع 20
دالة مثال():
    ف := اول
    ارجع ف(ف := ثان)
اكتب(مثال())
)fa",
            "10\n" },
        { "ListCapturesEarlierElements", R"fa(دالة مثال():
    س := 1
    ارجع [س، س := 2، س]
اكتب(مثال())
)fa",
            "[1, 2, 2]\n" },
        { "DictionaryCapturesKeyBeforeValue", R"fa(دالة مثال():
    س := 1
    د := {س: س := 2}
    ارجع [د[1]، س]
اكتب(مثال())
)fa",
            "[2, 2]\n" },
        { "DuplicateDictionaryKeysPreserveSideEffects", R"fa(سجل := []
دالة علم(س):
    اضف(سجل، س)
    ارجع س
د := {علم(1): علم(10)، علم(1): علم(20)}
اكتب(سجل)
اكتب(طول(د))
اكتب(د[1])
)fa",
            "[1, 10, 1, 20]\n1\n20\n" },
        // Different field layouts expose stale inferred types after a local
        // variable is assigned an instance of another class.
        { "FieldReadAfterClassReassignment", R"fa(نوع اول:
    دالة بداية():
        هذا.هدف := 10
نوع ثان:
    دالة بداية():
        هذا.حاجز := 99
        هذا.هدف := 20
س := اول()
س := ثان()
اكتب(س.هدف)
)fa",
            "20\n" },
        { "FieldWriteAfterClassReassignment", R"fa(نوع اول:
    دالة بداية():
        هذا.هدف := 10
نوع ثان:
    دالة بداية():
        هذا.حاجز := 99
        هذا.هدف := 20
س := اول()
س := ثان()
س.هدف := 7
اكتب(س.حاجز)
اكتب(س.هدف)
)fa",
            "99\n7\n" },
        { "FieldReadAfterConditionalClassReassignment", R"fa(نوع اول:
    دالة بداية():
        هذا.هدف := 10
نوع ثان:
    دالة بداية():
        هذا.حاجز := 99
        هذا.هدف := 20
س := اول()
شرط := صحيح
اذا شرط:
    س := ثان()
اكتب(س.هدف)
)fa",
            "20\n" },
        { "InternalMethodNestedArgumentsStayContiguous", R"fa(نوع حاسب:
    دالة هو(س):
        ارجع س
    دالة جمع(ا، ب، ج):
        ارجع ا * 100 + ب * 10 + ج
    دالة نفذ():
        ارجع هذا.جمع(هذا.هو(1)، هذا.هو(2)، هذا.هو(3))
اكتب(حاسب().نفذ())
)fa",
            "123\n" },
        { "ExternalMethodNestedArgumentsStayContiguous", R"fa(نوع حاسب:
    دالة هو(س):
        ارجع س
    دالة جمع(ا، ب، ج):
        ارجع ا * 100 + ب * 10 + ج
س := حاسب()
اكتب(س.جمع(س.هو(1)، س.هو(2)، س.هو(3)))
)fa",
            "123\n" },
        { "MethodReceiverCapturedBeforeArgumentRebinding", R"fa(نوع علبة:
    دالة بداية(ق):
        هذا.ق := ق
    دالة خذ(مهمل):
        ارجع هذا.ق
دالة مثال():
    س := علبة(10)
    ارجع س.خذ(س := علبة(20))
اكتب(مثال())
)fa",
            "10\n" },
        { "GreaterEqualUsesRegisteredOverload", R"fa(نوع علبة:
    دالة بداية(ق):
        هذا.ق := ق
    دالة اكبر_او_يساوي(اخر):
        ارجع هذا.ق >= اخر.ق
اكتب(علبة(9) >= علبة(7))
)fa",
            "صحيح\n" },
        // Both implementations produce the same boolean for these operands;
        // the event log reveals which overload was actually dispatched.
        { "GreaterThanCallsItsOwnOverload", R"fa(سجل := []
نوع علبة:
    دالة بداية(ق):
        هذا.ق := ق
    دالة اصغر_من(اخر):
        اضف(سجل، "lt")
        ارجع هذا.ق < اخر.ق
    دالة اكبر_من(اخر):
        اضف(سجل، "gt")
        ارجع هذا.ق > اخر.ق
اكتب(علبة(9) > علبة(7))
اكتب(اجمع(سجل، ","))
)fa",
            "صحيح\ngt\n" },
        { "PolymorphicMethodCallSite", R"fa(نوع اول:
    دالة خذ():
        ارجع 10
نوع ثان:
    دالة حاجز():
        ارجع 99
    دالة خذ():
        ارجع 20
دالة نداء(س):
    ارجع س.خذ()
ا := اول()
ب := ثان()
اكتب(نداء(ا))
اكتب(نداء(ب))
اكتب(نداء(ا))
)fa",
            "10\n20\n10\n" },
        { "PolymorphicBinaryCallSite", R"fa(دالة جمع(ا، ب):
    ارجع ا + ب
اكتب(جمع(1، 2))
اكتب(جمع("a"، "b"))
اكتب(جمع(1.5، 2.5))
اكتب(جمع(3، 4))
)fa",
            "3\nab\n4\n7\n" },
        { "ForIterableExpressionEvaluatedOnce", R"fa(عدد := [0]
دالة عناصر():
    عدد[0] += 1
    ارجع [1، 2، 3]
مجموع := 0
لكل س في عناصر():
    مجموع += س
اكتب(مجموع)
اكتب(عدد[0])
)fa",
            "6\n1\n" },
        { "WhileConditionRunsBeforeEachIteration", R"fa(عدد := [0]
دالة شرط():
    عدد[0] += 1
    ارجع عدد[0] <= 3
مجموع := 0
طالما شرط():
    مجموع += 10
اكتب(مجموع)
اكتب(عدد[0])
)fa",
            "30\n4\n" },
        { "ListAliasesSurviveGrowth", R"fa(ق := [7]
نسخة := ق
س := 0
طالما س < 2048:
    اضف(ق، س)
    س += 1
نسخة[0] := 9
اكتب(طول(نسخة))
اكتب(ق[0])
اكتب(نسخة[2048])
)fa",
            "2049\n9\n2047\n" },
        { "DictionaryValuesSurviveGrowth", R"fa(د := {}
ق := [7]
د[0] := ق
س := 1
طالما س < 1024:
    د[س] := س
    س += 1
ق[0] := 9
اكتب(طول(د))
اكتب(د[0][0])
اكتب(د[1023])
)fa",
            "1024\n9\n1023\n" },
        { "ListAccessFailureStopsLaterSideEffects", R"fa(ق := []
اكتب(ق[0])
اكتب("must not execute")
)fa",
            "", Expectation::Error, false, true },
    };

    // A family of minimal local-variable probes. Save the left operand's
    // value before evaluating an assignment on the right. The different
    // operators distinguish wrong arithmetic from wrong comparisons.
    struct SnapshotProbe {
        char const* name;
        char const* expression;
        char const* expected;
    };
    SnapshotProbe probes[] = {
        { "Subtract", "س - (س := 2)", "-1\n" },
        { "Multiply", "س * (س := 3)", "3\n" },
        { "Divide", "س / (س := 2)", "0.5\n" },
        { "Equal", "س = (س := 2)", "خطا\n" },
        { "LessThan", "س < (س := 2)", "صحيح\n" },
        { "GreaterThan", "س > (س := 0)", "صحيح\n" },
        { "BitOr", "س | (س := 2)", "3\n" },
    };
    for (auto const& probe : probes) {
        result.push_back({ std::string("OperandSnapshot") + probe.name,
            std::string("دالة مثال():\n    س := 1\n    ارجع ") + probe.expression + "\nاكتب(مثال())\n",
            probe.expected });
    }
    expect_statement_only_assignments(result);
    return result;
}

std::vector<AdversarialCase> assignment_syntax_cases()
{
    struct InvalidAssignment {
        char const* name;
        char const* source;
    };
    InvalidAssignment invalid[] = {
        { "Binary", "اكتب(1 + (س := 2))\n" },
        { "Argument", "اكتب(س := 2)\n" },
        { "Condition", "اذا س := 2:\n    اكتب(س)\n" },
        { "Return", "دالة مثال():\n    ارجع س := 2\nمثال()\n" },
        { "List", "ق := [س := 2]\n" },
        { "Index", "ق := [0]\nق[س := 0] := 2\n" },
        { "Augmented", "س := 1\nاكتب(س += 2)\n" },
        { "ShortCircuit", "اكتب(خطا و (س := 2))\n" },
        { "ClassField", "نوع علبة:\n    دالة بداية():\n        .حقل := (س := 2)\n" },
        { "ClassAugmentedField", "نوع علبة:\n    دالة بداية():\n        .حقل += س := 2\n" },
    };
    std::vector<AdversarialCase> result;
    for (auto const& test : invalid) {
        for (bool check_only : { false, true }) {
            result.push_back({ std::string(test.name) + (check_only ? "Check" : "Run"),
                std::string("اكتب(\"must not run\")\n") + test.source,
                "", Expectation::AssignmentError, check_only });
        }
    }
    result.push_back({ "StandaloneAndChained",
        "ا := ب := 5\nا += 2\nاكتب(ا)\nاكتب(ب)\nاكتب(ا = 7)\n", "7\n5\nصحيح\n" });
    return result;
}

std::vector<AdversarialCase> arithmetic_cases()
{
    constexpr int64_t low = -(INT64_C(1) << 47), high = (INT64_C(1) << 47) - 1;
    int64_t boundary[] = { low, low + 1, -4294967296, -1, 0, 1, 2, 4294967296, high - 1, high };
    char const* operators[] = { "+", "-", "*", "%", "=", "<", "<=", ">", ">=" };
    std::mt19937 random(20260921);
    std::vector<AdversarialCase> result;
    for (int i = 0; i < 100; ++i) {
        int64_t a = i < 50 ? boundary[random() % 10] : static_cast<int64_t>(random() % 200001) - 100000;
        int64_t b = i < 50 ? boundary[random() % 10] : static_cast<int64_t>(random() % 200001) - 100000;
        unsigned op = random() % 9;
        if (op == 3 && b == 0)
            b = 1;
        // The oracle deliberately uses wider arithmetic, so multiplying two
        // legal 48-bit operands cannot overflow the C++ reference calculation.
        __int128 value = 0;
        switch (op) {
        case 0: value = static_cast<__int128>(a) + b; break;
        case 1: value = static_cast<__int128>(a) - b; break;
        case 2: value = static_cast<__int128>(a) * b; break;
        case 3: value = a % b; break;
        case 4: value = a == b; break;
        case 5: value = a < b; break;
        case 6: value = a <= b; break;
        case 7: value = a > b; break;
        case 8: value = a >= b; break;
        }
        bool overflow = value < INT64_MIN || value > INT64_MAX;
        std::string expected = overflow ? "" : op >= 4 ? (value ? "صحيح\n" : "خطا\n")
                                                       : std::to_string(static_cast<int64_t>(value)) + "\n";
        for (bool literal : { true, false }) {
            auto source = literal
                ? "اكتب((" + std::to_string(a) + ") " + operators[op] + " (" + std::to_string(b) + "))\n"
                : "ا := " + std::to_string(a) + "\nب := " + std::to_string(b) + "\nاكتب(ا " + operators[op] + " ب)\n";
            result.push_back({ "Case" + std::to_string(i) + (literal ? "Literal" : "Runtime"), source, expected,
                overflow ? Expectation::Error : Expectation::Output });
        }
    }
    return result;
}

std::vector<AdversarialCase> repair_regression_cases()
{
    std::vector<AdversarialCase> result {
        { "ArgumentParserDefaultMayBeNil", "من محلل_وسائط استورد محلل_وسائط\nم := محلل_وسائط(\"app\"، \"\")\nم.اضف_موضع(\"x\"، {\"default\": عدم})\nاكتب(م.حلل([])[\"x\"])\n", "عدم\n" },
        { "ArgumentParserRequiredOverridesDefault", "من محلل_وسائط استورد محلل_وسائط\nم := محلل_وسائط(\"app\"، \"\")\nم.اضف_موضع(\"x\"، {\"default\": 7، \"required\": صحيح})\nم.حلل([])\nاكتب(99)\n", "", Expectation::Error, false, true },
        { "ArgumentParserPositionWithoutDefaultRequired", "من محلل_وسائط استورد محلل_وسائط\nم := محلل_وسائط(\"app\"، \"\")\nم.اضف_موضع(\"x\"، {})\nم.حلل([])\nاكتب(99)\n", "", Expectation::Error, false, true },
        { "BoxedArithmetic", "س := 281474976710656\nاكتب(س + 1)\nاكتب(س - 1)\nاكتب(س * 2)\nاكتب(س / 1)\nاكتب(س % (س + 1))\n",
            "281474976710657\n281474976710655\n562949953421312\n281474976710656\n281474976710656\n" },
        { "BoxedBitwise", "س := 281474976710656\nاكتب(س | 1)\nاكتب(س ^ 1)\nاكتب(س & س)\nاكتب(~س)\nاكتب(س >> 0)\n",
            "281474976710657\n281474976710657\n281474976710656\n-281474976710657\n281474976710656\n" },
        { "ImmediateArithmeticPromotes", "اكتب(140737488355327 + 1)\nاكتب((-140737488355328) / -1)\nاكتب(1 << 48)\n",
            "140737488355328\n140737488355328\n281474976710656\n" },
        { "IntegerConversionPreservesPrecision", "اكتب(طبيعي(9223372036854775807))\n", "9223372036854775807\n" },
        { "IntegerConversionRejectsUpperBound", "اكتب(طبيعي(9223372036854775808.0))\n", "", Expectation::Error },
        { "IntegerConversionRejectsLowerOverflow", "اكتب(طبيعي(-18446744073709551616.0))\n", "", Expectation::Error },
        { "IntegerConversionAcceptsLowerBound", "اكتب(طبيعي(-9223372036854775808.0))\n", "-9223372036854775808\n" },
        { "MinimumIntegerModuloMinusOne", "س := -9223372036854775807 - 1\nاكتب(س % -1)\n", "0\n" },
        { "MinimumIntegerNegationOverflow", "س := -9223372036854775807 - 1\nاكتب(-س)\n", "", Expectation::Error },
        { "DynamicShiftsUseValueNotRegister", "س := 3\nز := 99\nن := 5\nاكتب(س << ن)\nاكتب(96 >> ن)\n", "96\n3\n" },
        { "DynamicShiftNegative", "ن := -1\nاكتب(1 << ن)\n", "", Expectation::Error },
        { "DynamicShiftOversized", "ن := 64\nاكتب(1 >> ن)\n", "", Expectation::Error },
        { "DynamicShiftNonInteger", "ن := 1.5\nاكتب(1 << ن)\n", "", Expectation::Error },
        { "AugmentedReadBeforeRhsMutation", "ق := [10]\nدالة غير():\n    ق[0] := 100\n    ارجع 5\nق[0] += غير()\nاكتب(ق[0])\n", "15\n" },
        { "AugmentedReadErrorStopsRhs", "ق := []\nدالة اثر():\n    اكتب(99)\n    ارجع 5\nق[0] += اثر()\n", "", Expectation::Error, false, true },
        { "FieldWriteAcrossLoopBackedge", R"fa(نوع اول:
    دالة بداية():
        هذا.هدف := 10
نوع ثان:
    دالة بداية():
        هذا.حاجز := 99
        هذا.هدف := 20
س := اول()
ن := 0
طالما ن < 2:
    س.هدف := 7
    اذا ن = 0:
        س := ثان()
    ن += 1
اكتب(س.حاجز)
اكتب(س.هدف)
)fa",
            "99\n7\n" },
        { "FieldWriteAfterUntakenBranch", R"fa(نوع اول:
    دالة بداية():
        هذا.هدف := 10
نوع ثان:
    دالة بداية():
        هذا.حاجز := 99
        هذا.هدف := 20
س := اول()
اذا خطا:
    س := ثان()
س.هدف := 7
اكتب(س.هدف)
)fa",
            "7\n" },
    };
    struct AugmentedCase {
        char const* name;
        char const* op;
        char const* expected;
    };
    AugmentedCase operators[] = {
        { "Add", "+=", "14" },
        { "Subtract", "-=", "10" },
        { "Multiply", "*=", "24" },
        { "Divide", "/=", "6" },
        { "Modulo", "%=", "0" },
        { "And", "&=", "0" },
        { "Or", "|=", "14" },
        { "Xor", "^=", "14" },
        { "ShiftLeft", "<<=", "48" },
        { "ShiftRight", ">>=", "3" },
    };
    for (auto const& op : operators) {
        result.push_back({ std::string("AugmentedIndex") + op.name,
            std::string("ق := [12]\nعدد := [0]\nدالة فهرس():\n    عدد[0] += 1\n    ارجع 0\nن := 2\nق[فهرس()] ")
                + op.op + " ن\nاكتب(ق[0])\nاكتب(عدد[0])\n",
            std::string(op.expected) + "\n1\n" });
    }
    return result;
}

std::vector<AdversarialCase> parser_cases()
{
    std::vector<AdversarialCase> result;
    for (int count : { 64, 256, 1024 }) {
        result.push_back({ "NestedParentheses" + std::to_string(count),
            "اكتب(" + std::string(count, '(') + "1" + std::string(count, ')') + ")\n", "", Expectation::NoCrash, true });
        std::string arguments = "اكتب(1";
        for (int i = 1; i < count; ++i)
            arguments += "، 1";
        result.push_back({ "ArgumentLimit" + std::to_string(count), arguments + ")\n", "", Expectation::NoCrash, true });
    }
    std::string seeds[] = { "اكتب(1 + 2)\n", "ا := [1، 2، 3]\nاكتب(ا[0])\n", "اذا صحيح:\n    اكتب(1)\n", "دالة ا(ب):\n    ارجع ب + 1\nاكتب(ا(2))\n" };
    std::vector<std::string> mutations = { "(", ")", "[", "]", "{", "}", ":", "،", "\n", "\t", "\"", "'", "0", "9", "+", "*", "=", "ا", std::string(1, '\0'), "\xff" };
    std::mt19937 random(20260921);
    for (int i = 0; i < 60; ++i) {
        auto source = seeds[random() % 4];
        unsigned count = 1 + random() % 4;
        for (unsigned j = 0; j < count; ++j) {
            size_t at = random() % (source.size() + 1);
            source.insert(at, mutations[random() % mutations.size()]);
        }
        result.push_back({ "Mutation" + std::to_string(i), source, "", Expectation::NoCrash, true });
    }
    return result;
}

auto case_name = [](testing::TestParamInfo<AdversarialCase> const& info) { return info.param.name; };
INSTANTIATE_TEST_SUITE_P(Semantics, AdversarialInterpreter, testing::ValuesIn(semantic_cases()), case_name);
INSTANTIATE_TEST_SUITE_P(Arithmetic, AdversarialInterpreter, testing::ValuesIn(arithmetic_cases()), case_name);
INSTANTIATE_TEST_SUITE_P(Parser, AdversarialInterpreter, testing::ValuesIn(parser_cases()), case_name);
INSTANTIATE_TEST_SUITE_P(Interactions, AdversarialInterpreter, testing::ValuesIn(interaction_cases()), case_name);
INSTANTIATE_TEST_SUITE_P(AssignmentSyntax, AdversarialInterpreter, testing::ValuesIn(assignment_syntax_cases()), case_name);
INSTANTIATE_TEST_SUITE_P(RepairRegression, AdversarialInterpreter, testing::ValuesIn(repair_regression_cases()), case_name);

} // namespace
