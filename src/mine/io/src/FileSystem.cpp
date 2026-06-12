/**
 * @file FileSystem.cpp
 * @brief mine::io::FileSystem Win32 实现。
 *
 * 目录遍历使用 FindFirstFileW/FindNextFileW；
 * 文件属性使用 GetFileAttributesExW。
 */

#include <mine/io/FileSystem.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

#include <mine/core/Assert.h>

#if defined(_WIN32)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#   include <fileapi.h>
#endif

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// UTF-8 ↔ UTF-16（Windows 专用）
// ──────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)

namespace {

std::wstring to_wide(const char* utf8, size_t len) noexcept {
    if (len == 0) return {};
    int wide_len = ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len),
                                         nullptr, 0);
    if (wide_len <= 0) return {};
    std::wstring result(static_cast<size_t>(wide_len), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len),
                          result.data(), wide_len);
    return result;
}

std::string from_wide(const wchar_t* wide, int wide_len = -1) noexcept {
    if (!wide) return {};
    if (wide_len < 0) wide_len = static_cast<int>(std::wcslen(wide));
    if (wide_len == 0) return {};

    int utf8_len = ::WideCharToMultiByte(CP_UTF8, 0, wide, wide_len,
                                         nullptr, 0, nullptr, nullptr);
    if (utf8_len <= 0) return {};
    std::string result(static_cast<size_t>(utf8_len), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, wide, wide_len,
                          result.data(), utf8_len, nullptr, nullptr);
    return result;
}

mine::core::StatusCode win32_error_to_status(DWORD error) noexcept {
    switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return mine::core::StatusCode::NotFound;
        case ERROR_ACCESS_DENIED:
            return mine::core::StatusCode::PermissionDenied;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
            return mine::core::StatusCode::AlreadyExists;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return mine::core::StatusCode::OutOfMemory;
        case ERROR_INVALID_PARAMETER:
            return mine::core::StatusCode::InvalidArg;
        default:
            return mine::core::StatusCode::Unknown;
    }
}

/// 将 FILETIME 转换为 FileTimestamp
FileTimestamp filetime_to_timestamp(const FILETIME& ft) noexcept {
    // FILETIME: 自 1601-01-01 起的 100 纳秒间隔
    // Unix 纪元: 1970-01-01
    // 差值: 11644473600 秒 = 116444736000000000 * 100 ns
    constexpr int64_t EPOCH_DIFF = 11644473600LL;

    ULARGE_INTEGER uli;
    uli.LowPart  = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    int64_t hundred_nanos = static_cast<int64_t>(uli.QuadPart);
    int64_t seconds = (hundred_nanos / 10000000LL) - EPOCH_DIFF;

    return FileTimestamp{seconds};
}

} // namespace

#endif // _WIN32

// ──────────────────────────────────────────────────────────────────────────────
// FileEntry::Impl（须在 entry_from_find_data 之前定义）
// ──────────────────────────────────────────────────────────────────────────────

struct FileEntry::Impl {
    Path            path;
    FileType        type               = FileType::Unknown;
    uint64_t        file_size          = 0;
    FileTimestamp   creation_time;
    FileTimestamp   last_access_time;
    FileTimestamp   last_write_time;
    bool            is_hidden          = false;
    bool            is_read_only       = false;

    Impl() noexcept = default;
};

// ──────────────────────────────────────────────────────────────────────────────
// DirectoryIterator::Impl（须在 list_dir 实现之前定义）
// ──────────────────────────────────────────────────────────────────────────────

struct DirectoryIterator::Impl {
#if defined(_WIN32)
    HANDLE          find_handle = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW find_data;
    bool            first = true;
    bool            finished = false;
#endif
    FileEntry       current_entry;
    Path            dir_path;

    Impl() noexcept = default;
};

// ──────────────────────────────────────────────────────────────────────────────
// 平台相关辅助函数（依赖 Impl 完整定义）
// ──────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)

