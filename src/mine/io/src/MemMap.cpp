/**
 * @file MemMap.cpp
 * @brief mine::io::MemMap Win32 实现。
 *
 * 使用 CreateFileMappingW + MapViewOfFile 实现内存映射文件。
 */

#include <mine/io/MemMap.h>
#include <mine/io/File.h>

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
#endif

namespace mine::io {

// ──────────────────────────────────────────────────────────────────────────────
// UTF-8 → UTF-16 转换（Windows 专用）
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

mine::core::StatusCode win32_error_to_status(DWORD error) noexcept {
    switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            return mine::core::StatusCode::NotFound;
        case ERROR_ACCESS_DENIED:
            return mine::core::StatusCode::PermissionDenied;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            return mine::core::StatusCode::OutOfMemory;
        case ERROR_INVALID_PARAMETER:
            return mine::core::StatusCode::InvalidArg;
        case ERROR_DISK_FULL:
            return mine::core::StatusCode::IoError;
        default:
            return mine::core::StatusCode::Unknown;
    }
}

} // namespace

#endif // _WIN32

// ──────────────────────────────────────────────────────────────────────────────
// MemMap::Impl
// ──────────────────────────────────────────────────────────────────────────────

struct MemMap::Impl {
#if defined(_WIN32)
    HANDLE file_handle   = INVALID_HANDLE_VALUE;  ///< 底层文件句柄
    HANDLE mapping_handle = nullptr;               ///< 文件映射对象句柄
    void*  mapped_view   = nullptr;               ///< 映射视图指针
    uint64_t mapped_size = 0;                      ///< 映射大小
#else
    int    fd            = -1;                     ///< POSIX 文件描述符
    void*  mapped_view   = nullptr;
    uint64_t mapped_size = 0;
#endif
    bool   is_mapped_flag = false;                 ///< 是否已映射

    mine::core::Status close() noexcept {
#if defined(_WIN32)
        if (mapped_view) {
            ::UnmapViewOfFile(mapped_view);
            mapped_view = nullptr;
        }
        if (mapping_handle) {
            ::CloseHandle(mapping_handle);
            mapping_handle = nullptr;
        }
        if (file_handle != INVALID_HANDLE_VALUE) {
            ::CloseHandle(file_handle);
            file_handle = INVALID_HANDLE_VALUE;
        }
#endif
        mapped_size = 0;
        is_mapped_flag = false;
        return mine::core::Status::success();
    }
};

// ──────────────────────────────────────────────────────────────────────────────
// 构造/析构
// ──────────────────────────────────────────────────────────────────────────────

MemMap::MemMap() noexcept : impl_{new Impl{}} {}

MemMap::MemMap(MemMap&& other) noexcept
    : impl_{other.impl_} {
    other.impl_ = new Impl{};
}

MemMap& MemMap::operator=(MemMap&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = new Impl{};
    }
    return *this;
}

MemMap::~MemMap() noexcept {
    if (impl_->is_mapped_flag) {
        (void)impl_->close();  // 析构中忽略关闭失败
    }
    delete impl_;
}

// ──────────────────────────────────────────────────────────────────────────────
// 内部构造函数（须在工厂方法之前定义）
// ──────────────────────────────────────────────────────────────────────────────

MemMap::MemMap(Impl* impl) noexcept : impl_{impl} {}

// ──────────────────────────────────────────────────────────────────────────────
// 工厂方法
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Result<MemMap>
MemMap::open_read(const Path& path) noexcept {
    return open_with_options(path, MemMapMode::Read, 0, 0);
}

mine::core::Result<MemMap>
MemMap::open_read_write(const Path& path) noexcept {
    return open_with_options(path, MemMapMode::ReadWrite, 0, 0);
}

