/**
 * @file Win32Clipboard.cpp
 * @brief IClipboard 的 Win32 OpenClipboard/GetClipboardData 实现。
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>

#include "Win32Clipboard.h"
#include "Win32Helpers.h"

#include <mine/core/Status.h>

namespace mine::platform::win32 {

bool Win32Clipboard::has_text() const {
    return IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
}

core::Status Win32Clipboard::get_text(
    char* buffer, size_t buffer_size, size_t* out_size) const {

    if (!has_text()) {
        if (out_size) {
            *out_size = 0;
        }
        return core::Status{};
    }

    if (!OpenClipboard(nullptr)) {
        return core::Status{core::StatusCode::Unknown};
    }

    core::Status result{};
    const HANDLE hdata = GetClipboardData(CF_UNICODETEXT);

    if (!hdata) {
        CloseClipboard();
        return core::Status{core::StatusCode::Unknown};
    }

    const auto* wstr = static_cast<const wchar_t*>(GlobalLock(hdata));
    if (!wstr) {
        CloseClipboard();
        return core::Status{core::StatusCode::Unknown};
    }

    // 将 UTF-16 转换为 UTF-8
    const std::string utf8 = utf16_to_utf8(wstr);
    GlobalUnlock(hdata);
    CloseClipboard();

    if (out_size) {
        *out_size = utf8.size();
    }

    if (buffer && buffer_size > 0) {
        // 拷贝到调用方缓冲区（不超出 buffer_size，确保 null 终止）
        const size_t copy_len = (utf8.size() < buffer_size - 1)
                                ? utf8.size()
                                : buffer_size - 1;
        if (copy_len > 0) {
            // 使用 memcpy 安全拷贝（UTF-8 字节序列）
            memcpy(buffer, utf8.data(), copy_len);
        }
        buffer[copy_len] = '\0';

        if (utf8.size() >= buffer_size) {
            // 缓冲区不足，但已尽量拷贝
            result = core::Status{core::StatusCode::OutOfRange};
        }
    }

    return result;
}

core::Status Win32Clipboard::set_text(core::StringView text) {
    const std::wstring wstr = utf8_to_utf16(text);

    if (!OpenClipboard(nullptr)) {
        return core::Status{core::StatusCode::Unknown};
    }

    EmptyClipboard();

    // 分配全局内存（包含 null 终止符）
    const size_t byte_size = (wstr.size() + 1) * sizeof(wchar_t);
    HGLOBAL hglobal = GlobalAlloc(GMEM_MOVEABLE, byte_size);
    if (!hglobal) {
        CloseClipboard();
        return core::Status{core::StatusCode::OutOfMemory};
    }

    auto* dest = static_cast<wchar_t*>(GlobalLock(hglobal));
    if (!dest) {
        GlobalFree(hglobal);
        CloseClipboard();
        return core::Status{core::StatusCode::Unknown};
    }

    // 安全拷贝（包含 null 终止符）
    memcpy(dest, wstr.data(), (wstr.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(hglobal);

    if (!SetClipboardData(CF_UNICODETEXT, hglobal)) {
        GlobalFree(hglobal);
        CloseClipboard();
        return core::Status{core::StatusCode::Unknown};
    }

    CloseClipboard();
    // 注意：SetClipboardData 成功后系统接管 hglobal，不能再 GlobalFree
    return core::Status{};
}

void Win32Clipboard::clear() {
    if (OpenClipboard(nullptr)) {
        EmptyClipboard();
        CloseClipboard();
    }
}

} // namespace mine::platform::win32
