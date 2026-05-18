/**
 * @file IBuffer.h
 * @brief GPU 缓冲资源接口（顶点/索引/常量/结构化缓冲）。
 */

#pragma once

#include "GfxTypes.h"

namespace mine::gfx {

/**
 * @brief GPU 缓冲资源抽象接口。
 *
 * 通过 IDevice::create_buffer() 创建，由 OwnedPtr<IBuffer> 管理生命周期。
 */
class IBuffer {
public:
    virtual ~IBuffer() = default;

    /// 返回创建时的描述符（只读）
    [[nodiscard]] virtual const BufferDesc& desc() const noexcept = 0;

    /// 缓冲字节大小
    [[nodiscard]] uint64_t size() const noexcept { return desc().size; }
};

} // namespace mine::gfx
