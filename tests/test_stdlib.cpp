#include "../fairuz/fvm.hpp"
#include "test_common.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <string>

using namespace fairuz;
using namespace fairuz::runtime;

namespace {

class Fa_StdlibPerfTest : public ::testing::Test {
protected:
    void SetUp() override { REQUIRE_PERF(); }

    void TearDown() override { }
};

Fa_Value make_list(Fa_VM& vm, std::initializer_list<Fa_Value> values)
{
    Fa_Value list = vm.Fa_list(0, nullptr);
    for (Fa_Value m_value : values)
        list.as_list()->elements.push(m_value);
    return list;
}

std::string as_std_string(Fa_Value m_value)
{
    EXPECT_TRUE(m_value.is_string());
    if (!m_value.is_string())
        return { };
    Fa_StringRef const& text = m_value.as_string()->str;
    return std::string(text.data(), text.len());
}

double elapsed_us(std::chrono::high_resolution_clock::time_point start)
{
    using namespace std::chrono;
    return static_cast<double>(
               duration_cast<nanoseconds>(high_resolution_clock::now() - start).count())
        / 1000.0;
}

} // namespace

TEST(StdlibRegression, SplitPreservesEmptyFieldsAtBothEnds)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str(",alpha,,omega,"), str(",") };
    Fa_Value result = vm.Fa_split(2, m_args);

    ASSERT_TRUE(result.is_list());
    ASSERT_EQ(result.as_list()->elements.size(), 5u);
    EXPECT_EQ(as_std_string(result.as_list()->elements[0]), "");
    EXPECT_EQ(as_std_string(result.as_list()->elements[1]), "alpha");
    EXPECT_EQ(as_std_string(result.as_list()->elements[2]), "");
    EXPECT_EQ(as_std_string(result.as_list()->elements[3]), "omega");
    EXPECT_EQ(as_std_string(result.as_list()->elements[4]), "");
}

TEST(StdlibRegression, JoinStringifiesMixedScalarValues)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_Value::from_int(7),
                                      Fa_Value::from_bool(true),
                                      str("ok"),
                                      Fa_Value::nil(),
                                  });
    Fa_Value m_args[] = { list, str("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "7|صحيح|ok|nil");
}

TEST(StdlibRegression, JoinEmptyListReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, str("|") };
    Fa_Value result = vm.Fa_join(2, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibRegression, AppendAddsMultipleValuesInOrder)
{
    Fa_VM vm;
    Fa_Value list = vm.Fa_list(0, nullptr);
    Fa_Value m_args[] = { list, Fa_Value::from_int(1), Fa_Value::from_int(2), Fa_Value::from_int(3) };
    Fa_Value result = vm.Fa_append(4, m_args);

    EXPECT_TRUE(result.is_nil());
    ASSERT_EQ(list.as_list()->elements.size(), 3u);
    EXPECT_EQ(list.as_list()->elements[0].as_int(), 1);
    EXPECT_EQ(list.as_list()->elements[1].as_int(), 2);
    EXPECT_EQ(list.as_list()->elements[2].as_int(), 3);
}

TEST(StdlibRegression, PopRemovesLastElementFromList)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_Value::from_int(10),
                                      Fa_Value::from_int(20),
                                      Fa_Value::from_int(30),
                                  });
    Fa_Value result = vm.Fa_pop(1, &list);

    EXPECT_TRUE(result.is_list());
    ASSERT_EQ(list.as_list()->elements.size(), 2u);
    EXPECT_EQ(list.as_list()->elements[0].as_int(), 10);
    EXPECT_EQ(list.as_list()->elements[1].as_int(), 20);
}