mine::core::Result<MemMap>
MemMap::open_with_options(const Path& path, MemMapMode mode,
                          uint64_t offset, uint64_t length) noexcept {
#if defined(_WIN32)
    auto path_str = path.string();
    std::wstring wide_path = to_wide(path_str.data(), path_str.size());

    // 转换访问模式
    DWORD desired_access = 0;
    DWORD fl_protect = 0;
    DWORD map_access = 0;

    bool is_read = (static_cast<uint32_t>(mode) & static_cast<uint32_t>(MemMapMode::Read)) != 0;
    bool is_write = (static_cast<uint32_t>(mode) & static_cast<uint32_t>(MemMapMode::Write)) != 0;
    bool is_copy = (static_cast<uint32_t>(mode) & static_cast<uint32_t>(MemMapMode::CopyOnWrite)) != 0;

    if (is_read && is_write) {
        desired_access = GENERIC_READ | GENERIC_WRITE;
        fl_protect = PAGE_READWRITE;
        map_access = FILE_MAP_WRITE;
    } else if (is_write) {
        desired_access = GENERIC_READ | GENERIC_WRITE;
        fl_protect = PAGE_READWRITE;
        map_access = FILE_MAP_WRITE;
    } else if (is_copy) {
        desired_access = GENERIC_READ | GENERIC_WRITE;
        fl_protect = PAGE_WRITECOPY;
        map_access = FILE_MAP_COPY;
    } else {
        // 默认只读
        desired_access = GENERIC_READ;
        fl_protect = PAGE_READONLY;
        map_access = FILE_MAP_READ;
    }

    // 打开文件
    HANDLE hFile = ::CreateFileW(
        wide_path.c_str(),
        desired_access,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        DWORD err = ::GetLastError();
        return mine::core::Result<MemMap>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    // 获取文件大小
    LARGE_INTEGER li_size;
    if (!::GetFileSizeEx(hFile, &li_size)) {
        DWORD err = ::GetLastError();
        ::CloseHandle(hFile);
        return mine::core::Result<MemMap>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    uint64_t file_size = static_cast<uint64_t>(li_size.QuadPart);
    if (length == 0) {
        length = file_size - offset;
    }

    // 边界检查
    if (offset + length > file_size) {
        ::CloseHandle(hFile);
        return mine::core::Result<MemMap>{mine::core::err_tag,
            mine::core::Status{mine::core::StatusCode::OutOfRange}};
    }

    // 创建文件映射
    // 若 length == 0 且文件为空，需特殊处理
    uint64_t mapping_size = (length > 0) ? (offset + length) : 0;
    DWORD size_high = static_cast<DWORD>(mapping_size >> 32);
    DWORD size_low  = static_cast<DWORD>(mapping_size & 0xFFFFFFFF);

    HANDLE hMapping = ::CreateFileMappingW(
        hFile,
        nullptr,
        fl_protect,
        size_high,
        size_low,
        nullptr
    );

    if (!hMapping) {
        DWORD err = ::GetLastError();
        ::CloseHandle(hFile);
        return mine::core::Result<MemMap>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    // 映射视图
    DWORD offset_high = static_cast<DWORD>(offset >> 32);
    DWORD offset_low  = static_cast<DWORD>(offset & 0xFFFFFFFF);

    void* view = ::MapViewOfFile(
        hMapping,
        map_access,
        offset_high,
        offset_low,
        static_cast<SIZE_T>(length)
    );

    if (!view) {
        DWORD err = ::GetLastError();
        ::CloseHandle(hMapping);
        ::CloseHandle(hFile);
        return mine::core::Result<MemMap>{mine::core::err_tag,
            mine::core::Status{win32_error_to_status(err)}};
    }

    auto* impl = new Impl{};
    impl->file_handle = hFile;
    impl->mapping_handle = hMapping;
    impl->mapped_view = view;
    impl->mapped_size = length;
    impl->is_mapped_flag = true;

    return mine::core::Result<MemMap>{mine::core::ok_tag, MemMap{impl}};
#else
    return mine::core::Result<MemMap>{mine::core::err_tag,
        mine::core::Status{mine::core::StatusCode::NotSupported}};
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// 查询
// ──────────────────────────────────────────────────────────────────────────────

bool MemMap::is_mapped() const noexcept {
    return impl_->is_mapped_flag;
}

void* MemMap::data() noexcept {
    return impl_->mapped_view;
}

const void* MemMap::data() const noexcept {
    return impl_->mapped_view;
}

uint64_t MemMap::size() const noexcept {
    return impl_->mapped_size;
}

mine::core::Span<char> MemMap::span() noexcept {
    return {static_cast<char*>(impl_->mapped_view), static_cast<size_t>(impl_->mapped_size)};
}

mine::core::Span<const char> MemMap::span() const noexcept {
    return {static_cast<const char*>(impl_->mapped_view),
            static_cast<size_t>(impl_->mapped_size)};
}

mine::core::Span<uint8_t> MemMap::byte_span() noexcept {
    return {static_cast<uint8_t*>(impl_->mapped_view), static_cast<size_t>(impl_->mapped_size)};
}

mine::core::Span<const uint8_t> MemMap::byte_span() const noexcept {
    return {static_cast<const uint8_t*>(impl_->mapped_view),
            static_cast<size_t>(impl_->mapped_size)};
}

// ──────────────────────────────────────────────────────────────────────────────
// 同步
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Status MemMap::flush(uint64_t offset, uint64_t length) noexcept {
    if (!impl_->is_mapped_flag) {
        return mine::core::Status{mine::core::StatusCode::InvalidState};
    }

#if defined(_WIN32)
    if (length == 0 && impl_->mapped_size > offset) {
        length = impl_->mapped_size - offset;
    }

    if (offset + length > impl_->mapped_size) {
        return mine::core::Status{mine::core::StatusCode::OutOfRange};
    }

    uint8_t* base = static_cast<uint8_t*>(impl_->mapped_view) + offset;
    SIZE_T size = static_cast<SIZE_T>(length);

    if (!::FlushViewOfFile(base, size)) {
        DWORD err = ::GetLastError();
        return mine::core::Status{win32_error_to_status(err)};
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

mine::core::Status MemMap::flush_sync(uint64_t offset, uint64_t length) noexcept {
    if (!impl_->is_mapped_flag) {
        return mine::core::Status{mine::core::StatusCode::InvalidState};
    }

#if defined(_WIN32)
    // FlushViewOfFile 在返回前完成所有 I/O（等同于同步）
    auto status = flush(offset, length);
    if (!status.ok()) return status;

    // 可选：同时强制写回文件元数据
    if (impl_->file_handle != INVALID_HANDLE_VALUE) {
        ::FlushFileBuffers(impl_->file_handle);
    }
    return mine::core::Status::success();
#else
    return mine::core::Status{mine::core::StatusCode::NotSupported};
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// 关闭
// ──────────────────────────────────────────────────────────────────────────────

mine::core::Status MemMap::close() noexcept {
    return impl_->close();
}

} // namespace mine::io
