#include <gtest/gtest.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

struct PublicApiCase {
    char const* name;
    char const* source;
    char const* error = nullptr;
};

// Exercise imports, the compiler, VM, public wrappers, and native dispatch in
// a real CLI process. Runtime errors must not terminate the GoogleTest runner.
// Each process gets its own working directory, including for relative paths
// and globbing, so tests neither share /tmp filenames nor depend on test order.
class StdlibE2E : public ::testing::TestWithParam<PublicApiCase> {
protected:
    std::filesystem::path directory;

    void SetUp() override
    {
        std::string pattern = (std::filesystem::temp_directory_path()
            / "fairuz-stdlib-e2e-XXXXXX")
                                  .string();
        std::vector<char> writable(pattern.begin(), pattern.end());
        writable.push_back('\0');
        char* created = ::mkdtemp(writable.data());
        ASSERT_NE(created, nullptr);
        directory = created;
        std::filesystem::create_directory(directory / "nested");
        ASSERT_TRUE(std::filesystem::is_regular_file(FAIRUZ_TEST_EXECUTABLE));
    }

    void TearDown() override
    {
        if (!directory.empty()) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    std::string read(char const* name) const
    {
        std::ifstream stream(directory / name, std::ios::binary);
        return { std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
    }

    void run(PublicApiCase const& test)
    {
        {
            std::ofstream program(directory / "main.ف");
            program << "اكتب(\"stdlib-e2e-start\")\n"
                    << test.source
                    << "\nاكتب(\"stdlib-e2e-ok\")\n";
            ASSERT_TRUE(program.good());
        }

        pid_t child = ::fork();
        ASSERT_GE(child, 0);
        if (child == 0) {
            if (::chdir(directory.c_str()) != 0
                || std::freopen("stdout", "w", stdout) == nullptr
                || std::freopen("stderr", "w", stderr) == nullptr)
                ::_exit(126);
            // Keep ASan active, without nested leak-at-exit scans on macOS.
            if (::setenv("ASAN_OPTIONS", "detect_leaks=0", 1) != 0
                || ::setenv("FAIRUZ_STDLIB", FAIRUZ_TEST_STDLIB_DIR, 1) != 0
                || ::setenv("NO_COLOR", "1", 1) != 0)
                ::_exit(126);
            ::execl(FAIRUZ_TEST_EXECUTABLE, FAIRUZ_TEST_EXECUTABLE,
                "main.ف", "--diagnostics=text", static_cast<char*>(nullptr));
            ::_exit(127);
        }

        int status = 0;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        for (;;) {
            pid_t waited = ::waitpid(child, &status, WNOHANG);
            if (waited == child)
                break;
            if (waited < 0 && errno != EINTR) {
                FAIL() << "waitpid failed: " << errno;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                ::kill(child, SIGKILL);
                while (::waitpid(child, &status, 0) < 0 && errno == EINTR) { }
                FAIL() << "Interpreter timed out\n"
                       << read("stderr");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        std::string output = read("stdout");
        std::string error = read("stderr");
        ASSERT_TRUE(WIFEXITED(status)) << "Interpreter crashed: " << status << '\n'
                                       << error;
        if (test.error == nullptr) {
            EXPECT_EQ(WEXITSTATUS(status), 0) << error;
            EXPECT_EQ(output, "stdlib-e2e-start\nstdlib-e2e-ok\n") << error;
            EXPECT_TRUE(error.empty()) << error;
        } else {
            EXPECT_EQ(WEXITSTATUS(status), 65) << error;
            EXPECT_EQ(output, "stdlib-e2e-start\n") << "Execution continued after failure";
            EXPECT_NE(error.find(test.error), std::string::npos) << error;
            EXPECT_EQ(error.find("AddressSanitizer"), std::string::npos) << error;
        }
    }
};

TEST_P(StdlibE2E, ExecutesPublicApi)
{
    run(GetParam());
}

PublicApiCase const cases[] = {
    { "FileLifecycleAndFailureResults", R"fa(
من ملفات استورد ملف، اقرا_نص_ملف، اكتب_نص_ملف، انسخ_ملف، مع_ملف
دالة لا_يجب_استدعاؤها(المورد):
    عطل("callback must not run for a missing file")
المورد := ملف("missing.txt"، "قراءة")
تاكد(ليس المورد.فتح())
تاكد(ليس المورد.يعمل())
تاكد(المورد.اقرا_الكل() = عدم)
تاكد(ليس المورد.اكتب_نص("x"))
تاكد(المورد.اغلق())
تاكد(اقرا_نص_ملف("missing.txt") = عدم)
تاكد(ليس انسخ_ملف("missing.txt"، "copy.txt"))
تاكد(اقرا_نص_ملف("copy.txt") = عدم)
تاكد(ليس اكتب_نص_ملف("absent/file.txt"، "x"))
النتيجة := مع_ملف("missing.txt"، "قراءة"، لا_يجب_استدعاؤها)
تاكد(النتيجة.فشل())
تاكد(النتيجة.او_بديل("fallback") = "fallback")
تاكد(اكتب_نص_ملف("missing.txt"، "created"))
تاكد(المورد.فتح())
تاكد(المورد.فتح())
تاكد(المورد.اقرا_الكل() = "created")
تاكد(المورد.اغلق())
تاكد(المورد.اغلق())
تاكد(ليس المورد.ادفع())
)fa" },
    { "BinaryFilesAndEof", R"fa(
من ملفات استورد ملف، اكتب_نص_ملف، اقرا_نص_ملف، مع_ملف
من ترميزات استورد hex_فك، hex_رمز
البيانات := hex_فك("4100ff420a")
تاكد(اكتب_نص_ملف("bytes.bin"، البيانات))
تاكد(hex_رمز(اقرا_نص_ملف("bytes.bin")) = "4100ff420a")
دالة اقرا_بايتات(المورد):
    تاكد(المورد.اقرا(0) = "")
    تاكد(hex_رمز(المورد.اقرا(2)) = "4100")
    تاكد(hex_رمز(المورد.اقرا_الكل()) = "ff420a")
    تاكد(المورد.اقرا(1) = "")
    تاكد(المورد.اقرا_سطر() = عدم)
    ارجع صحيح
تاكد(مع_ملف("bytes.bin"، "قراءة"، اقرا_بايتات).افتح())
)fa" },
    { "FilesystemGlobsAndTemporaryCleanup", R"fa(
من ملفات استورد اكتب_نص_ملف، اقرا_نص_ملف
من نظام_الملفات استورد glob، glob_متكرر، ملف_مؤقت، مجلد_مؤقت، احذف_شجرة
تاكد(اكتب_نص_ملف("top.txt"، "top"))
تاكد(اكتب_نص_ملف("nested/child.txt"، "child"))
تاكد(طول(glob("*.txt")) = 1)
تاكد(طول(glob_متكرر("*.txt")) = 2)
تاكد(طول(glob("*.absent")) = 0)
مؤقت := ملف_مؤقت("owned-"، ".tmp"، ".")
المسار := مؤقت.المسار.نص()
تاكد(مؤقت.الملف.اكتب_نص("temporary"))
تاكد(مؤقت.الملف.ادفع())
تاكد(اقرا_نص_ملف(المسار) = "temporary")
مؤقت.اغلق()
تاكد(اقرا_نص_ملف(المسار) = عدم)
تاكد(طول(glob("owned-*.tmp")) = 0)
المجلد := مجلد_مؤقت("owned-dir-"، ".")
تاكد(طول(glob("owned-dir-*")) = 1)
المجلد.اغلق()
تاكد(طول(glob("owned-dir-*")) = 0)
تاكد(احذف_شجرة("nested"))
تاكد(اقرا_نص_ملف("nested/child.txt") = عدم)
)fa" },
    { "JsonCompressionEncodingFilePipeline", R"fa(
من جيسون استورد JSON_رمز، JSON_حلل
من ضغط استورد gzip، gunzip
من ترميزات استورد base64_رمز، base64_فك
من ملفات استورد اكتب_نص_ملف، اقرا_نص_ملف
القيمة := {"معرف": 2 ** 100، "نص": "مرحبا\n🙂"، "قيم": [عدم، صحيح، 3.5]}
النص := JSON_رمز(القيمة)
تاكد(اكتب_نص_ملف("payload.txt"، base64_رمز(gzip(النص، 6))))
المستعاد := gunzip(base64_فك(اقرا_نص_ملف("payload.txt"))، 4096)
تاكد(JSON_حلل(المستعاد) = القيمة)
)fa" },
    { "CodecsRejectMalformedInput", R"fa(
من ترميزات استورد base64_رمز، base64_فك، base64_URL_رمز، base64_URL_فك، hex_رمز، hex_فك، هاش، SHA256، HMAC
البيانات := hex_فك("fbff00")
تاكد(base64_رمز(البيانات) = "+/8A")
تاكد(base64_URL_رمز(البيانات) = "-_8A")
تاكد(hex_رمز(base64_فك("+/8A")) = "fbff00")
تاكد(hex_رمز(base64_URL_فك("-_8A")) = "fbff00")
تاكد(base64_فك("not base64!") = عدم)
تاكد(hex_فك("abc") = عدم)
تاكد(hex_فك("gg") = عدم)
تاكد(base64_فك("") = "")
تاكد(hex_فك("") = "")
تاكد(SHA256("") = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
تاكد(hex_رمز(هاش("sha256").حدث("abc").بايتات()) = SHA256("abc"))
تاكد(HMAC("unsupported"، "key"، "data") = عدم)
)fa" },
    { "CompressionLimitsAndCorruptStreams", R"fa(
من ضغط استورد اضغط، gzip، gunzip
من ترميزات استورد hex_فك
البيانات := hex_فك("4100ff42")
المضغوط := gzip(البيانات، 6)
تاكد(gunzip(المضغوط، 4) = البيانات)
تاكد(gunzip(المضغوط، 3) = عدم)
تاكد(gunzip("corrupt"، 100) = عدم)
تاكد(gzip("x"، -1) = عدم)
تاكد(gzip("x"، 10) = عدم)
تاكد(اضغط("unsupported"، "x"، 6) = عدم)
تاكد(gunzip(gzip(""، 0)، 1) = "")
)fa" },
    { "IniFileResultsAndCsvRoundTrip", R"fa(
من ملفات استورد اكتب_نص_ملف
من إعدادات استورد اقرا_INI
من قيم_مفصولة استورد CSV_رمز، CSV_حلل
تاكد(اقرا_INI("missing.ini").فشل())
تاكد(اكتب_نص_ملف("app.ini"، "timeout=30\n[server]\nname=فيروز\n"))
الاعدادات := اقرا_INI("app.ini").افتح()
تاكد(الاعدادات.خذ("server"، "name"، عدم) = "فيروز")
تاكد(الاعدادات.خذ("server"، "timeout"، عدم) = "30")
تاكد(الاعدادات.خذ("server"، "missing"، "fallback") = "fallback")
الصفوف := [["مرحبا"، "a,b"، "quote\""، "line\nbreak"، ""]]
تاكد(CSV_حلل(CSV_رمز(الصفوف، ",")، ",") = الصفوف)
)fa" },
    { "LazyIteratorsAndResultCallbacks", R"fa(
من مكررات استورد مدى_كسول، خريطة_كسولة، رشح_كسول، الى_قائمة
من نتيجة استورد نجاح، فشل، بعض، لاشيء
دالة ضعف(القيمة):
    ارجع القيمة * 2
دالة كبير(القيمة):
    ارجع القيمة > 4
دالة لا_يجب_استدعاؤها(القيمة):
    عطل("callback must not run for an empty result")
المصدر := رشح_كسول(خريطة_كسولة(مدى_كسول(1، 5، 1)، ضعف)، كبير)
تاكد(الى_قائمة(المصدر) = [6، 8])
تاكد(ليس المصدر.التالي().له_قيمة())
تاكد(ليس المصدر.التالي().له_قيمة())
تاكد(نجاح(3).حول(ضعف).افتح() = 6)
تاكد(فشل("bad").حول(لا_يجب_استدعاؤها).فشل())
تاكد(بعض(3).حول(ضعف).افتح() = 6)
تاكد(لاشيء().حول(لا_يجب_استدعاؤها).او_بديل(9) = 9)
)fa" },
    { "TaskResultsOrderAndCompletedCancellation", R"fa(
من لا_متزامن استورد منفذ_مهام
دالة ضعف(القيمة):
    ارجع القيمة * 2
المنفذ := منفذ_مهام(2)
تاكد(المنفذ.انتظر_الكل(0) = [])
الاول := المنفذ.قدم(ضعف، [3])
الثاني := المنفذ.قدم(ضعف، [7])
تاكد(الاول.تم())
تاكد(ليس الاول.الغي())
تاكد(الاول.نتيجة(0) = 6)
تاكد(الثاني.نتيجة(0) = 14)
تاكد(المنفذ.انتظر_الكل(0) = [6، 14])
تاكد(المنفذ.اغلق(صحيح))
)fa" },
    { "JsonTrailingData", R"fa(
من جيسون استورد JSON_حلل
JSON_حلل("{} trailing")
)fa",
        "بيانات زائدة بعد قيمة JSON" },
    { "JsonInvalidEscape", R"fa(
من جيسون استورد JSON_حلل
JSON_حلل("\"\\q\"")
)fa",
        "سلسلة JSON غير صالحة" },
    { "JsonCyclicInput", R"fa(
من جيسون استورد JSON_رمز
القيم := []
اضف(القيم، القيم)
JSON_رمز(القيم)
)fa",
        "لا يمكن ترميز بنية دورية إلى JSON" },
    { "JsonNonStringKeys", R"fa(
من جيسون استورد JSON_رمز
JSON_رمز({1: "value"})
)fa",
        "مفاتيح JSON يجب أن تكون سلاسل" },
    { "CsvInvalidSeparator", R"fa(
من قيم_مفصولة استورد CSV_حلل
CSV_حلل("a,b"، "::")
)fa",
        "فاصل CSV يجب أن يكون حرفاً واحداً" },
    { "CsvUnterminatedQuote", R"fa(
من قيم_مفصولة استورد CSV_حلل
CSV_حلل("\"unterminated"، ",")
)fa",
        "حقل CSV مقتبس لم يغلق" },
    { "RegexInvalidPattern", R"fa(
من تعابير_نمطية استورد نمط
نمط("["، 0)
)fa",
        "تعبير نمطي غير صالح" },
    { "FileNegativeRead", R"fa(
من ملفات استورد ملف، اكتب_نص_ملف
تاكد(اكتب_نص_ملف("input.txt"، "abc"))
المورد := ملف("input.txt"، "قراءة")
تاكد(المورد.فتح())
المورد.اقرا(-1)
)fa",
        "عدد بايتات القراءة لا يمكن أن يكون سالباً" },
    { "CompressionInvalidLimit", R"fa(
من ضغط استورد gzip، gunzip
gunzip(gzip("abc"، 6)، 0)
)fa",
        "حد فك الضغط يجب أن يكون موجباً" },
    { "IteratorZeroStep", R"fa(
من مكررات استورد مدى_كسول
مدى_كسول(0، 10، 0)
)fa",
        "خطوة المدى لا يمكن أن تكون صفراً" },
    { "EmptyOptionUnwrap", R"fa(
من نتيجة استورد لاشيء
لاشيء().افتح()
)fa",
        "محاولة فتح خيار فارغ" },
    { "FailedResultUnwrap", R"fa(
من نتيجة استورد فشل
فشل("expected public result failure").افتح()
)fa",
        "expected public result failure" },
    { "UnsupportedHash", R"fa(
من ترميزات استورد هاش
هاش("unsupported")
)fa",
        "خوارزمية هاش غير مدعومة" },
    { "TaskInvalidWorkerCount", R"fa(
من لا_متزامن استورد منفذ_مهام
منفذ_مهام(0)
)fa",
        "عدد العمال يجب أن يكون موجباً" },
    { "TaskSubmissionAfterClose", R"fa(
من لا_متزامن استورد منفذ_مهام
من دوالية استورد هوية
المنفذ := منفذ_مهام(1)
تاكد(المنفذ.اغلق(صحيح))
المنفذ.قدم(هوية، [1])
)fa",
        "executor is closed" },
};

INSTANTIATE_TEST_SUITE_P(PublicApi, StdlibE2E, ::testing::ValuesIn(cases),
    [](auto const& info) { return info.param.name; });

} // namespace