TEST(StdlibRegression, SliceReturnsCopyNotAlias)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_Value::from_int(1),
                                        Fa_Value::from_int(2),
                                        Fa_Value::from_int(3),
                                        Fa_Value::from_int(4),
                                    });
    Fa_Value args[] = { source, Fa_Value::from_int(1), Fa_Value::from_int(2) };
    Fa_Value result = vm.Fa_slice(3, args);

    ASSERT_TRUE(result.is_list());
    ASSERT_EQ(result.as_list()->elements.size(), 2u);
    EXPECT_EQ(result.as_list()->elements[0].as_int(), 2);
    EXPECT_EQ(result.as_list()->elements[1].as_int(), 3);

    result.as_list()->elements[0] = Fa_Value::from_int(99);
    EXPECT_EQ(source.as_list()->elements[1].as_int(), 2);
}

TEST(StdlibRegression, SliceTwoArgsReturnsTail)
{
    Fa_VM vm;
    Fa_Value source = make_list(vm, {
                                        Fa_Value::from_int(4),
                                        Fa_Value::from_int(5),
                                        Fa_Value::from_int(6),
                                        Fa_Value::from_int(7),
                                    });
    Fa_Value args[] = { source, Fa_Value::from_int(2) };
    Fa_Value result = vm.Fa_slice(2, args);

    ASSERT_TRUE(result.is_list());
    ASSERT_EQ(result.as_list()->elements.size(), 2u);
    EXPECT_EQ(result.as_list()->elements[0].as_int(), 6);
    EXPECT_EQ(result.as_list()->elements[1].as_int(), 7);
}

TEST(StdlibRegression, SubstrClampsEndPastStringLength)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), Fa_Value::from_int(2), Fa_Value::from_int(99) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "iruz");
}

TEST(StdlibRegression, SubstrZeroWidthRangeReturnsEmptyString)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), Fa_Value::from_int(3), Fa_Value::from_int(3) };
    Fa_Value result = vm.Fa_substr(3, m_args);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "");
}

TEST(StdlibUnicode, SubstrUsesCodepointIndices)
{
    Fa_VM vm;
    Fa_Value args[] = { vm.m_gc.make_string("aمرحباz"), Fa_Value::from_int(1), Fa_Value::from_int(6) };
    EXPECT_EQ(as_std_string(vm.Fa_substr(3, args)), "مرحبا");
}

TEST(StdlibUnicode, CharacterFromCodepointSupportsUnicodeScalars)
{
    Fa_VM vm;
    Fa_Value arabic = Fa_Value::from_int(0x0645);
    EXPECT_EQ(as_std_string(vm.Fa_char_from_codepoint(1, &arabic)), "م");
}

TEST(StdlibUnicode, LenFallsBackToBytesForBinaryStrings)
{
    Fa_VM vm;
    Fa_StringRef bytes(3, '\0');
    bytes[0] = static_cast<char>(0xff);
    bytes[1] = '\0';
    bytes[2] = 'a';
    Fa_Value value = vm.m_gc.make_string(bytes);
    EXPECT_EQ(vm.Fa_len(1, &value).as_int(), 3);
}

TEST(StdlibJsonPrimitives, ParsesNumbersAndRejectsNonJsonForms)
{
    Fa_VM vm;
    Fa_Value integer = vm.m_gc.make_string("-42");
    Fa_Value real = vm.m_gc.make_string("-2.5e1");
    Fa_Value invalid = vm.m_gc.make_string("+1");
    EXPECT_EQ(vm.Fa_number_from_text(1, &integer).as_int(), -42);
    EXPECT_DOUBLE_EQ(vm.Fa_number_from_text(1, &real).as_double(), -25.0);
    EXPECT_TRUE(vm.Fa_number_from_text(1, &invalid).is_nil());
}