namespace {

/// 从 WIN32_FIND_DATAW 填充 FileEntry
FileEntry entry_from_find_data(const WIN32_FIND_DATAW& fd, const Path& dir) noexcept {
    auto* impl = new FileEntry::Impl{};

    // 文件名
    std::string name = from_wide(fd.cFileName);
    impl->path = dir.join({name.data(), name.size()});

    // 类型
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        impl->type = FileType::Directory;
    } else if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        impl->type = FileType::Symlink;
    } else {
        impl->type = FileType::Regular;
    }

    // 大小
    ULARGE_INTEGER uli;
    uli.LowPart  = fd.nFileSizeLow;
    uli.HighPart = fd.nFileSizeHigh;
    impl->file_size = uli.QuadPart;

    // 时间戳
    impl->creation_time     = filetime_to_timestamp(fd.ftCreationTime);
    impl->last_access_time  = filetime_to_timestamp(fd.ftLastAccessTime);
    impl->last_write_time   = filetime_to_timestamp(fd.ftLastWriteTime);

    // 属性
    impl->is_hidden   = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    impl->is_read_only = (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;

    FileEntry entry;
    entry.impl_ = impl;
    return entry;
}

} // namespace

#endif // _WIN32

// ──────────────────────────────────────────────────────────────────────────────
// FileEntry 实现
// ──────────────────────────────────────────────────────────────────────────────

FileEntry::FileEntry() noexcept : impl_{new Impl{}} {}
FileEntry::~FileEntry() noexcept { delete impl_; }

FileEntry::FileEntry(const FileEntry& other) noexcept
    : impl_{new Impl{*other.impl_}} {}

FileEntry& FileEntry::operator=(const FileEntry& other) noexcept {
    if (this != &other) {
        *impl_ = *other.impl_;
    }
    return *this;
}

FileEntry::FileEntry(FileEntry&& other) noexcept
    : impl_{other.impl_} {
    other.impl_ = new Impl{};
}

FileEntry& FileEntry::operator=(FileEntry&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl{};
    }
    return *this;
}

const Path& FileEntry::path()               const noexcept { return impl_->path; }
FileType     FileEntry::type()              const noexcept { return impl_->type; }
bool         FileEntry::is_regular()        const noexcept { return impl_->type == FileType::Regular; }
bool         FileEntry::is_directory()      const noexcept { return impl_->type == FileType::Directory; }
bool         FileEntry::is_symlink()        const noexcept { return impl_->type == FileType::Symlink || impl_->type == FileType::Junction; }
uint64_t     FileEntry::file_size()         const noexcept { return impl_->file_size; }
FileTimestamp FileEntry::creation_time()    const noexcept { return impl_->creation_time; }
FileTimestamp FileEntry::last_access_time() const noexcept { return impl_->last_access_time; }
FileTimestamp FileEntry::last_write_time()  const noexcept { return impl_->last_write_time; }
bool         FileEntry::is_hidden()         const noexcept { return impl_->is_hidden; }
bool         FileEntry::is_read_only()      const noexcept { return impl_->is_read_only; }

// ──────────────────────────────────────────────────────────────────────────────
// DirectoryIterator 实现
// ──────────────────────────────────────────────────────────────────────────────

DirectoryIterator::DirectoryIterator() noexcept : impl_{new Impl{}} {}
DirectoryIterator::~DirectoryIterator() noexcept {
#if defined(_WIN32)
    if (impl_->find_handle != INVALID_HANDLE_VALUE) {
        ::FindClose(impl_->find_handle);
    }
#endif
    delete impl_;
}

DirectoryIterator::DirectoryIterator(DirectoryIterator&& other) noexcept
    : impl_{other.impl_} {
    other.impl_ = new Impl{};
}

DirectoryIterator& DirectoryIterator::operator=(DirectoryIterator&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl{};
    }
    return *this;
}

bool DirectoryIterator::done() const noexcept {
#if defined(_WIN32)
    return impl_->finished;
#else
    return true;
#endif
}

