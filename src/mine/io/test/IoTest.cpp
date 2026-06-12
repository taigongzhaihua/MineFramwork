/**
 * @file IoTest.cpp
 * @brief mine.io 模块单元测试（doctest）
 *
 * 测试覆盖：
 *   - Path：构造、组件查询、拼接、比较、修改、规范化
 *   - File：打开/关闭、读写、定位、大小
 *   - FileSystem：exists、stat、目录遍历、创建/删除目录、文件操作
 *   - MemMap：只读映射、读写映射、span 访问
 *   - PipeStream：文件读写、缓冲读取、行读取、行写入
 */

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <mine/io/Io.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace mine::io;

// ──────────────────────────────────────────────────────────────────────────────
// 测试辅助：临时文件/目录管理
// ──────────────────────────────────────────────────────────────────────────────

namespace {

/// 获取测试用的临时目录
Path test_temp_dir() {
    auto result = FileSystem::temp_dir();
    if (result.ok()) {
        return result.value().join("mine_io_test");
    }
    return Path{"mine_io_test"};
}

/// 创建测试临时目录
void setup_test_dir() {
    auto dir = test_temp_dir();
    auto exists = FileSystem::exists(dir);
    if (exists.ok() && exists.value()) {
        FileSystem::remove_dir_all(dir);
    }
    FileSystem::create_dir_all(dir);
}

/// 清理测试临时目录
void cleanup_test_dir() {
    auto dir = test_temp_dir();
    FileSystem::remove_dir_all(dir);
}

/// 在测试目录中创建文件并写入内容
Path create_test_file(const char* name, const char* content) {
    auto dir = test_temp_dir();
    auto file_path = dir.join({name, std::strlen(name)});
    auto file = File::open(file_path, FileMode::DefaultWrite).value();
    file.write_all({content, std::strlen(content)});
    file.close();
    return file_path;
}

} // namespace

// ──────────────────────────────────────────────────────────────────────────────
// Path 测试
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("io_Path_DefaultConstructor_IsDot") {
    Path p;
    CHECK(p.string() == mine::core::StringView{"."});
    CHECK(p.is_relative());
}

TEST_CASE("io_Path_FromAbsolutePath_IsAbsolute") {
    Path p{"/home/user/docs"};
    CHECK(p.is_absolute());
    CHECK_FALSE(p.is_relative());
}

TEST_CASE("io_Path_FromRelativePath_IsRelative") {
    Path p{"src/main.cpp"};
    CHECK(p.is_relative());
    CHECK_FALSE(p.is_absolute());
}

TEST_CASE("io_Path_WindowsDriveLetter_IsAbsolute") {
    Path p{"C:/Windows/System32"};
    CHECK(p.is_absolute());
}

TEST_CASE("io_Path_WindowsBackslash_IsNormalized") {
    Path p{"C:\\Users\\test\\file.txt"};
    CHECK(p.string() == mine::core::StringView{"C:/Users/test/file.txt"});
}

TEST_CASE("io_Path_ParentPath_ReturnsParent") {
    Path p{"/home/user/docs/file.txt"};
    Path parent = p.parent_path();
    CHECK(parent.string() == mine::core::StringView{"/home/user/docs"});
}

TEST_CASE("io_Path_ParentPath_Root_ReturnsRoot") {
    Path p{"/home"};
    Path parent = p.parent_path();
    CHECK(parent.string() == mine::core::StringView{"/"});
}

TEST_CASE("io_Path_Filename_ReturnsLastComponent") {
    Path p{"/home/user/file.txt"};
    CHECK(p.filename() == mine::core::StringView{"file.txt"});
}

TEST_CASE("io_Path_Extension_ReturnsWithDot") {
    Path p{"/home/user/file.txt"};
    CHECK(p.extension() == mine::core::StringView{".txt"});
}

TEST_CASE("io_Path_Extension_NoExtension_ReturnsEmpty") {
    Path p{"/home/user/file"};
    CHECK(p.extension().empty());
}

TEST_CASE("io_Path_Stem_ReturnsNameWithoutExt") {
    Path p{"/home/user/file.txt"};
    CHECK(p.stem() == mine::core::StringView{"file"});
}

