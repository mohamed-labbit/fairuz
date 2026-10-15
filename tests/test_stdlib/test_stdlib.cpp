//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies standard-library module behavior.
//===----------------------------------------------------------------------===//

#include "Interpreter/vm.hpp"
#include "Platform.hpp"
#include "test_common.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>

using namespace fairuz;
using namespace fairuz::runtime;

namespace {

class StdlibPerfTest : public ::testing::Test {
protected:
    void SetUp() override { REQUIRE_PERF(); }

    void TearDown() override { }
};

Value make_list(VM& vm, std::initializer_list<Value> values)
{
    Value list = run_success(vm.list(0, nullptr));
    for (Value m_value : values)
        list.as_list()->elements.push(m_value);
    return list;
}

std::string as_std_string(Value m_value)
{
    EXPECT_TRUE(m_value.is_string());
    if (!m_value.is_string())
        return { };
    StringRef const& text = m_value.as_string()->str;
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
    VM vm;
    Value args[] = { str(",alpha,,omega,"), str(",") };
    auto result = run_success(vm.split(2, args));
    Value v = result;

    ASSERT_TRUE(v.is_list());
    ASSERT_EQ(v.as_list()->elements.size(), 5u);
    EXPECT_EQ(as_std_string(v.as_list()->elements[0]), "");
    EXPECT_EQ(as_std_string(v.as_list()->elements[1]), "alpha");
    EXPECT_EQ(as_std_string(v.as_list()->elements[2]), "");
    EXPECT_EQ(as_std_string(v.as_list()->elements[3]), "omega");
    EXPECT_EQ(as_std_string(v.as_list()->elements[4]), "");
}

TEST(StdlibRegression, JoinStringifiesMixedScalarValues)
{
    VM vm;
    Value list = make_list(vm, {
                                   Value::from_int(7),
                                   Value::from_bool(true),
                                   str("ok"),
                                   Value::nil(),
                               });
    Value args[] = { list, str("|") };
    auto result = run_success(vm.join(2, args));
    Value v = result;

    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(as_std_string(v), "7|صحيح|ok|nil");
}

TEST(StdlibRegression, JoinEmptyListReturnsEmptyString)
{
    VM vm;
    Value list = run_success(vm.list(0, nullptr));
    Value args[] = { list, str("|") };
    auto result = run_success(vm.join(2, args));
    Value v = result;

    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(as_std_string(v), "");
}

TEST(StdlibRegression, AppendAddsMultipleValuesInOrder)
{
    VM vm;
    Value list = run_success(vm.list(0, nullptr));
    Value args[] = { list, Value::from_int(1), Value::from_int(2), Value::from_int(3) };
    auto result = run_success(vm.append(4, args));
    Value v = result;

    EXPECT_TRUE(v.is_nil());
    ASSERT_EQ(list.as_list()->elements.size(), 3u);
    EXPECT_EQ(list.as_list()->elements[0].as_int(), 1);
    EXPECT_EQ(list.as_list()->elements[1].as_int(), 2);
    EXPECT_EQ(list.as_list()->elements[2].as_int(), 3);
}

TEST(StdlibRegression, PopRemovesLastElementFromList)
{
    VM vm;
    Value list = make_list(vm, {
                                   Value::from_int(10),
                                   Value::from_int(20),
                                   Value::from_int(30),
                               });
    auto result = run_success(vm.pop(1, &list));
    Value v = result;

    EXPECT_TRUE(v.is_list());
    ASSERT_EQ(list.as_list()->elements.size(), 2u);
    EXPECT_EQ(list.as_list()->elements[0].as_int(), 10);
    EXPECT_EQ(list.as_list()->elements[1].as_int(), 20);
}

TEST(StdlibRegression, SliceReturnsCopyNotAlias)
{
    VM vm;
    Value source = make_list(vm, {
                                     Value::from_int(1),
                                     Value::from_int(2),
                                     Value::from_int(3),
                                     Value::from_int(4),
                                 });
    Value args[] = { source, Value::from_int(1), Value::from_int(2) };
    auto result = run_success(vm.slice(3, args));
    Value v = result;

    ASSERT_TRUE(v.is_list());
    ASSERT_EQ(v.as_list()->elements.size(), 2u);
    EXPECT_EQ(v.as_list()->elements[0].as_int(), 2);
    EXPECT_EQ(v.as_list()->elements[1].as_int(), 3);

    v.as_list()->elements[0] = Value::from_int(99);
    EXPECT_EQ(source.as_list()->elements[1].as_int(), 2);
}

TEST(StdlibRegression, SliceTwoArgsReturnsTail)
{
    VM vm;
    Value source = make_list(vm, {
                                     Value::from_int(4),
                                     Value::from_int(5),
                                     Value::from_int(6),
                                     Value::from_int(7),
                                 });
    Value args[] = { source, Value::from_int(2) };
    auto result = run_success(vm.slice(2, args));
    Value v = result;

    ASSERT_TRUE(v.is_list());
    ASSERT_EQ(v.as_list()->elements.size(), 2u);
    EXPECT_EQ(v.as_list()->elements[0].as_int(), 6);
    EXPECT_EQ(v.as_list()->elements[1].as_int(), 7);
}

TEST(StdlibRegression, SubstrClampsEndPastStringLength)
{
    VM vm;
    Value args[] = { str("fairuz"), Value::from_int(2), Value::from_int(99) };
    auto result = run_success(vm.substr(3, args));
    Value v = result;

    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(as_std_string(v), "iruz");
}

TEST(StdlibRegression, SubstrZeroWidthRangeReturnsEmptyString)
{
    VM vm;
    Value args[] = { str("fairuz"), Value::from_int(3), Value::from_int(3) };
    auto result = run_success(vm.substr(3, args));
    Value v = result;

    ASSERT_TRUE(v.is_string());
    EXPECT_EQ(as_std_string(v), "");
}

/*
TEST(StdlibUnicode, SubstrUsesCodepointIndices)
{
    VM vm;
    Value args[] = { vm.gc().make_string("aمرحباz"), Value::from_int(1), Value::from_int(6) };
    EXPECT_EQ(as_std_string(run_success(vm.substr(3, args))), "مرحبا");
}
*/

TEST(StdlibUnicode, CharacterFromCodepointSupportsUnicodeScalars)
{
    VM vm;
    Value arabic = Value::from_int(0x0645);
    EXPECT_EQ(as_std_string(run_success(vm.char_from_codepoint(1, &arabic))), "م");
}

TEST(StdlibUnicode, LenFallsBackToBytesForBinaryStrings)
{
    VM vm;
    StringRef bytes(3, '\0');
    bytes[0] = static_cast<char>(0xff);
    bytes[1] = '\0';
    bytes[2] = 'a';
    Value value = vm.gc().make_string(bytes);
    EXPECT_EQ(run_success(vm.len(1, &value)).as_int(), 3);
}

TEST(StdlibJsonPrimitives, ParsesNumbersAndRejectsNonJsonForms)
{
    VM vm;
    Value integer = vm.gc().make_string("-42");
    Value real = vm.gc().make_string("-2.5e1");
    Value invalid = vm.gc().make_string("+1");
    EXPECT_EQ(run_success(vm.number_from_text(1, &integer)).as_int(), -42);
    EXPECT_DOUBLE_EQ(run_success(vm.number_from_text(1, &real)).as_double(), -25.0);
    EXPECT_EQ(run_failed(vm.number_from_text(1, &invalid)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(StdlibJsonPrimitives, EscapesAndReadsUnicodeJsonStrings)
{
    VM vm;
    Value raw = vm.gc().make_string("quote: \" slash: \\ newline:\n"
                                    " مرحبا");
    Value escaped = run_success(vm.json_escape(1, &raw));
    EXPECT_EQ(as_std_string(escaped), "quote: \\\" slash: \\\\ newline:\\n مرحبا");

    Value json = vm.gc().make_string("\"A\\n\\u0645\\uD83D\\uDE00\" tail");
    Value position = Value::from_int(0);
    Value args[] = { json, position };
    Value parsed = run_success(vm.json_read_string(2, args));
    ASSERT_TRUE(parsed.is_list());
    ASSERT_EQ(parsed.as_list()->elements.size(), 2u);
    EXPECT_EQ(as_std_string(parsed.as_list()->elements[0]), "A\n"
                                                            "م😀");
    EXPECT_EQ(parsed.as_list()->elements[1].as_int(), 23);
}

TEST(StdlibJsonPrimitives, ReportsFiniteAndNanNumbers)
{
    VM vm;
    Value finite = Value::from_real(1.5);
    Value infinite = Value::from_real(std::numeric_limits<double>::infinity());
    Value nan = Value::from_real(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(run_success(vm.number_finite(1, &finite)).as_bool());
    EXPECT_FALSE(run_success(vm.number_finite(1, &infinite)).as_bool());
    EXPECT_TRUE(run_success(vm.number_is_nan(1, &nan)).as_bool());
}

TEST(StdlibRegistry, LookupIsLazyAndCachedAcrossCollection)
{
    VM vm;
    auto initial_memory = vm.gc().current_memory();
    EXPECT_EQ(vm.builtin_env().find("missing_builtin"), nullptr);
    EXPECT_EQ(vm.gc().current_memory(), initial_memory);
    for (Value value : vm.builtins().values())
        EXPECT_TRUE(value.is_nil());

    Value const* length = vm.builtin_env().find("طول");
    ASSERT_NE(length, nullptr);
    ASSERT_TRUE(length->is_native());
    auto* native = length->as_native();
    EXPECT_EQ(native->arity, 1);
    EXPECT_EQ(native->fn, &VM::len);
    auto loaded_memory = vm.gc().current_memory();
    EXPECT_EQ(vm.builtin_env().find("طول"), length);
    EXPECT_EQ(vm.gc().current_memory(), loaded_memory);

    vm.gc().collect(&vm);
    EXPECT_EQ(vm.builtin_env().find("طول")->as_native(), native);
    EXPECT_EQ(native->name->str, StringRef("طول"));
    Value argument = make_list(vm, { Value::from_int(1) });
    EXPECT_EQ(run_success((vm.*native->fn)(1, &argument)).as_int(), 1);
}

TEST(StdlibRegistry, GlobalShadowingDoesNotReplaceBuiltin)
{
    VM vm;
    Value const* builtin = vm.find_global(nullptr, "طول");
    ASSERT_NE(builtin, nullptr);
    ASSERT_TRUE(builtin->is_native());
    vm.store_global(nullptr, "طول", Value::from_int(42));
    EXPECT_EQ(vm.find_global(nullptr, "طول")->as_int(), 42);
    EXPECT_EQ(vm.builtin_env().find("طول"), builtin);

    VM other;
    auto* other_builtin = other.find_global(nullptr, "طول");
    ASSERT_NE(other_builtin, nullptr);
    EXPECT_TRUE(other_builtin->is_native());
    EXPECT_NE(other_builtin->as_native(), builtin->as_native());
}

TEST(StdlibCallable, DynamicCallInvokesCallableWithListArguments)
{
    VM vm;
    Value const* length = vm.builtin_env().find("طول");
    ASSERT_NE(length, nullptr);
    Value values = make_list(vm, { Value::from_int(1), Value::from_int(2) });
    Value arguments = make_list(vm, { values });
    Value call[] = { *length, arguments };
    auto result = run_success(vm.dynamic_call(2, call));
    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(StdlibTasks, ExecutorAndCompletedTaskLifecycle)
{
    VM vm;
    Value workers = Value::from_int(2);
    Value executor = run_success(vm.executor_new(1, &workers));
    ASSERT_TRUE(executor.is_dict());

    Value const* length = vm.builtin_env().find("طول");
    ASSERT_NE(length, nullptr);
    Value values = make_list(vm, { Value::from_int(1), Value::from_int(2), Value::from_int(3) });
    Value arguments = make_list(vm, { values });
    Value start[] = { executor, *length, arguments };
    Value task = run_success(vm.task_start(3, start));
    EXPECT_TRUE(run_success(vm.task_done(1, &task)).as_bool());

    Value timeout = Value::from_int(1000);
    Value result_args[] = { task, timeout };
    EXPECT_EQ(run_success(vm.task_result(2, result_args)).as_int(), 3);
    EXPECT_FALSE(run_success(vm.task_cancel(1, &task)).as_bool());

    Value tasks = make_list(vm, { task });
    Value wait_args[] = { tasks, timeout };
    Value results = run_success(vm.task_wait_all(2, wait_args));
    ASSERT_TRUE(results.is_list());
    ASSERT_EQ(results.as_list()->elements.size(), 1u);
    EXPECT_EQ(results.as_list()->elements[0].as_int(), 3);

    Value wait = Value::from_bool(true);
    Value close_args[] = { executor, wait };
    EXPECT_TRUE(run_success(vm.executor_close(2, close_args)).as_bool());
}

TEST(StdlibTasks, PendingTaskCanBeCancelled)
{
    VM vm;
    Value task = run_success(vm.dict(0, nullptr));
    task.as_dict()->set(vm.gc().make_string("done"), Value::from_bool(false));
    EXPECT_TRUE(run_success(vm.task_cancel(1, &task)).as_bool());
    EXPECT_TRUE(run_success(vm.task_done(1, &task)).as_bool());
}

TEST(StdlibFiles, WriteFlushReadLineReadBytesAndReadAll)
{
    VM vm;
    auto path = std::filesystem::temp_directory_path() / "fairuz-native-file-test.txt";
    Value path_value = vm.gc().make_string(platform::utf8(path).c_str());
    Value write_mode = vm.gc().make_string("كتابة");
    Value open_write[] = { path_value, write_mode };
    Value handle = run_success(vm.file_open(2, open_write));
    ASSERT_TRUE(handle.is_file_handle());
    Value content = vm.gc().make_string("first\n"
                                        "second");
    Value write[] = { handle, content };
    EXPECT_TRUE(run_success(vm.file_write(2, write)).as_bool());
    EXPECT_TRUE(run_success(vm.file_flush(1, &handle)).as_bool());
    EXPECT_TRUE(run_success(vm.close(1, &handle)).as_bool());

    Value read_mode = vm.gc().make_string("قراءة");
    Value open_read[] = { path_value, read_mode };
    handle = run_success(vm.file_open(2, open_read));
    EXPECT_EQ(as_std_string(run_success(vm.file_read_line(1, &handle))), "first");
    Value count = Value::from_int(3);
    Value read[] = { handle, count };
    EXPECT_EQ(as_std_string(run_success(vm.file_read(2, read))), "sec");
    EXPECT_EQ(as_std_string(run_success(vm.file_read_all(1, &handle))), "ond");
    EXPECT_TRUE(run_success(vm.close(1, &handle)).as_bool());
    std::filesystem::remove(path);
}

TEST(StdlibFilesystem, TemporaryResourcesGlobAndDeletion)
{
    VM vm;
    Value prefix = vm.gc().make_string("fairuz-native-");
    Value suffix = vm.gc().make_string(".tmp");
    Value none = Value::nil();
    Value file_args[] = { prefix, suffix, none };
    Value file_info = run_success(vm.temp_file(3, file_args));
    ASSERT_TRUE(file_info.is_dict());
    Value path_key = vm.gc().make_string("path");
    Value file_path = *file_info.as_dict()->data.find_ptr(path_key);
    EXPECT_TRUE(std::filesystem::exists(platform::path(as_std_string(file_path))));

    Value pattern = vm.gc().make_string(
        platform::utf8(std::filesystem::temp_directory_path() / "fairuz-native-*.tmp").c_str());
    Value recursive = Value::from_bool(false);
    Value glob_args[] = { pattern, recursive };
    Value matches = run_success(vm.path_glob(2, glob_args));
    ASSERT_TRUE(matches.is_list());
    EXPECT_GE(matches.as_list()->elements.size(), 1u);
    EXPECT_TRUE(run_success(vm.path_delete(1, &file_path)).as_bool());

    Value dir_args[] = { prefix, none };
    Value directory = run_success(vm.temp_directory(2, dir_args));
    ASSERT_TRUE(directory.is_string());
    EXPECT_TRUE(std::filesystem::is_directory(platform::path(as_std_string(directory))));
    EXPECT_TRUE(run_success(vm.remove_tree(1, &directory)).as_bool());
    EXPECT_FALSE(std::filesystem::exists(as_std_string(directory)));
}

TEST(StdlibDatetime, EpochFieldsConstructionParsingAndFormatting)
{
    VM vm;
    Value utc = vm.gc().make_string("UTC");
    Value epoch = Value::from_int(0);
    Value field_args[] = { epoch, utc };
    Value fields = run_success(vm.datetime_to_fields(2, field_args));
    ASSERT_TRUE(fields.is_list());
    EXPECT_EQ(fields.as_list()->elements[0].as_int(), 1970);
    EXPECT_EQ(fields.as_list()->elements[1].as_int(), 1);
    EXPECT_EQ(fields.as_list()->elements[2].as_int(), 1);

    Value construct[] = {
        Value::from_int(2020),
        Value::from_int(5),
        Value::from_int(6),
        Value::from_int(7),
        Value::from_int(8),
        Value::from_int(9),
        utc,
    };
    Value timestamp = run_success(vm.datetime_from_fields(7, construct));
    ASSERT_TRUE(timestamp.is_int());
    EXPECT_EQ(timestamp.as_int(), 1588748889);

    Value text = vm.gc().make_string("2020-05-06T07:08:09Z");
    Value iso = vm.gc().make_string("ISO8601");
    Value parse[] = { text, iso, utc };
    EXPECT_EQ(run_success(vm.datetime_parse(3, parse)).as_int(), timestamp.as_int());
    Value format[] = { timestamp, utc, iso };
    EXPECT_EQ(as_std_string(run_success(vm.datetime_format(3, format))), "2020-05-06T07:08:09Z");
}

TEST(StdlibDatetime, CurrentTimeReturnsUnixSeconds)
{
    VM vm;
    auto before = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    Value current = run_success(vm.datetime_now(0, nullptr));
    auto after = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    ASSERT_TRUE(current.is_int());
    EXPECT_GE(current.as_int(), static_cast<i64>(before));
    EXPECT_LE(current.as_int(), static_cast<i64>(after));
}

TEST(StdlibCodecs, Base64SupportsStandardUrlSafeAndBinaryData)
{
    VM vm;
    Value standard_args[] = { vm.gc().make_string("hello"), Value::from_bool(false) };
    Value encoded = run_success(vm.base64_encode(2, standard_args));
    EXPECT_EQ(as_std_string(encoded), "aGVsbG8=");
    Value decode_args[] = { encoded, Value::from_bool(false) };
    EXPECT_EQ(as_std_string(run_success(vm.base64_decode(2, decode_args))), "hello");

    StringRef bytes(3, '\0');
    bytes[0] = static_cast<char>(0xfb);
    bytes[1] = static_cast<char>(0xff);
    bytes[2] = '\0';
    Value url_args[] = { vm.gc().make_string(bytes), Value::from_bool(true) };
    Value url_encoded = run_success(vm.base64_encode(2, url_args));
    EXPECT_EQ(as_std_string(url_encoded), "-_8A");
    Value url_decode_args[] = { url_encoded, Value::from_bool(true) };
    EXPECT_EQ(as_std_string(run_success(vm.base64_decode(2, url_decode_args))), as_std_string(url_args[0]));

    Value invalid_args[] = { vm.gc().make_string("not base64!"), Value::from_bool(false) };
    EXPECT_EQ(run_failed(vm.base64_decode(2, invalid_args)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(StdlibCodecs, HexRoundTripsEmbeddedNullAndRejectsMalformedInput)
{
    VM vm;
    StringRef bytes(3, '\0');
    bytes[0] = 'A';
    bytes[1] = '\0';
    bytes[2] = static_cast<char>(0xff);
    Value input = vm.gc().make_string(bytes);
    Value encoded = run_success(vm.hex_encode(1, &input));
    EXPECT_EQ(as_std_string(encoded), "4100ff");
    EXPECT_EQ(as_std_string(run_success(vm.hex_decode(1, &encoded))), as_std_string(input));

    Value malformed = vm.gc().make_string("xyz");
    EXPECT_EQ(run_failed(vm.hex_decode(1, &malformed)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(StdlibCodecs, Sha256SupportsIncrementalAndRawDigests)
{
    VM vm;
    Value algorithm = vm.gc().make_string("sha256");
    Value handle = run_success(vm.hash_new(1, &algorithm));
    ASSERT_TRUE(handle.is_dict());

    Value first_args[] = { handle, vm.gc().make_string("hello ") };
    Value second_args[] = { handle, vm.gc().make_string("world") };
    EXPECT_TRUE(run_success(vm.hash_update(2, first_args)).as_bool());
    EXPECT_TRUE(run_success(vm.hash_update(2, second_args)).as_bool());

    Value hex_args[] = { handle, Value::from_bool(true) };
    EXPECT_EQ(as_std_string(run_success(vm.hash_digest(2, hex_args))),
        "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    Value raw_args[] = { handle, Value::from_bool(false) };
    EXPECT_EQ(as_std_string(run_success(vm.hash_digest(2, raw_args))).size(), 32u);

    Value unsupported = vm.gc().make_string("md5");
    EXPECT_EQ(run_failed(vm.hash_new(1, &unsupported)).errc, ErrorCode::NATIVE_TYPE_ERROR);
}

TEST(StdlibCodecs, HmacSha256MatchesPublishedVector)
{
    VM vm;
    Value args[] = {
        vm.gc().make_string("sha256"),
        vm.gc().make_string("key"),
        vm.gc().make_string("The quick brown fox jumps over the lazy dog"),
    };
    EXPECT_EQ(as_std_string(run_success(vm.hmac(3, args))),
        "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

TEST(StdlibCompression, GzipRoundTripsAllSupportedBoundaryLevelsAndBinaryData)
{
    VM vm;
    StringRef payload(1027, 'a');
    payload[0] = '\0';
    payload[1026] = static_cast<char>(0xff);
    Value algorithm = vm.gc().make_string("gzip");
    Value input = vm.gc().make_string(payload);

    for (i64 level : { 0, 1, 6, 9 }) {
        Value compress_args[] = { algorithm, input, Value::from_int(level) };
        Value compressed = run_success(vm.compress(3, compress_args));
        ASSERT_TRUE(compressed.is_string());
        EXPECT_GT(compressed.as_string()->str.len(), 0u);

        Value decompress_args[] = { algorithm, compressed, Value::from_int(2048) };
        Value restored = run_success(vm.decompress(3, decompress_args));
        EXPECT_EQ(as_std_string(restored), as_std_string(input));
    }
}

TEST(StdlibCompression, GzipRejectsInvalidDataLevelAlgorithmAndOutputLimit)
{
    VM vm;
    Value gzip = vm.gc().make_string("gzip");
    Value text = vm.gc().make_string("compress me");
    Value bad_level_args[] = { gzip, text, Value::from_int(10) };
    EXPECT_EQ(run_failed(vm.compress(3, bad_level_args)).errc, ErrorCode::NATIVE_TYPE_ERROR);

    Value other = vm.gc().make_string("other");
    Value other_args[] = { other, text, Value::from_int(6) };
    EXPECT_EQ(run_failed(vm.compress(3, other_args)).errc, ErrorCode::NATIVE_TYPE_ERROR);

    Value good_args[] = { gzip, text, Value::from_int(6) };
    Value compressed = run_success(vm.compress(3, good_args));
    Value limited_args[] = { gzip, compressed, Value::from_int(5) };
    EXPECT_EQ(run_failed(vm.decompress(3, limited_args)).errc, ErrorCode::RUNTIME_ERROR);
    Value corrupt_args[] = { gzip, text, Value::from_int(100) };
    EXPECT_EQ(run_failed(vm.decompress(3, corrupt_args)).errc, ErrorCode::RUNTIME_ERROR);
}

TEST(StdlibRegression, ContainsEmptyNeedleIsTrue)
{
    VM vm;
    Value args[] = { str("fairuz"), str("") };
    auto result = run_success(vm.contains(2, args));

    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.as_bool());
}

TEST(StdlibRegression, ContainsExactMatchIsTrue)
{
    VM vm;
    Value args[] = { str("fairuz"), str("fairuz") };
    auto result = run_success(vm.contains(2, args));

    ASSERT_TRUE(result.is_bool());
    EXPECT_TRUE(result.as_bool());
}

TEST(StdlibRegression, StrStringifiesListsLikePrint)
{
    VM vm;
    Value list = make_list(vm, {
                                   Value::from_int(1),
                                   Value::from_bool(false),
                                   str("z"),
                               });
    auto result = run_success(vm.str(1, &list));

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), R"([1, خطا, "z"])");
}

TEST(StdlibRegression, StrStringifiesDictsLikePrint)
{
    VM vm;
    Value dict = run_success(vm.dict(0, nullptr));
    dict.as_dict()->data[str("k")] = Value::from_int(3);
    dict.as_dict()->data[str("name")] = str("fairuz");

    auto result = run_success(vm.str(1, &dict));

    ASSERT_TRUE(result.is_string());
    EXPECT_TRUE(as_std_string(result).find("\"k\": 3") != std::string::npos);
    EXPECT_TRUE(as_std_string(result).find("\"name\": \"fairuz\"") != std::string::npos);
}

TEST(StdlibRegression, LenSupportsDicts)
{
    VM vm;
    Value dict = run_success(vm.dict(0, nullptr));
    dict.as_dict()->data[str("a")] = Value::from_int(1);
    dict.as_dict()->data[str("b")] = Value::from_int(2);

    auto result = run_success(vm.len(1, &dict));

    ASSERT_TRUE(result.is_int());
    EXPECT_EQ(result.as_int(), 2);
}

TEST(StdlibRegression, DictConstructorPopulatesPairs)
{
    VM vm;
    Value args[] = {
        str("a"),
        Value::from_int(1),
        str("b"),
        Value::from_bool(true),
    };

    Value dict = run_success(vm.dict(4, args));

    ASSERT_TRUE(dict.is_dict());
    Value* a = dict.as_dict()->data.find_ptr(args[0]);
    Value* b = dict.as_dict()->data.find_ptr(args[2]);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->as_int(), 1);
    EXPECT_TRUE(b->is_bool());
    EXPECT_TRUE(b->as_bool());
}

TEST(StdlibDictionary, KeysPreserveInsertionOrderAndReassignmentPosition)
{
    VM vm;
    Value args[] = {
        str("first"),
        Value::from_int(1),
        str("second"),
        Value::from_int(2),
    };
    Value dict = run_success(vm.dict(4, args));
    Value replacement[] = { dict, args[0], Value::from_int(9) };
    vm.dict_put(&replacement[0], replacement[1], replacement[2]);

    Value keys = run_success(vm.dict_keys(1, &dict));
    ASSERT_TRUE(keys.is_list());
    ASSERT_EQ(keys.as_list()->elements.size(), 2u);
    EXPECT_EQ(as_std_string(keys.as_list()->elements[0]), "first");
    EXPECT_EQ(as_std_string(keys.as_list()->elements[1]), "second");
    EXPECT_EQ(dict.as_dict()->data.find_ptr(args[0])->as_int(), 9);
}

TEST(StdlibDictionary, ContainsAndDeleteReturnPythonStyleResults)
{
    VM vm;
    Value args[] = { str("key"), Value::from_int(42) };
    Value dict = run_success(vm.dict(2, args));
    Value query[] = { dict, args[0] };

    EXPECT_TRUE(run_success(vm.dict_contains(2, query)).as_bool());
    Value removed = run_success(vm.dict_delete(2, query));
    ASSERT_TRUE(removed.is_int());
    EXPECT_EQ(removed.as_int(), 42);
    EXPECT_FALSE(run_success(vm.dict_contains(2, query)).as_bool());
    EXPECT_TRUE(run_success(vm.dict_delete(2, query)).is_nil());
    EXPECT_TRUE(dict.as_dict()->insertion_order.empty());
}

TEST(StdlibDictionary, DynamicallyAllocatedEqualStringsShareAKey)
{
    VM vm;
    Value inserted_key = vm.gc().make_string("dynamic-key");
    Value lookup_key = vm.gc().make_string("dynamic-key");
    ASSERT_NE(inserted_key.as_obj(), lookup_key.as_obj());

    Value args[] = { inserted_key, Value::from_int(7) };
    Value dict = run_success(vm.dict(2, args));
    Value query[] = { dict, lookup_key };
    EXPECT_TRUE(run_success(vm.dict_contains(2, query)).as_bool());
    ASSERT_NE(dict.as_dict()->data.find_ptr(lookup_key), nullptr);
    EXPECT_EQ(dict.as_dict()->data.find_ptr(lookup_key)->as_int(), 7);
}

TEST(StdlibRegression, StrScalarConversionsMatchSurfaceSyntax)
{
    VM vm;

    Value int_value = Value::from_int(42);
    Value bool_value = Value::from_bool(true);
    Value nil_value = Value::nil();

    EXPECT_EQ(as_std_string(run_success(vm.str(1, &int_value))), "42");
    EXPECT_EQ(as_std_string(run_success(vm.str(1, &bool_value))), "صحيح");
    EXPECT_EQ(as_std_string(run_success(vm.str(1, &nil_value))), "nil");
}

TEST(StdlibRegression, TrimRemovesMixedLeadingAndTrailingWhitespace)
{
    VM vm;
    Value arg = str("\n"
                    "\t  fairuz  \r\n");
    auto result = run_success(vm.trim(1, &arg));

    ASSERT_TRUE(result.is_string());
    EXPECT_EQ(as_std_string(result), "fairuz");
}

TEST_F(StdlibPerfTest, SplitJoinRoundTripLargeCsv)
{
    VM vm;
    std::string csv;
    csv.reserve(32 * 2000);
    for (int i = 0; i < 2000; i++) {
        if (i)
            csv += ',';
        csv += "field";
        csv += std::to_string(i);
    }

    Value split_args[] = { str(csv.c_str()), str(",") };
    auto start = std::chrono::high_resolution_clock::now();
    Value parts = run_success(vm.split(2, split_args));
    double split_us = elapsed_us(start);

    ASSERT_TRUE(parts.is_list());
    ASSERT_EQ(parts.as_list()->elements.size(), 2000u);

    Value join_args[] = { parts, str(",") };
    start = std::chrono::high_resolution_clock::now();
    Value roundtrip = run_success(vm.join(2, join_args));
    double join_us = elapsed_us(start);

    ASSERT_TRUE(roundtrip.is_string());
    EXPECT_EQ(as_std_string(roundtrip), csv);
    std::printf("  stdlib split 2k fields: %.1f us, join: %.1f us\n", split_us, join_us);
}

TEST_F(StdlibPerfTest, LenOnLargeString100kCalls)
{
    VM vm;
    std::string payload(8192, 'x');
    Value arg = str(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    i64 last = -1;
    for (int i = 0; i < 100000; i++) {
        Value value = run_success(vm.len(1, &arg));
        ASSERT_TRUE(value.is_int());
        last = value.as_int();
    }
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, 8192);
    std::printf("  stdlib len 100k calls (8 KiB string): %.1f us\n", total_us);
}

TEST_F(StdlibPerfTest, TrimLargePaddedString50kCalls)
{
    VM vm;
    std::string payload(1024, ' ');
    payload += "fairuz";
    payload.append(1024, '\t');
    Value arg = str(payload.c_str());

    auto start = std::chrono::high_resolution_clock::now();
    std::string last;
    for (int i = 0; i < 50000; i++)
        last = as_std_string(run_success(vm.trim(1, &arg)));
    double total_us = elapsed_us(start);

    EXPECT_EQ(last, "fairuz");
    std::printf("  stdlib trim 50k calls (2 KiB padding): %.1f us\n", total_us);
}