TEST(StdlibJsonPrimitives, EscapesAndReadsUnicodeJsonStrings)
{
    Fa_VM vm;
    Fa_Value raw = vm.m_gc.make_string("quote: \" slash: \\ newline:\n مرحبا");
    Fa_Value escaped = vm.Fa_json_escape(1, &raw);
    EXPECT_EQ(as_std_string(escaped), "quote: \\\" slash: \\\\ newline:\\n مرحبا");

    Fa_Value json = vm.m_gc.make_string("\"A\\n\\u0645\\uD83D\\uDE00\" tail");
    Fa_Value position = Fa_Value::from_int(0);
    Fa_Value args[] = { json, position };
    Fa_Value parsed = vm.Fa_json_read_string(2, args);
    ASSERT_TRUE(parsed.is_list());
    ASSERT_EQ(parsed.as_list()->elements.size(), 2u);
    EXPECT_EQ(as_std_string(parsed.as_list()->elements[0]), "A\nم😀");
    EXPECT_EQ(parsed.as_list()->elements[1].as_int(), 23);
}

TEST(StdlibJsonPrimitives, ReportsFiniteAndNanNumbers)
{
    Fa_VM vm;
    Fa_Value finite = Fa_Value::from_real(1.5);
    Fa_Value infinite = Fa_Value::from_real(std::numeric_limits<double>::infinity());
    Fa_Value nan = Fa_Value::from_real(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(vm.Fa_number_finite(1, &finite).as_bool());
    EXPECT_FALSE(vm.Fa_number_finite(1, &infinite).as_bool());
    EXPECT_TRUE(vm.Fa_number_is_nan(1, &nan).as_bool());
}

TEST(StdlibCallable, DynamicCallInvokesCallableWithListArguments)
{
    Fa_VM vm;
    Fa_Value const* length = vm.m_builtin_environment.find("طول");
    ASSERT_NE(length, nullptr);
    Fa_Value values = make_list(vm, { Fa_Value::from_int(1), Fa_Value::from_int(2) });
    Fa_Value arguments = make_list(vm, { values });
    Fa_Value call[] = { *length, arguments };
    Fa_Value result = vm.Fa_dynamic_call(2, call);
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(StdlibTasks, ExecutorAndCompletedTaskLifecycle)
{
    Fa_VM vm;
    Fa_Value workers = Fa_Value::from_int(2);
    Fa_Value executor = vm.Fa_executor_new(1, &workers);
    ASSERT_TRUE(executor.is_dict());

    Fa_Value const* length = vm.m_builtin_environment.find("طول");
    ASSERT_NE(length, nullptr);
    Fa_Value values = make_list(vm, { Fa_Value::from_int(1), Fa_Value::from_int(2), Fa_Value::from_int(3) });
    Fa_Value arguments = make_list(vm, { values });
    Fa_Value start[] = { executor, *length, arguments };
    Fa_Value task = vm.Fa_task_start(3, start);
    EXPECT_TRUE(vm.Fa_task_done(1, &task).as_bool());

    Fa_Value timeout = Fa_Value::from_int(1000);
    Fa_Value result_args[] = { task, timeout };
    EXPECT_EQ(vm.Fa_task_result(2, result_args).as_int(), 3);
    EXPECT_FALSE(vm.Fa_task_cancel(1, &task).as_bool());

    Fa_Value tasks = make_list(vm, { task });
    Fa_Value wait_args[] = { tasks, timeout };
    Fa_Value results = vm.Fa_task_wait_all(2, wait_args);
    ASSERT_TRUE(results.is_list());
    ASSERT_EQ(results.as_list()->elements.size(), 1u);
    EXPECT_EQ(results.as_list()->elements[0].as_int(), 3);

    Fa_Value wait = Fa_Value::from_bool(true);
    Fa_Value close_args[] = { executor, wait };
    EXPECT_TRUE(vm.Fa_executor_close(2, close_args).as_bool());
}

TEST(StdlibTasks, PendingTaskCanBeCancelled)
{
    Fa_VM vm;
    Fa_Value task = vm.Fa_dict(0, nullptr);
    task.as_dict()->set(vm.m_gc.make_string("done"), Fa_Value::from_bool(false));
    EXPECT_TRUE(vm.Fa_task_cancel(1, &task).as_bool());
    EXPECT_TRUE(vm.Fa_task_done(1, &task).as_bool());
}

TEST(StdlibFiles, WriteFlushReadLineReadBytesAndReadAll)
{
    Fa_VM vm;
    auto path = std::filesystem::temp_directory_path() / "fairuz-native-file-test.txt";
    Fa_Value path_value = vm.m_gc.make_string(path.string().c_str());
    Fa_Value write_mode = vm.m_gc.make_string("كتابة");
    Fa_Value open_write[] = { path_value, write_mode };
    Fa_Value handle = vm.Fa_file_open(2, open_write);
    ASSERT_TRUE(handle.is_file_handle());
    Fa_Value content = vm.m_gc.make_string("first\nsecond");
    Fa_Value write[] = { handle, content };
    EXPECT_TRUE(vm.Fa_file_write(2, write).as_bool());
    EXPECT_TRUE(vm.Fa_file_flush(1, &handle).as_bool());
    EXPECT_TRUE(vm.Fa_close(1, &handle).as_bool());

    Fa_Value read_mode = vm.m_gc.make_string("قراءة");
    Fa_Value open_read[] = { path_value, read_mode };
    handle = vm.Fa_file_open(2, open_read);
    EXPECT_EQ(as_std_string(vm.Fa_file_read_line(1, &handle)), "first");
    Fa_Value count = Fa_Value::from_int(3);
    Fa_Value read[] = { handle, count };
    EXPECT_EQ(as_std_string(vm.Fa_file_read(2, read)), "sec");
    EXPECT_EQ(as_std_string(vm.Fa_file_read_all(1, &handle)), "ond");
    EXPECT_TRUE(vm.Fa_close(1, &handle).as_bool());
    std::filesystem::remove(path);
}

TEST(StdlibFilesystem, TemporaryResourcesGlobAndDeletion)
{
    Fa_VM vm;
    Fa_Value prefix = vm.m_gc.make_string("fairuz-native-");
    Fa_Value suffix = vm.m_gc.make_string(".tmp");
    Fa_Value none = Fa_Value::nil();
    Fa_Value file_args[] = { prefix, suffix, none };
    Fa_Value file_info = vm.Fa_temp_file(3, file_args);
    ASSERT_TRUE(file_info.is_dict());
    Fa_Value path_key = vm.m_gc.make_string("path");
    Fa_Value file_path = *file_info.as_dict()->data.find_ptr(path_key);
    EXPECT_TRUE(std::filesystem::exists(as_std_string(file_path)));

    Fa_Value pattern = vm.m_gc.make_string(
        (std::filesystem::temp_directory_path() / "fairuz-native-*.tmp").string().c_str());
    Fa_Value recursive = Fa_Value::from_bool(false);
    Fa_Value glob_args[] = { pattern, recursive };
    Fa_Value matches = vm.Fa_path_glob(2, glob_args);
    ASSERT_TRUE(matches.is_list());
    EXPECT_GE(matches.as_list()->elements.size(), 1u);
    EXPECT_TRUE(vm.Fa_path_delete(1, &file_path).as_bool());

    Fa_Value dir_args[] = { prefix, none };
    Fa_Value directory = vm.Fa_temp_directory(2, dir_args);
    ASSERT_TRUE(directory.is_string());
    EXPECT_TRUE(std::filesystem::is_directory(as_std_string(directory)));
    EXPECT_TRUE(vm.Fa_remove_tree(1, &directory).as_bool());
    EXPECT_FALSE(std::filesystem::exists(as_std_string(directory)));
}

TEST(StdlibDatetime, EpochFieldsConstructionParsingAndFormatting)
{
    Fa_VM vm;
    Fa_Value utc = vm.m_gc.make_string("UTC");
    Fa_Value epoch = Fa_Value::from_int(0);
    Fa_Value field_args[] = { epoch, utc };
    Fa_Value fields = vm.Fa_datetime_to_fields(2, field_args);
    ASSERT_TRUE(fields.is_list());
    EXPECT_EQ(fields.as_list()->elements[0].as_int(), 1970);
    EXPECT_EQ(fields.as_list()->elements[1].as_int(), 1);
    EXPECT_EQ(fields.as_list()->elements[2].as_int(), 1);

    Fa_Value construct[] = {
        Fa_Value::from_int(2020), Fa_Value::from_int(5), Fa_Value::from_int(6),
        Fa_Value::from_int(7), Fa_Value::from_int(8), Fa_Value::from_int(9), utc,
    };
    Fa_Value timestamp = vm.Fa_datetime_from_fields(7, construct);
    ASSERT_TRUE(timestamp.is_int());
    EXPECT_EQ(timestamp.as_int(), 1588748889);

    Fa_Value text = vm.m_gc.make_string("2020-05-06T07:08:09Z");
    Fa_Value iso = vm.m_gc.make_string("ISO8601");
    Fa_Value parse[] = { text, iso, utc };
    EXPECT_EQ(vm.Fa_datetime_parse(3, parse).as_int(), timestamp.as_int());
    Fa_Value format[] = { timestamp, utc, iso };
    EXPECT_EQ(as_std_string(vm.Fa_datetime_format(3, format)), "2020-05-06T07:08:09Z");
}

TEST(StdlibDatetime, CurrentTimeReturnsUnixSeconds)
{
    Fa_VM vm;
    auto before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    Fa_Value current = vm.Fa_datetime_now(0, nullptr);
    auto after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ASSERT_TRUE(current.is_int());
    EXPECT_GE(current.as_int(), static_cast<i64>(before));
    EXPECT_LE(current.as_int(), static_cast<i64>(after));
}

TEST(StdlibCodecs, Base64SupportsStandardUrlSafeAndBinaryData)
{
    Fa_VM vm;
    Fa_Value standard_args[] = { vm.m_gc.make_string("hello"), Fa_Value::from_bool(false) };
    Fa_Value encoded = vm.Fa_base64_encode(2, standard_args);
    EXPECT_EQ(as_std_string(encoded), "aGVsbG8=");
    Fa_Value decode_args[] = { encoded, Fa_Value::from_bool(false) };
    EXPECT_EQ(as_std_string(vm.Fa_base64_decode(2, decode_args)), "hello");

    Fa_StringRef bytes(3, '\0');
    bytes[0] = static_cast<char>(0xfb);
    bytes[1] = static_cast<char>(0xff);
    bytes[2] = '\0';
    Fa_Value url_args[] = { vm.m_gc.make_string(bytes), Fa_Value::from_bool(true) };
    Fa_Value url_encoded = vm.Fa_base64_encode(2, url_args);
    EXPECT_EQ(as_std_string(url_encoded), "-_8A");
    Fa_Value url_decode_args[] = { url_encoded, Fa_Value::from_bool(true) };
    EXPECT_EQ(as_std_string(vm.Fa_base64_decode(2, url_decode_args)), as_std_string(url_args[0]));

    Fa_Value invalid_args[] = { vm.m_gc.make_string("not base64!"), Fa_Value::from_bool(false) };
    EXPECT_TRUE(vm.Fa_base64_decode(2, invalid_args).is_nil());
}

TEST(StdlibCodecs, HexRoundTripsEmbeddedNullAndRejectsMalformedInput)
{
    Fa_VM vm;
    Fa_StringRef bytes(3, '\0');
    bytes[0] = 'A';
    bytes[1] = '\0';
    bytes[2] = static_cast<char>(0xff);
    Fa_Value input = vm.m_gc.make_string(bytes);
    Fa_Value encoded = vm.Fa_hex_encode(1, &input);
    EXPECT_EQ(as_std_string(encoded), "4100ff");
    EXPECT_EQ(as_std_string(vm.Fa_hex_decode(1, &encoded)), as_std_string(input));

    Fa_Value malformed = vm.m_gc.make_string("xyz");
    EXPECT_TRUE(vm.Fa_hex_decode(1, &malformed).is_nil());
}

TEST(StdlibCodecs, Sha256SupportsIncrementalAndRawDigests)
{
    Fa_VM vm;
    Fa_Value algorithm = vm.m_gc.make_string("sha256");
    Fa_Value handle = vm.Fa_hash_new(1, &algorithm);
    ASSERT_TRUE(handle.is_dict());

    Fa_Value first_args[] = { handle, vm.m_gc.make_string("hello ") };
    Fa_Value second_args[] = { handle, vm.m_gc.make_string("world") };
    EXPECT_TRUE(vm.Fa_hash_update(2, first_args).as_bool());
    EXPECT_TRUE(vm.Fa_hash_update(2, second_args).as_bool());

    Fa_Value hex_args[] = { handle, Fa_Value::from_bool(true) };
    EXPECT_EQ(as_std_string(vm.Fa_hash_digest(2, hex_args)),
        "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    Fa_Value raw_args[] = { handle, Fa_Value::from_bool(false) };
    EXPECT_EQ(as_std_string(vm.Fa_hash_digest(2, raw_args)).size(), 32u);

    Fa_Value unsupported = vm.m_gc.make_string("md5");
    EXPECT_TRUE(vm.Fa_hash_new(1, &unsupported).is_nil());
}

TEST(StdlibCodecs, HmacSha256MatchesPublishedVector)
{
    Fa_VM vm;
    Fa_Value args[] = {
        vm.m_gc.make_string("sha256"),
        vm.m_gc.make_string("key"),
        vm.m_gc.make_string("The quick brown fox jumps over the lazy dog"),
    };
    EXPECT_EQ(as_std_string(vm.Fa_hmac(3, args)),
        "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

TEST(StdlibCompression, GzipRoundTripsAllSupportedBoundaryLevelsAndBinaryData)
{
    Fa_VM vm;
    Fa_StringRef payload(1027, 'a');
    payload[0] = '\0';
    payload[1026] = static_cast<char>(0xff);
    Fa_Value algorithm = vm.m_gc.make_string("gzip");
    Fa_Value input = vm.m_gc.make_string(payload);

    for (i64 level : { 0, 1, 6, 9 }) {
        Fa_Value compress_args[] = { algorithm, input, Fa_Value::from_int(level) };
        Fa_Value compressed = vm.Fa_compress(3, compress_args);
        ASSERT_TRUE(compressed.is_string());
        EXPECT_GT(compressed.as_string()->str.len(), 0u);

        Fa_Value decompress_args[] = { algorithm, compressed, Fa_Value::from_int(2048) };
        Fa_Value restored = vm.Fa_decompress(3, decompress_args);
        EXPECT_EQ(as_std_string(restored), as_std_string(input));
    }
}

TEST(StdlibCompression, GzipRejectsInvalidDataLevelAlgorithmAndOutputLimit)
{
    Fa_VM vm;
    Fa_Value gzip = vm.m_gc.make_string("gzip");
    Fa_Value text = vm.m_gc.make_string("compress me");
    Fa_Value bad_level_args[] = { gzip, text, Fa_Value::from_int(10) };
    EXPECT_TRUE(vm.Fa_compress(3, bad_level_args).is_nil());

    Fa_Value other = vm.m_gc.make_string("other");
    Fa_Value other_args[] = { other, text, Fa_Value::from_int(6) };
    EXPECT_TRUE(vm.Fa_compress(3, other_args).is_nil());

    Fa_Value good_args[] = { gzip, text, Fa_Value::from_int(6) };
    Fa_Value compressed = vm.Fa_compress(3, good_args);
    Fa_Value limited_args[] = { gzip, compressed, Fa_Value::from_int(5) };
    EXPECT_TRUE(vm.Fa_decompress(3, limited_args).is_nil());
    Fa_Value corrupt_args[] = { gzip, text, Fa_Value::from_int(100) };
    EXPECT_TRUE(vm.Fa_decompress(3, corrupt_args).is_nil());
}

TEST(StdlibRegression, ContainsEmptyNeedleIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), str("") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.as_bool());
}

TEST(StdlibRegression, ContainsExactMatchIsTrue)
{
    Fa_VM vm;
    Fa_Value m_args[] = { str("fairuz"), str("fairuz") };
    Fa_Value result = vm.Fa_contains(2, m_args);

    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.as_bool());
}

TEST(StdlibRegression, StrStringifiesListsLikePrint)
{
    Fa_VM vm;
    Fa_Value list = make_list(vm, {
                                      Fa_Value::from_int(1),
                                      Fa_Value::from_bool(false),
                                      str("z"),
                                  });
    Fa_Value result = vm.Fa_str(1, &list);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), R"([1, خطا, "z"])");
}

