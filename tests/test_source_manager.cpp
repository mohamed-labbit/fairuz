//===----------------------------------------------------------------------===//
// Fairuz — Arabic programming language
// Copyright (c) 2026 Mohamed labbit
//
// SPDX-License-Identifier: MIT
// See LICENSE in the project root for the full license text.
//
// Verifies source loading and reload failure handling.
//===----------------------------------------------------------------------===//

#include "SourceManager.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

using namespace fairuz;

namespace {

struct SourceFile {
    std::filesystem::path path = std::filesystem::temp_directory_path()
        / ("fairuz-source-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".fa");
    explicit SourceFile(std::string const& bytes)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), bytes.size());
    }
    ~SourceFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
};

}

TEST(SourceManagerTest, PreservesBytesAndSupportsInvalidUtf8)
{
    std::string bytes = "س\r\n";
    bytes += std::string("\0\xff \t\n", 5);
    SourceFile file(bytes);
    FileManager manager(file.path.string());
    EXPECT_EQ(manager.buffer(), bytes);
    SourceManager source(manager);
    EXPECT_EQ(source.buffer(), bytes);
    EXPECT_EQ(source.line(1), "س");
    EXPECT_EQ(source.line(2), std::string("\0\xff \t", 4));
    EXPECT_EQ(source.line(0), "");
    EXPECT_EQ(source.line(3), "");
    EXPECT_EQ(source.line(4), "");
    // Byte slices deliberately need not align with Unicode boundaries.
    EXPECT_EQ(source.source_slice(1, 2), bytes.substr(1, 1));
    EXPECT_EQ(source.source_slice(0, SIZE_MAX), bytes);
    EXPECT_TRUE(source.source_slice(SIZE_MAX, SIZE_MAX).empty());
    EXPECT_TRUE(source.source_slice(2, 1).empty());
}

TEST(SourceManagerTest, SnapshotOutlivesAndIgnoresChangesToFileManager)
{
    SourcePtr source;
    {
        FileManager file;
        file.buffer() = "original\n";
        source = std::make_shared<SourceManager>(file);
        file.buffer() = "changed";
        EXPECT_EQ(source->line(1), "original");
    }
    EXPECT_EQ(source->path, "<input>");
    EXPECT_EQ(source->buffer(), "original\n");
}

TEST(SourceManagerTest, FailedReloadPreservesFileAndReturnsTypedFailure)
{
    SourceFile first("first\n");
    SourceFile second("second\n");
    FileManager file(first.path.string());
    EXPECT_EQ(file.load(second.path.string()), "second\n");
    EXPECT_EQ(file.buffer(), "first\n");
    EXPECT_EQ(file.get_path(), first.path.string());
    try {
        file.load(first.path.string() + ".missing", true);
        FAIL() << "Missing file should fail";
    } catch (FileError const& error) {
        EXPECT_EQ(error.kind(), FileError::Kind::NOT_FOUND);
        EXPECT_EQ(error.path(), first.path.string() + ".missing");
    }
    EXPECT_EQ(file.buffer(), "first\n");
    EXPECT_EQ(file.get_path(), first.path.string());
    file.load(second.path.string(), true);
    EXPECT_EQ(file.buffer(), "second\n");
    EXPECT_EQ(file.get_line_at(1), "second");
    EXPECT_EQ(file.get_line_at(99), "");
}

TEST(SourceManagerTest, DirectoryIsNotAnInputFile)
{
    try {
        FileManager file(std::filesystem::temp_directory_path().string());
        FAIL() << "Directory should fail";
    } catch (FileError const& error) {
        EXPECT_EQ(error.kind(), FileError::Kind::NOT_OPEN);
    }
}
