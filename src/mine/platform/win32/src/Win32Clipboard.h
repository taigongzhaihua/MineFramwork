/**
 * @file Win32Clipboard.h
 * @brief IClipboard 的 Win32 实现。
 */

#pragma once

#include <mine/platform/IClipboard.h>

namespace mine::platform::win32 {

/**
 * @brief 基于 Win32 OpenClipboard/SetClipboardData API 的剪贴板实现。
 *
 * 所有方法均为同步调用，需在主线程使用。
 */
class Win32Clipboard final : public IClipboard {
public:
    Win32Clipboard() = default;
    ~Win32Clipboard() override = default;

    [[nodiscard]] bool has_text() const override;

    core::Status get_text(
        char* buffer, size_t buffer_size, size_t* out_size) const override;

    core::Status set_text(core::StringView text) override;

    void clear() override;
};

} // namespace mine::platform::win32
