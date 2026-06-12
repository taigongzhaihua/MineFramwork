/**
 * @file File.cpp
 * @brief mine::io::File Win32 实现。
 *
 * 使用 Win32 CreateFileW/ReadFile/WriteFile API 实现文件操作。
 * UTF-8 路径自动转换为 UTF-16 供 Win32 API 使用。
 */

#include <mine/io/File.h>

#include <algorithm>
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
#endif

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// UTF-8 → UTF-16 转换（Windows 专用）
// ──────────────────────────────────────────────────────────────────────────────

#if defined(_WIN32)

namespace {

/// 将 UTF-8 字符串转换为 UTF-16（宽字符串）
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

/// 将 StatusCode 映射为 Win32 具体错误
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
        case ERROR_TIMEOUT:
            return mine::core::StatusCode::Timeout;
        case ERROR_OPERATION_ABORTED:
            return mine::core::StatusCode::Cancelled;
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL:
            return mine::core::StatusCode::IoError;
        default:
            return mine::core::StatusCode::Unknown;
    }
}

} // namespace

#endif // _WIN32

// ──────────────────────────────────────────────────────────────────────────────
// File::Impl（平台相关内部状态）
// ──────────────────────────────────────────────────────────────────────────────

struct File::Impl {
#if defined(_WIN32)
    HANDLE handle = INVALID_HANDLE_VALUE;  ///< Win32 文件句柄
#else
    int    fd     = -1;                    ///< POSIX 文件描述符（预留）
#endif
    bool   is_open_flag = false;            ///< 文件是否已打开

    /// 检查句柄是否有效
    [[nodiscard]] bool valid() const noexcept {
#if defined(_WIN32)
        return handle != INVALID_HANDLE_VALUE;
#else
        return fd >= 0;
#endif
    }

    /// 关闭句柄
    mine::core::Status close() noexcept {
#if defined(_WIN32)
        if (handle != INVALID_HANDLE_VALUE) {
            if (!::CloseHandle(handle)) {
                DWORD err = ::GetLastError();
                handle = INVALID_HANDLE_VALUE;
                is_open_flag = false;
                return mine::core::Status{win32_error_to_status(err)};
            }
            handle = INVALID_HANDLE_VALUE;
        }
#else
        // POSIX 预留
#endif
        is_open_flag = false;
        return mine::core::Status::success();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 构造/析构
// ──────────────────────────────────────────────────────────────────────────────

File::File() noexcept
    : impl_{new Impl{}}
{}

File::File(File&& other) noexcept
    : impl_{other.impl_}
{
    other.impl_ = new Impl{};
}

File& File::operator=(File&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl{};
    }
    return *this;
}

File::~File() noexcept {
    if (impl_->is_open_flag) {
        (void)impl_->close();  // 析构中忽略关闭失败
    }
    delete impl_;
}

File::File(Impl* impl) noexcept
    : impl_{impl}
{}

// ──────────────────────────────────────────────────────────────────────────────
// 打开
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<File>
File::open(const Path& path, FileMode mode) noexcept {
    return open_with_access(path, mode, FileAccess::Default);
}

mine::core::Result<File>
File::open_with_access(const Path& path, FileMode mode, FileAccess /*access*/) noexcept {
#if defined(_WIN32)
    // 将 FileMode 转换为 Win32 标志
    DWORD desired_access = 0;
    if (has_flag(mode, FileMode::Read))  desired_access |= GENERIC_READ;
    if (has_flag(mode, FileMode::Write)) desired_access |= GENERIC_WRITE;

    DWORD creation_disposition = 0;
    if (has_flag(mode, FileMode::Create)) {
        if (has_flag(mode, FileMode::Exclusive)) {
            creation_disposition = CREATE_NEW;
        } else if (has_flag(mode, FileMode::Truncate)) {
            creation_disposition = CREATE_ALWAYS;
        } else {
            creation_disposition = OPEN_ALWAYS;
        }
    } else {
        if (has_flag(mode, FileMode::Truncate)) {
            creation_disposition = TRUNCATE_EXISTING;
        } else {
            creation_disposition = OPEN_EXISTING;
        }
    }

    DWORD flags = FILE_ATTRIBUTE_NORMAL;

    // UTF-8 转 UTF-16
    auto path_str = path.string();
    std::wstring wide_path = to_wide(path_str.data(), path_str.size());

    HANDLE h = ::CreateFileW(
        wide_path.c_str(),
        desired_access,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        creation_disposition,
        flags,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = ::GetLastError();
        return mine::core::Result<File>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    // 追加模式：定位到文件末尾
    if (has_flag(mode, FileMode::Append)) {
        LARGE_INTEGER li_zero;
        li_zero.QuadPart = 0;
        ::SetFilePointerEx(h, li_zero, nullptr, FILE_END);
    }

    auto* impl = new Impl{};
    impl->handle = h;
    impl->is_open_flag = true;
    return mine::core::Result<File>{mine::core::ok_tag, File{impl}};
#else
    // POSIX 预留
    return mine::core::Result<File>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// 状态查询
// ──────────────────────────────────────────────────────────────────────────────

bool File::is_open() const noexcept {
    return impl_->is_open_flag;
}

mine::core::Status File::close() noexcept {
    return impl_->close();
}

// ──────────────────────────────────────────────────────────────────────────────
// 读取
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<size_t>
File::read(mine::core::Span<char> buf) noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

#if defined(_WIN32)
    DWORD bytes_read = 0;
    BOOL ok = ::ReadFile(impl_->handle, buf.data(),
                         static_cast<DWORD>(buf.size()),
                         &bytes_read, nullptr);
    if (!ok) {
        DWORD err = ::GetLastError();
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }
    return mine::core::Result<size_t>{mine::core::ok_tag,
        static_cast<size_t>(bytes_read)};
#else
    return mine::core::Result<size_t>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Result<mine::core::Status>
File::read_all(mine::core::Span<char> out_buffer) noexcept {
    auto file_size_result = size();
    if (!file_size_result.ok()) {
        return mine::core::Result<mine::core::Status>{mine::core::err_tag,
            file_size_result.error()};
    }

    size_t file_sz = file_size_result.value();
    if (file_sz > out_buffer.size()) {
        return mine::core::Result<mine::core::Status>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::OutOfRange}};
    }

    auto seek_result = seek(0, FileSeekOrigin::Begin);
    if (!seek_result.ok()) {
        return mine::core::Result<mine::core::Status>{mine::core::err_tag,
            seek_result.error()};
    }

    // 读取全部内容
    size_t total_read = 0;
    while (total_read < file_sz) {
        auto read_result = read(out_buffer.subspan(total_read));
        if (!read_result.ok()) {
            return mine::core::Result<mine::core::Status>{mine::core::err_tag,
                read_result.error()};
        }
        size_t n = read_result.value();
        if (n == 0) break;  // EOF
        total_read += n;
    }

    return mine::core::Result<mine::core::Status>{mine::core::ok_tag,
        mine::core::Status::success()};
}

mine::core::Result<size_t>
File::read_exact(mine::core::Span<char> buf) noexcept {
    size_t total = 0;
    while (total < buf.size()) {
        auto result = read(buf.subspan(total));
        if (!result.ok()) {
            return result;
        }
        size_t n = result.value();
        if (n == 0) break;  // EOF
        total += n;
    }
    return mine::core::Result<size_t>{mine::core::ok_tag, total};
}

// ──────────────────────────────────────────────────────────────────────────────
// 写入
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<size_t>
File::write(mine::core::Span<const char> buf) noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

#if defined(_WIN32)
    DWORD bytes_written = 0;
    BOOL ok = ::WriteFile(impl_->handle, buf.data(),
                          static_cast<DWORD>(buf.size()),
                          &bytes_written, nullptr);
    if (!ok) {
        DWORD err = ::GetLastError();
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }
    return mine::core::Result<size_t>{mine::core::ok_tag,
        static_cast<size_t>(bytes_written)};
#else
    return mine::core::Result<size_t>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Status File::write_all(mine::core::Span<const char> buf) noexcept {
    size_t total = 0;
    while (total < buf.size()) {
        auto result = write(buf.subspan(total));
        if (!result.ok()) {
            return result.error();
        }
        size_t n = result.value();
        if (n == 0) {
            return mine::core::Status{mine::core::StatusCode::IoError};
        }
        total += n;
    }
    return mine::core::Status::success();
}

mine::core::Status File::flush() noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Status{mine::core::StatusCode::InvalidState};
    }

