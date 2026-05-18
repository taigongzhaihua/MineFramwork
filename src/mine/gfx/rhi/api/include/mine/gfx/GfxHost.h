/**
 * @file GfxHost.h
 * @brief 图形设备工厂函数声明（平台/后端无关接口）。
 *
 * 使用方式：链接对应后端库（mine.gfx.d3d11 / mine.gfx.d3d12 / ...），
 * 链接器会将具体实现绑定到此声明的函数符号。
 * 不需要在代码中引用任何后端特定头文件。
 *
 * 示例：
 * @code
 *   #include <mine/gfx/Gfx.h>
 *   auto device = mine::gfx::create_device(); // 自动选择最优后端
 * @endcode
 */

#pragma once

#include "GfxTypes.h"
#include "IDevice.h"
#include <mine/core/Memory.h>

namespace mine::gfx {

/**
 * @brief 创建图形设备。
 *
 * @param backend  后端选择，Auto 表示运行时自动选择最优后端。
 *                 实际可用后端由链接的后端库决定。
 * @return 已初始化的设备；若指定后端不可用或初始化失败则返回 nullptr。
 *
 * @note 函数定义由链接的后端库提供（链接时替换模式）。
 *       链接多个后端时，Auto 将按 D3D12 > D3D11 > Vulkan > Metal > GLES 顺序尝试。
 */
[[nodiscard]] core::OwnedPtr<IDevice> create_device(
    Backend backend = Backend::Auto);

} // namespace mine::gfx
