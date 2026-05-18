/**
 * @file D3D11Backend.cpp
 * @brief mine::gfx::create_device() 工厂函数的 D3D11 实现。
 *
 * 通过链接 mine.gfx.d3d11 库，此文件提供 GfxHost.h 中声明的工厂函数实现。
 * 链接时替换模式：调用方只需 #include <mine/gfx/GfxHost.h>，
 * 链接 mine.gfx.d3d11 后工厂函数会自动解析到此实现。
 */

#include <mine/gfx/GfxHost.h>
#include "D3D11Device.h"

#include <mine/core/Memory.h>
#include <iterator> // std::size

namespace mine::gfx {

/**
 * @brief 创建 D3D11 图形设备。
 *
 * @param backend 指定后端类型：
 *   - Backend::D3D11  → 明确请求 D3D11 后端
 *   - Backend::Auto   → 在 Windows 平台自动选择 D3D11
 *   - 其他            → 返回 nullptr（此库不处理）
 *
 * @return 成功返回设备的 OwnedPtr，失败（硬件不支持/系统错误）返回 nullptr
 */
mine::core::OwnedPtr<IDevice> create_device(Backend backend) {
    // 此后端库只处理 D3D11 和 Auto
    if (backend != Backend::D3D11 && backend != Backend::Auto) {
        return nullptr;
    }

    auto* raw = MINE_NEW(d3d11::D3D11DeviceImpl);
    if (!raw->is_valid()) {
        MINE_DELETE(raw);
        return nullptr;
    }

    return mine::core::OwnedPtr<IDevice>(
        raw,
        &mine::core::detail::typed_deleter<d3d11::D3D11DeviceImpl>);
}

} // namespace mine::gfx