TEST_CASE("io_Path_Join_AppendsPath") {
    Path p{"/home/user"};
    Path joined = p.join({"docs"});
    CHECK(joined.string() == mine::core::StringView{"/home/user/docs"});
}

TEST_CASE("io_Path_OperatorSlash_AppendsPath") {
    Path p{"/home/user"};
    Path joined = p / "docs";
    CHECK(joined.string() == mine::core::StringView{"/home/user/docs"});
}

TEST_CASE("io_Path_Equality_SamePath_Equal") {
    Path a{"/home/user"};
    Path b{"/home/user"};
    CHECK(a == b);
}

TEST_CASE("io_Path_Equality_DifferentPath_NotEqual") {
    Path a{"/home/user"};
    Path b{"/home/admin"};
    CHECK(a != b);
}

TEST_CASE("io_Path_StartsWith_MatchesPrefix") {
    Path p{"/home/user/docs"};
    CHECK(p.starts_with({"/home"}));
    CHECK_FALSE(p.starts_with({"/var"}));
}

TEST_CASE("io_Path_EndsWith_MatchesSuffix") {
    Path p{"/home/user/file.txt"};
    CHECK(p.ends_with({".txt"}));
    CHECK_FALSE(p.ends_with({".md"}));
}

TEST_CASE("io_Path_ReplaceExtension_ChangesExt") {
    Path p{"/home/user/file.txt"};
    p.replace_extension({".md"});
    CHECK(p.string() == mine::core::StringView{"/home/user/file.md"});
}

TEST_CASE("io_Path_RemoveExtension_StripsExt") {
    Path p{"/home/user/file.txt"};
    p.remove_extension();
    CHECK(p.string() == mine::core::StringView{"/home/user/file"});
}

TEST_CASE("io_Path_Normalize_RemovesRedundantSeparators") {
    Path p{"a//b///c"};
    CHECK(p.string() == mine::core::StringView{"a/b/c"});
}

TEST_CASE("io_Path_Normalize_RemovesDotSegments") {
    Path p{"/a/./b/./c"};
    CHECK(p.string() == mine::core::StringView{"/a/b/c"});
}

TEST_CASE("io_Path_CopyConstructor_DeepCopy") {
    Path a{"/home/user"};
    Path b{a};
    CHECK(a == b);
    b.replace_extension({".bak"});
    CHECK(a != b);
}

TEST_CASE("io_Path_MoveConstructor_TransfersOwnership") {
    Path a{"/home/user"};
    Path b{std::move(a)};
    CHECK(b.string() == mine::core::StringView{"/home/user"});
}

// ──────────────────────────────────────────────────────────────────────────────
// File 测试
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("io_File_OpenReadWriteRoundTrip") {
    setup_test_dir();

    auto file_path = test_temp_dir().join({"roundtrip_test.txt"});
    const char* content = "Hello, MineFramework IO!";

    // 写入
    {
        auto file = File::open(file_path, FileMode::DefaultWrite).value();
        CHECK(file.is_open());
        auto written = file.write({content, std::strlen(content)}).value();
        CHECK(written == std::strlen(content));
    }  // 析构自动关闭

    // 读取
    {
        auto file = File::open(file_path, FileMode::DefaultRead).value();
        CHECK(file.is_open());
        char buf[256] = {};
        auto n = file.read({buf, sizeof(buf)}).value();
        CHECK(n == std::strlen(content));
        CHECK(std::strcmp(buf, content) == 0);
    }

    cleanup_test_dir();
}

TEST_CASE("io_File_ReadAll_ReadsEntireFile") {
    setup_test_dir();

    const char* content = "Full file content for read_all test";
    auto file_path = create_test_file("read_all.txt", content);

    auto file = File::open(file_path, FileMode::DefaultRead).value();
    char buf[256] = {};
    auto status = file.read_all({buf, sizeof(buf)});
    CHECK(status.ok());
    CHECK(std::strcmp(buf, content) == 0);

    cleanup_test_dir();
}