#if defined(_WIN32)
    if (!::FlushFileBuffers(impl_->handle)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
#endif
    return mine::core::Status::success();
}

// ──────────────────────────────────────────────────────────────────────────────
// 定位
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<size_t>
File::seek(int64_t offset, FileSeekOrigin origin) noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

#if defined(_WIN32)
    DWORD move_method = 0;
    switch (origin) {
        case FileSeekOrigin::Begin:   move_method = FILE_BEGIN;   break;
        case FileSeekOrigin::Current: move_method = FILE_CURRENT; break;
        case FileSeekOrigin::End:     move_method = FILE_END;     break;
    }

    LARGE_INTEGER li_offset;
    li_offset.QuadPart = offset;

    LARGE_INTEGER new_pos;
    if (!::SetFilePointerEx(impl_->handle, li_offset, &new_pos, move_method)) {
        DWORD err = ::GetLastError();
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }
    return mine::core::Result<size_t>{mine::core::ok_tag,
        static_cast<size_t>(new_pos.QuadPart)};
#else
    return mine::core::Result<size_t>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Result<size_t> File::tell() const noexcept {
    return const_cast<File*>(this)->seek(0, FileSeekOrigin::Current);
}

// ──────────────────────────────────────────────────────────────────────────────
// 大小
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<size_t> File::size() const noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::InvalidState}};
    }

#if defined(_WIN32)
    LARGE_INTEGER li_size;
    if (!::GetFileSizeEx(impl_->handle, &li_size)) {
        DWORD err = ::GetLastError();
        return mine::core::Result<size_t>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }
    return mine::core::Result<size_t>{mine::core::ok_tag,
        static_cast<size_t>(li_size.QuadPart)};
#else
    return mine::core::Result<size_t>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

mine::core::Status File::set_size(size_t new_size) noexcept {
    if (!impl_->is_open_flag) {
        return mine::core::Status{mine::core::StatusCode::InvalidState};
    }

#if defined(_WIN32)
    // 先定位到截断位置
    LARGE_INTEGER li_pos;
    li_pos.QuadPart = static_cast<int64_t>(new_size);
    if (!::SetFilePointerEx(impl_->handle, li_pos, nullptr, FILE_BEGIN)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    // 设置文件末尾
    if (!::SetEndOfFile(impl_->handle)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// 原始句柄
// ──────────────────────────────────────────────────────────────────────────────

void* File::native_handle() const noexcept {
#if defined(_WIN32)
    return impl_->handle;
#else
    return nullptr;
#endif
}

} // namespace mine::io