TEST(StdlibRegression, StrStringifiesDictsLikePrint)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    dict.as_dict()->data[str("k")] = Fa_Value::from_int(3);
    dict.as_dict()->data[str("name")] = str("fairuz");

    Fa_Value result = vm.Fa_str(1, &dict);

    ASSERT_TRUE(result.is_string());
    EXPECT_TRUE(as_std_string(result).find("\"k\": 3") != std::string::npos);
    EXPECT_TRUE(as_std_string(result).find("\"name\": \"fairuz\"") != std::string::npos);
}

TEST(StdlibRegression, LenSupportsDicts)
{
    Fa_VM vm;
    Fa_Value dict = vm.Fa_dict(0, nullptr);
    dict.as_dict()->data[str("a")] = Fa_Value::from_int(1);
    dict.as_dict()->data[str("b")] = Fa_Value::from_int(2);

    Fa_Value result = vm.Fa_len(1, &dict);

    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(StdlibRegression, DictConstructorPopulatesPairs)
{
    Fa_VM vm;
    Fa_Value m_args[] = {
        str("a"),
        Fa_Value::from_int(1),
        str("b"),
        Fa_Value::from_bool(true),
    };

    Fa_Value dict = vm.Fa_dict(4, m_args);

    ASSERT_TRUE(dict.is_dict());
    Fa_Value* a = dict.as_dict()->data.find_ptr(m_args[0]);
    Fa_Value* b = dict.as_dict()->data.find_ptr(m_args[2]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->as_int(), 1);
    EXPECT_TRUE(b->is_bool());
    EXPECT_TRUE(b->as_bool());
}

TEST(StdlibDictionary, KeysPreserveInsertionOrderAndReassignmentPosition)
{
    Fa_VM vm;
    Fa_Value args[] = {
        str("first"), Fa_Value::from_int(1),
        str("second"), Fa_Value::from_int(2),
    };
    Fa_Value dict = vm.Fa_dict(4, args);
    Fa_Value replacement[] = { dict, args[0], Fa_Value::from_int(9) };
    vm.Fa_dict_put(&replacement[0], replacement[1], replacement[2]);

    Fa_Value keys = vm.Fa_dict_keys(1, &dict);
    ASSERT_TRUE(keys.is_list());
    ASSERT_EQ(keys.as_list()->elements.size(), 2u);
    EXPECT_EQ(as_std_string(keys.as_list()->elements[0]), "first");
    EXPECT_EQ(as_std_string(keys.as_list()->elements[1]), "second");
    EXPECT_EQ(dict.as_dict()->data.find_ptr(args[0])->as_int(), 9);
}

TEST(StdlibDictionary, ContainsAndDeleteReturnPythonStyleResults)
{
    Fa_VM vm;
    Fa_Value args[] = { str("key"), Fa_Value::from_int(42) };
    Fa_Value dict = vm.Fa_dict(2, args);
    Fa_Value query[] = { dict, args[0] };

    EXPECT_TRUE(vm.Fa_dict_contains(2, query).as_bool());
    Fa_Value removed = vm.Fa_dict_delete(2, query);
    ASSERT_TRUE(removed.is_int());
    EXPECT_EQ(removed.as_int(), 42);
    EXPECT_FALSE(vm.Fa_dict_contains(2, query).as_bool());
    EXPECT_TRUE(vm.Fa_dict_delete(2, query).is_nil());
    EXPECT_TRUE(dict.as_dict()->insertion_order.empty());
}

TEST(StdlibDictionary, DynamicallyAllocatedEqualStringsShareAKey)
{
    Fa_VM vm;
    Fa_Value inserted_key = vm.m_gc.make_string("dynamic-key");
    Fa_Value lookup_key = vm.m_gc.make_string("dynamic-key");
    ASSERT_NE(inserted_key.as_obj(), lookup_key.as_obj());

    Fa_Value args[] = { inserted_key, Fa_Value::from_int(7) };
    Fa_Value dict = vm.Fa_dict(2, args);
    Fa_Value query[] = { dict, lookup_key };
    EXPECT_TRUE(vm.Fa_dict_contains(2, query).as_bool());
    ASSERT_NE(dict.as_dict()->data.find_ptr(lookup_key), nullptr);
    EXPECT_EQ(dict.as_dict()->data.find_ptr(lookup_key)->as_int(), 7);
}

TEST(StdlibRegression, StrScalarConversionsMatchSurfaceSyntax)
{
    Fa_VM vm;

    Fa_Value int_value = Fa_Value::from_int(42);
    Fa_Value bool_value = Fa_Value::from_bool(true);
    Fa_Value nil_value = Fa_Value::nil();

    EXPECT_EQ(as_std_string(vm.Fa_str(1, &int_value)), "42");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &bool_value)), "صحيح");
    EXPECT_EQ(as_std_string(vm.Fa_str(1, &nil_value)), "nil");
}