TEST_CASE("io_File_SeekTell_PositionsCorrectly") {
    setup_test_dir();

    const char* content = "0123456789";
    auto file_path = create_test_file("seek_test.txt", content);

    auto file = File::open(file_path, FileMode::DefaultRead).value();

    // 定位到偏移 5
    auto pos = file.seek(5, FileSeekOrigin::Begin).value();
    CHECK(pos == 5);

    // 读取一个字符，应读到 '5'
    char ch = 0;
    file.read({&ch, 1});
    CHECK(ch == '5');

    // 从当前位置前进 2，应到 8
    auto pos2 = file.seek(2, FileSeekOrigin::Current).value();
    CHECK(pos2 == 8);

    // tell 应返回 8
    auto tell_pos = file.tell().value();
    CHECK(tell_pos == 8);

    cleanup_test_dir();
}

TEST_CASE("io_File_Size_ReturnsCorrectSize") {
    setup_test_dir();

    const char* content = "SizeTest12345";  // 13 bytes
    auto file_path = create_test_file("size_test.txt", content);

    auto file = File::open(file_path, FileMode::DefaultRead).value();
    auto sz = file.size().value();
    CHECK(sz == 13);

    cleanup_test_dir();
}

TEST_CASE("io_File_WriteAll_WritesAllData") {
    setup_test_dir();

    auto file_path = test_temp_dir().join({"write_all.txt"});
    const char* content = "Write all data at once!";

    auto file = File::open(file_path, FileMode::DefaultWrite).value();
    auto status = file.write_all({content, std::strlen(content)});
    CHECK(status.ok());
    file.close();

    // 验证
    auto read_file = File::open(file_path, FileMode::DefaultRead).value();
    char buf[128] = {};
    read_file.read({buf, sizeof(buf)});
    CHECK(std::strcmp(buf, content) == 0);

    cleanup_test_dir();
}

TEST_CASE("io_File_OpenNonExistent_ReturnsError") {
    Path path{"/non_existent_file_xyz_12345.txt"};
    auto result = File::open(path, FileMode::DefaultRead);
    CHECK_FALSE(result.ok());
}

TEST_CASE("io_File_MoveSemantics_TransfersHandle") {
    setup_test_dir();

    auto file_path = create_test_file("move_test.txt", "data");

    auto file1 = File::open(file_path, FileMode::DefaultRead).value();
    CHECK(file1.is_open());

    // 移动
    auto file2 = std::move(file1);
    CHECK_FALSE(file1.is_open());
    CHECK(file2.is_open());

    char buf[10] = {};
    file2.read({buf, sizeof(buf)});
    CHECK(std::strcmp(buf, "data") == 0);

    cleanup_test_dir();
}

// ──────────────────────────────────────────────────────────────────────────────
// FileSystem 测试
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("io_FileSystem_Exists_ReturnsTrueForExisting") {
    setup_test_dir();

    auto file_path = create_test_file("exists_test.txt", "hello");
    auto result = FileSystem::exists(file_path);
    CHECK(result.ok());
    CHECK(result.value() == true);

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_Exists_ReturnsFalseForNonExisting") {
    Path path{"/definitely_not_exists_12345.xyz"};
    auto result = FileSystem::exists(path);
    CHECK(result.ok());
    CHECK(result.value() == false);
}