const FileEntry& DirectoryIterator::entry() const noexcept {
    return impl_->current_entry;
}

mine::core::Status DirectoryIterator::next() noexcept {
#if defined(_WIN32)
    if (impl_->finished) {
        return mine::core::Status{mine::core::StatusCode::OutOfRange};
    }

    // 第一次调用时，数据已经在构造函数中获取
    if (!impl_->first) {
        if (!::FindNextFileW(impl_->find_handle, &impl_->find_data)) {
            DWORD err = ::GetLastError();
            impl_->finished = true;
            if (err == ERROR_NO_MORE_FILES) {
                return mine::core::Status{mine::core::StatusCode::OutOfRange};
            }
            return mine::core::Status{win32_error_to_status(err)};
        }
    } else {
        impl_->first = false;
    }

    // 跳过 "." 和 ".."
    while (impl_->find_data.cFileName[0] == L'.') {
        if (impl_->find_data.cFileName[1] == L'\0' ||
            (impl_->find_data.cFileName[1] == L'.' && impl_->find_data.cFileName[2] == L'\0')) {
            if (!::FindNextFileW(impl_->find_handle, &impl_->find_data)) {
                impl_->finished = true;
                return mine::core::Status{mine::core::StatusCode::OutOfRange};
            }
            continue;
        }
        break;
    }

    impl_->current_entry = entry_from_find_data(impl_->find_data, impl_->dir_path);
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// FileSystem 操作实现
// ──────────────────────────────────────────────────────────────────────────────

namespace FileSystem {

mine::core::Result<bool> exists(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide = to_wide(path_str.data(), path_str.size());

    DWORD attr = ::GetFileAttributesW(wide.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD err = ::GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return mine::core::Result<bool>{mine::core::ok_tag, false};
        }
        return mine::core::Result<bool>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }
    return mine::core::Result<bool>{mine::core::ok_tag, true};
#else
    return mine::core::Result<bool>{mine::core::ok_tag, false};
#endif
}

mine::core::Result<FileEntry> stat(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide = to_wide(path_str.data(), path_str.size());

    WIN32_FILE_ATTRIBUTE_DATA attr_data;
    if (!::GetFileAttributesExW(wide.c_str(), GetFileExInfoStandard, &attr_data)) {
        DWORD err = ::GetLastError();
        return mine::core::Result<FileEntry>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    auto* impl = new FileEntry::Impl{};
    impl->path = path;

    if (attr_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        impl->type = FileType::Directory;
    } else if (attr_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        impl->type = FileType::Symlink;
    } else {
        impl->type = FileType::Regular;
    }

    ULARGE_INTEGER uli;
    uli.LowPart  = attr_data.nFileSizeLow;
    uli.HighPart = attr_data.nFileSizeHigh;
    impl->file_size = uli.QuadPart;

    impl->creation_time     = filetime_to_timestamp(attr_data.ftCreationTime);
    impl->last_access_time  = filetime_to_timestamp(attr_data.ftLastAccessTime);
    impl->last_write_time   = filetime_to_timestamp(attr_data.ftLastWriteTime);
    impl->is_hidden   = (attr_data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
    impl->is_read_only = (attr_data.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0;

    FileEntry entry;
    entry.impl_ = impl;
    return mine::core::Result<FileEntry>{mine::core::ok_tag, std::move(entry)};
#else
    return mine::core::Result<FileEntry>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Result<bool> is_directory(const Path& path) noexcept {
    auto result = stat(path);
    if (!result.ok()) return mine::core::Result<bool>{mine::core::err_tag, result.error()};
    return mine::core::Result<bool>{mine::core::ok_tag, result.value().is_directory()};
}

mine::core::Result<bool> is_regular_file(const Path& path) noexcept {
    auto result = stat(path);
    if (!result.ok()) return mine::core::Result<bool>{mine::core::err_tag, result.error()};
    return mine::core::Result<bool>{mine::core::ok_tag, result.value().is_regular()};
}

mine::core::Result<uint64_t> file_size(const Path& path) noexcept {
    auto result = stat(path);
    if (!result.ok()) return mine::core::Result<uint64_t>{mine::core::err_tag, result.error()};
    return mine::core::Result<uint64_t>{mine::core::ok_tag, result.value().file_size()};
}

// ── 目录操作 ──────────────────────────────────────────────────────────────────

mine::core::Result<DirectoryIterator> list_dir(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    // 构造搜索模式：目录路径 + "\\*"
    std::string pattern(path_str.data(), path_str.size());
    if (!pattern.empty() && pattern.back() != '/') {
        pattern.push_back('/');
    }
    pattern.push_back('*');

    std::wstring wide = to_wide(pattern.data(), pattern.size());

    WIN32_FIND_DATAW fd;
    HANDLE h = ::FindFirstFileW(wide.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = ::GetLastError();
        return mine::core::Result<DirectoryIterator>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    auto* impl = new DirectoryIterator::Impl{};
    impl->find_handle = h;
    impl->find_data = fd;
    impl->first = true;
    impl->finished = false;
    impl->dir_path = path;

    DirectoryIterator iter;
    delete iter.impl_;
    iter.impl_ = impl;

    return mine::core::Result<DirectoryIterator>{mine::core::ok_tag, std::move(iter)};
#else
    return mine::core::Result<DirectoryIterator>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Status create_dir(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide = to_wide(path_str.data(), path_str.size());

    if (!::CreateDirectoryW(wide.c_str(), nullptr)) {
        DWORD err = ::GetLastError();
        if (err == ERROR_ALREADY_EXISTS) {
            return mine::core::Status{mine::core::StatusCode::AlreadyExists};
        }
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

mine::core::Status create_dir_all(const Path& path) noexcept {
    if (path.empty() || path.string().size() == 0) {
        return mine::core::Status::success();
    }

    // 递归创建父目录
    Path parent = path.parent_path();
    if (!parent.empty() && parent.string().size() > 0 &&
        parent.string().size() != path.string().size()) {
        auto parent_exists = exists(parent);
        if (parent_exists.ok() && !parent_exists.value()) {
            auto status = create_dir_all(parent);
            if (!status.ok()) return status;
        }
    }

    return create_dir(path);
}

mine::core::Status remove_dir(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide = to_wide(path_str.data(), path_str.size());

    if (!::RemoveDirectoryW(wide.c_str())) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

/// 递归删除目录内部实现
static mine::core::Status remove_dir_all_impl(const Path& path, int depth) noexcept {
    constexpr int kMaxDepth = 64;
    if (depth > kMaxDepth) {
        return mine::core::Status{mine::core::StatusCode::OutOfRange};
    }

    // 遍历目录内容
    auto iter_result = list_dir(path);
    if (!iter_result.ok()) {
        return iter_result.error();
    }

    auto& iter = iter_result.value();
    while (!iter.done()) {
        const auto& entry = iter.entry();
        if (entry.is_directory()) {
            auto status = remove_dir_all_impl(entry.path(), depth + 1);
            if (!status.ok()) return status;
        } else {
            auto status = remove_file(entry.path());
            if (!status.ok()) return status;
        }
        auto next_status = iter.next();
        if (!next_status.ok() && next_status.code() != mine::core::StatusCode::OutOfRange) {
            return next_status;
        }
    }

    return remove_dir(path);
}

mine::core::Status remove_dir_all(const Path& path) noexcept {
    return remove_dir_all_impl(path, 0);
}

// ── 文件操作 ──────────────────────────────────────────────────────────────────

mine::core::Status remove_file(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide = to_wide(path_str.data(), path_str.size());

    if (!::DeleteFileW(wide.c_str())) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

mine::core::Status rename(const Path& from, const Path& to) noexcept {
#if defined(_WIN32)
    auto from_str = from.string();
    auto to_str = to.string();
    std::wstring w_from = to_wide(from_str.data(), from_str.size());
    std::wstring w_to   = to_wide(to_str.data(), to_str.size());

    if (!::MoveFileExW(w_from.c_str(), w_to.c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

mine::core::Status copy_file(const Path& from, const Path& to) noexcept {
#if defined(_WIN32)
    auto from_str = from.string();
    auto to_str = to.string();
    std::wstring w_from = to_wide(from_str.data(), from_str.size());
    std::wstring w_to   = to_wide(to_str.data(), to_str.size());

    // 不覆盖已有文件
    if (!::CopyFileW(w_from.c_str(), w_to.c_str(), TRUE /* failIfExists */)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

mine::core::Status copy_file_overwrite(const Path& from, const Path& to) noexcept {
#if defined(_WIN32)
    auto from_str = from.string();
    auto to_str = to.string();
    std::wstring w_from = to_wide(from_str.data(), from_str.size());
    std::wstring w_to   = to_wide(to_str.data(), to_str.size());

    if (!::CopyFileW(w_from.c_str(), w_to.c_str(), FALSE /* overwrite */)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

// ── 路径工具 ──────────────────────────────────────────────────────────────────

mine::core::Result<Path> current_dir() noexcept {
#if defined(_WIN32)
    DWORD len = ::GetCurrentDirectoryW(0, nullptr);
    if (len == 0) {
        return mine::core::Result<Path>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::Unknown}};
    }

    std::wstring wide(len, L'\0');
    DWORD actual = ::GetCurrentDirectoryW(len, wide.data());
    if (actual == 0 || actual > len) {
        return mine::core::Result<Path>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::Unknown}};
    }

    std::string utf8 = from_wide(wide.data(), static_cast<int>(actual));
    return mine::core::Result<Path>{mine::core::ok_tag,
        Path{utf8.data(), utf8.size()}};
#else
    return mine::core::Result<Path>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Status set_current_dir(const Path& path) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide = to_wide(path_str.data(), path_str.size());

    if (!::SetCurrentDirectoryW(wide.c_str())) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

mine::core::Result<Path> temp_dir() noexcept {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH + 1];
    DWORD len = ::GetTempPathW(MAX_PATH, buf);
    if (len == 0) {
        return mine::core::Result<Path>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::Unknown}};
    }
    std::string utf8 = from_wide(buf, len);
    return mine::core::Result<Path>{mine::core::ok_tag,
        Path{utf8.data(), utf8.size()}};
#else
    return mine::core::Result<Path>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Result<Path> home_dir() noexcept {
#if defined(_WIN32)
    // 优先使用 USERPROFILE，回退 HOMEDRIVE + HOMEPATH
    const char* userprofile = std::getenv("USERPROFILE");
    if (userprofile) {
        return mine::core::Result<Path>{mine::core::ok_tag, Path{userprofile}};
    }
    const char* homedrive = std::getenv("HOMEDRIVE");
    const char* homepath  = std::getenv("HOMEPATH");
    if (homedrive && homepath) {
        Path p{homedrive};
        p = p.join({homepath, std::strlen(homepath)});
        return mine::core::Result<Path>{mine::core::ok_tag, std::move(p)};
    }
    return mine::core::Result<Path>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotFound}};
#else
    const char* home = std::getenv("HOME");
    if (home) {
        return mine::core::Result<Path>{mine::core::ok_tag, Path{home}};
    }
    return mine::core::Result<Path>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotFound}};
#endif
}

mine::core::Result<Path> exe_dir() noexcept {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH + 1];
    DWORD len = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        return mine::core::Result<Path>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::Unknown}};
    }
    buf[len] = L'\0';

    std::string utf8 = from_wide(buf, len);
    Path exe_path{utf8.data(), utf8.size()};
    return mine::core::Result<Path>{mine::core::ok_tag, exe_path.parent_path()};
#else
    return mine::core::Result<Path>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

} // namespace FileSystem
} // namespace mine::io
