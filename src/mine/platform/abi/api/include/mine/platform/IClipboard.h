/**
 * @file IClipboard.h
 * @brief 系统剪贴板访问接口。
 */

#pragma once

#include <mine/core/StringView.h>
#include <mine/core/Status.h>
#include <cstddef>

namespace mine::platform {

/**
 * @brief 系统剪贴板接口，提供文本读写能力。
 *
 * 实现位于各平台后端（win32 / macos / linux）。
 * 所有方法均为同步调用，应在主线程调用。
 *
 * 返回文本时使用 UTF-8 编码。
 */
class IClipboard {
public:
    virtual ~IClipboard() = default;

    /// 剪贴板当前是否包含纯文本内容
    [[nodiscard]] virtual bool has_text() const = 0;

    /**
     * @brief 将剪贴板文本写入调用方提供的缓冲区（UTF-8）。
     *
     * @param buffer       接收缓冲区（可为 nullptr 以查询所需长度）
     * @param buffer_size  缓冲区字节容量
     * @param out_size     [out] 实际写入或所需的字节数（不含 '\0'）
     * @return 成功返回 Status::ok()；缓冲区不足返回错误状态
     */
    virtual core::Status get_text(
        char*   buffer,
        size_t  buffer_size,
        size_t* out_size) const = 0;

    /**
     * @brief 将 UTF-8 文本写入剪贴板。
     * @param text 待写入的文本视图
     * @return 成功返回 Status::ok()；否则返回错误状态
     */
    virtual core::Status set_text(core::StringView text) = 0;

    /// 清空剪贴板内容
    virtual void clear() = 0;
};

} // namespace mine::platform