TEST_CASE("io_FileSystem_Stat_ReturnsFileInfo") {
    setup_test_dir();

    const char* content = "Stat test content";
    auto file_path = create_test_file("stat_test.txt", content);

    auto entry = FileSystem::stat(file_path).value();
    CHECK(entry.is_regular());
    CHECK_FALSE(entry.is_directory());
    CHECK(entry.file_size() == std::strlen(content));

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_Stat_Directory") {
    setup_test_dir();

    auto dir = test_temp_dir();
    auto entry = FileSystem::stat(dir).value();
    CHECK(entry.is_directory());
    CHECK_FALSE(entry.is_regular());

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_CreateDirRemoveDir_RoundTrip") {
    setup_test_dir();

    auto dir = test_temp_dir().join({"test_subdir"});

    // 创建
    auto status = FileSystem::create_dir(dir);
    CHECK(status.ok());

    // 存在
    auto exists = FileSystem::exists(dir);
    CHECK(exists.ok());
    CHECK(exists.value());

    // 删除
    status = FileSystem::remove_dir(dir);
    CHECK(status.ok());

    // 不再存在
    exists = FileSystem::exists(dir);
    CHECK(exists.ok());
    CHECK_FALSE(exists.value());

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_CreateDirAll_CreatesTree") {
    setup_test_dir();

    auto dir = test_temp_dir().join({"a"}).join({"b"}).join({"c"});

    auto status = FileSystem::create_dir_all(dir);
    CHECK(status.ok());

    auto exists = FileSystem::exists(dir);
    CHECK(exists.ok());
    CHECK(exists.value());

    // 清理父目录
    FileSystem::remove_dir_all(test_temp_dir().join({"a"}));

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_ListDir_EnumeratesEntries") {
    setup_test_dir();

    auto dir = test_temp_dir();
    create_test_file("a.txt", "a");
    create_test_file("b.txt", "bb");
    create_test_file("c.txt", "ccc");

    FileSystem::create_dir(dir.join({"sub"}));

    auto iter = FileSystem::list_dir(dir).value();
    CHECK_FALSE(iter.done());

    int count = 0;
    while (!iter.done()) {
        const auto& entry = iter.entry();
        auto name = entry.path().filename();
        // 至少有文件名
        CHECK_FALSE(name.empty());
        ++count;

        auto next_status = iter.next();
        if (!next_status.ok()) break;
    }
    CHECK(count >= 4);  // a.txt, b.txt, c.txt, sub (可能还有 . 和 ..)

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_RemoveFile_DeletesFile") {
    setup_test_dir();

    auto file_path = create_test_file("to_delete.txt", "bye");
    CHECK(FileSystem::exists(file_path).value());

    auto status = FileSystem::remove_file(file_path);
    CHECK(status.ok());

    CHECK_FALSE(FileSystem::exists(file_path).value());

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_Rename_MovesFile") {
    setup_test_dir();

    auto src = create_test_file("rename_src.txt", "source");
    auto dst = test_temp_dir().join({"rename_dst.txt"});

    auto status = FileSystem::rename(src, dst);
    CHECK(status.ok());

    CHECK_FALSE(FileSystem::exists(src).value());
    CHECK(FileSystem::exists(dst).value());

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_CopyFile_CopiesContent") {
    setup_test_dir();

    const char* content = "copy me";
    auto src = create_test_file("copy_src.txt", content);
    auto dst = test_temp_dir().join({"copy_dst.txt"});

    auto status = FileSystem::copy_file(src, dst);
    CHECK(status.ok());

    // 验证内容
    auto file = File::open(dst, FileMode::DefaultRead).value();
    char buf[32] = {};
    file.read({buf, sizeof(buf)});
    CHECK(std::strcmp(buf, content) == 0);

    cleanup_test_dir();
}

TEST_CASE("io_FileSystem_CurrentDir_ReturnsValid") {
    auto result = FileSystem::current_dir();
    CHECK(result.ok());
    CHECK_FALSE(result.value().empty());
}

TEST_CASE("io_FileSystem_TempDir_ReturnsValid") {
    auto result = FileSystem::temp_dir();
    CHECK(result.ok());
    CHECK_FALSE(result.value().empty());
}

// ──────────────────────────────────────────────────────────────────────────────
// MemMap 测试
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("io_MemMap_OpenRead_MapsFileContent") {
    setup_test_dir();

    const char* content = "Memory mapped file content";
    auto file_path = create_test_file("mmap_test.txt", content);

    auto mmap = MemMap::open_read(file_path).value();
    CHECK(mmap.is_mapped());
    CHECK(mmap.size() == std::strlen(content));

    auto span = mmap.span();
    CHECK(std::strncmp(span.data(), content, span.size()) == 0);

    mmap.close();
    CHECK_FALSE(mmap.is_mapped());

    cleanup_test_dir();
}

TEST_CASE("io_MemMap_ByteSpan_ReturnsCorrectView") {
    setup_test_dir();

    const char* content = "Byte span test";
    auto file_path = create_test_file("bytespan_test.txt", content);

    auto mmap = MemMap::open_read(file_path).value();
    auto byte_span = mmap.byte_span();

    CHECK(byte_span.size() == std::strlen(content));
    CHECK(byte_span[0] == static_cast<uint8_t>('B'));

    cleanup_test_dir();
}