TEST(StdlibRegression, TrimRemovesMixedLeadingAndTrailingWhitespace)
{
    Fa_VM vm;
    Fa_Value arg = str("\n\t  fairuz  \r\n");
    Fa_Value result = vm.Fa_trim(1, &arg);

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "fairuz");
}

TEST_F(Fa_StdlibPerfTest, SplitJoinRoundTripLargeCsv)
{
    Fa_VM vm;
    std::string csv;
    csv.reserve(32 * 2000);
    for (int i = 0; i < 2000; i++) {
        if (i)
            csv += ',';
        csv += "field";
        csv += std::to_string(i);
    }

    Fa_Value split_args[] = { str(csv.c_str()), str(",") };
    auto start = std::chrono::high_resolution_clock::now();
    Fa_Value parts = vm.Fa_split(2, split_args);
    double split_us = elapsed_us(start);

    ASSERT_TRUE(parts.is_list());
    ASSERT_EQ(parts.as_list()->elements.size(), 2000u);

    Fa_Value join_args[] = { parts, str(",") };
    start = std::chrono::high_resolution_clock::now();
    Fa_Value roundtrip = vm.Fa_join(2, join_args);
    double join_us = elapsed_us(start);

    ASSERT_TRUE(roundtrip.is_string());
    EXPECT_EQ(as_std_string(roundtrip), csv);
    std::printf("  stdlib split 2k fields: %.1f us, join: %.1f us\n", split_us, join_us);
}

TEST_F(Fa_StdlibPerfTest, LenOnLargeString100kCalls)
{
    Fa_VM vm;
    std::string payload(8192, 'x');
    Fa_Value arg = str(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    i64 last = -1;
    for (int i = 0; i < 100000; i++) {
        Fa_Value value = vm.Fa_len(1, &arg);
        ASSERT_TRUE(value.is_int());
        last = value.as_int();
    }
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, 8192);
    std::printf("  stdlib len 100k calls (8 KiB string): %.1f us\n", total_us);
}

TEST_F(Fa_StdlibPerfTest, TrimLargePaddedString50kCalls)
{
    Fa_VM vm;
    std::string payload(1024, ' ');
    payload += "fairuz";
    payload.append(1024, '\t');
    Fa_Value arg = str(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    std::string last;
    for (int i = 0; i < 50000; i++)
        last = as_std_string(vm.Fa_trim(1, &arg));
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, "fairuz");
    std::printf("  stdlib trim 50k calls (2 KiB padding): %.1f us\n", total_us);
}