TEST_CASE("io_MemMap_MoveSemantics_TransfersMapping") {
    setup_test_dir();

    auto file_path = create_test_file("mmap_move.txt", "movable");

    auto mmap1 = MemMap::open_read(file_path).value();
    CHECK(mmap1.is_mapped());

    auto mmap2 = std::move(mmap1);
    CHECK_FALSE(mmap1.is_mapped());
    CHECK(mmap2.is_mapped());

    auto span = mmap2.span();
    CHECK(span.size() == 7);  // "movable"

    cleanup_test_dir();
}

// ──────────────────────────────────────────────────────────────────────────────
// PipeStream 测试
// ──────────────────────────────────────────────────────────────────────────────

TEST_CASE("io_PipeStream_ReadFile_ReadsContent") {
    setup_test_dir();

    const char* content = "Stream reading test data";
    auto file_path = create_test_file("stream_read.txt", content);

    auto stream = PipeStream::from_file_read(file_path).value();
    CHECK(stream.is_open());

    char buf[128] = {};
    auto n = stream.read_some({buf, sizeof(buf)}).value();
    CHECK(n == std::strlen(content));
    CHECK(std::strcmp(buf, content) == 0);

    // 应到 EOF
    auto eof_result = stream.read_some({buf, sizeof(buf)}).value();
    CHECK(eof_result == 0);
    CHECK(stream.eof());

    cleanup_test_dir();
}

TEST_CASE("io_PipeStream_WriteFile_WritesContent") {
    setup_test_dir();

    auto file_path = test_temp_dir().join({"stream_write.txt"});
    const char* content = "Write stream content";

    {
        auto stream = PipeStream::from_file_write(file_path).value();
        auto status = stream.write_all({content, std::strlen(content)});
        CHECK(status.ok());
    }  // 析构自动 flush 和关闭

    // 验证
    auto file = File::open(file_path, FileMode::DefaultRead).value();
    char buf[128] = {};
    file.read({buf, sizeof(buf)});
    CHECK(std::strcmp(buf, content) == 0);

    cleanup_test_dir();
}

TEST_CASE("io_PipeStream_ReadLine_ReadsByDelimiter") {
    setup_test_dir();

    const char* content = "Line1\nLine2\nLine3";
    auto file_path = create_test_file("lines.txt", content);

    auto stream = PipeStream::from_file_read(file_path).value();

    char line_buf[64] = {};

    // Line 1
    auto len = stream.read_line({line_buf, sizeof(line_buf)}).value();
    CHECK(len == 5);
    CHECK(std::strncmp(line_buf, "Line1", 5) == 0);

    // Line 2
    len = stream.read_line({line_buf, sizeof(line_buf)}).value();
    CHECK(len == 5);
    CHECK(std::strncmp(line_buf, "Line2", 5) == 0);

    // Line 3
    len = stream.read_line({line_buf, sizeof(line_buf)}).value();
    CHECK(len == 5);
    CHECK(std::strncmp(line_buf, "Line3", 5) == 0);

    cleanup_test_dir();
}

TEST_CASE("io_PipeStream_WriteLine_AppendsNewline") {
    setup_test_dir();

    auto file_path = test_temp_dir().join({"writeline.txt"});

    {
        auto stream = PipeStream::from_file_write(file_path).value();
        stream.write_line({"Hello", 5});
        stream.write_line({"World", 5});
    }

    auto file = File::open(file_path, FileMode::DefaultRead).value();
    char buf[64] = {};
    file.read({buf, sizeof(buf)});
    CHECK(std::strcmp(buf, "Hello\nWorld\n") == 0);

    cleanup_test_dir();
}

TEST_CASE("io_PipeStream_Append_AppendsToEnd") {
    setup_test_dir();

    auto file_path = test_temp_dir().join({"append.txt"});

    // 先写一些内容
    {
        auto stream = PipeStream::from_file_write(file_path).value();
        stream.write_all({"First", 5});
    }

    // 追加
    {
        auto stream = PipeStream::from_file_append(file_path).value();
        stream.write_all({"Second", 6});
    }

    // 验证
    auto file = File::open(file_path, FileMode::DefaultRead).value();
    char buf[32] = {};
    file.read({buf, sizeof(buf)});
    CHECK(std::strcmp(buf, "FirstSecond") == 0);

    cleanup_test_dir();
}
