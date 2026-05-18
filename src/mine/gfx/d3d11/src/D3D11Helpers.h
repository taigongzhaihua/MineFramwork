/**
 * @file D3D11Helpers.h
 * @brief D3D11 后端内部辅助工具（HRESULT 检查、格式转换等）。
 *
 * 此头文件仅供 mine.gfx.d3d11 内部使用，不应出现在公共头文件中。
 */

#pragma once

// Windows SDK 精简引入顺序（必须保持此顺序）
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

#include <mine/gfx/GfxTypes.h>

namespace mine::gfx::d3d11 {

using Microsoft::WRL::ComPtr;

// ── HRESULT 检查宏 ──────────────────────────────────────────────────────────

/**
 * @brief 检查 HRESULT，失败时触发断言并返回 false。
 * @note  仅在内部函数中使用，不暴露给外部调用方。
 */
#define D3D11_CHECK(hr, msg)                                           \
    do {                                                               \
        if (FAILED(hr)) {                                              \
            /* 调试版本输出错误信息 */                                  \
            OutputDebugStringA("[mine.gfx.d3d11] FAILED: " msg "\n"); \
            return false;                                              \
        }                                                              \
    } while (false)

/**
 * @brief 检查 HRESULT，失败时触发断言并返回 nullptr。
 */
#define D3D11_CHECK_NULL(hr, msg)                                      \
    do {                                                               \
        if (FAILED(hr)) {                                              \
            OutputDebugStringA("[mine.gfx.d3d11] FAILED: " msg "\n"); \
            return nullptr;                                            \
        }                                                              \
    } while (false)

// ── 格式转换 ──────────────────────────────────────────────────────────────

/**
 * @brief 将 RHI 像素格式转换为 DXGI 格式。
 * @return 对应的 DXGI_FORMAT，未知格式返回 DXGI_FORMAT_UNKNOWN。
 */
inline DXGI_FORMAT to_dxgi_format(PixelFormat fmt) noexcept {
    switch (fmt) {
    case PixelFormat::RGBA8_UNorm:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case PixelFormat::RGBA8_UNorm_sRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case PixelFormat::BGRA8_UNorm:        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case PixelFormat::BGRA8_UNorm_sRGB:   return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case PixelFormat::R16G16B16A16_Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case PixelFormat::R32G32B32A32_Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case PixelFormat::D24_UNorm_S8_UInt:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case PixelFormat::D32_Float:          return DXGI_FORMAT_D32_FLOAT;
    case PixelFormat::D32_Float_S8_UInt:  return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    default:                              return DXGI_FORMAT_UNKNOWN;
    }
}

/**
 * @brief 将 DXGI 格式转换回 RHI 像素格式。
 */
inline PixelFormat from_dxgi_format(DXGI_FORMAT fmt) noexcept {
    switch (fmt) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:       return PixelFormat::RGBA8_UNorm;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  return PixelFormat::RGBA8_UNorm_sRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:       return PixelFormat::BGRA8_UNorm;
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  return PixelFormat::BGRA8_UNorm_sRGB;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:   return PixelFormat::R16G16B16A16_Float;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:   return PixelFormat::R32G32B32A32_Float;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:    return PixelFormat::D24_UNorm_S8_UInt;
    case DXGI_FORMAT_D32_FLOAT:            return PixelFormat::D32_Float;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return PixelFormat::D32_Float_S8_UInt;
    default:                               return PixelFormat::Unknown;
    }
}

/**
 * @brief 获取交换链格式对应的"无类型"基础格式（用于 CreateRTV 时的 sRGB 正确处理）。
 *
 * DXGI 交换链本身使用无类型格式（如 DXGI_FORMAT_B8G8R8A8_TYPELESS），
 * RTV 则指定 sRGB 格式，以便在交换链呈现时自动 gamma 校正。
 */
inline DXGI_FORMAT swapchain_typeless_format(PixelFormat fmt) noexcept {
    switch (fmt) {
    case PixelFormat::BGRA8_UNorm:
    case PixelFormat::BGRA8_UNorm_sRGB:
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case PixelFormat::RGBA8_UNorm:
    case PixelFormat::RGBA8_UNorm_sRGB:
        return DXGI_FORMAT_R8G8B8A8_TYPELESS;
    default:
        return to_dxgi_format(fmt);
    }
}

/**
 * @brief 获取交换链 RTV 使用的 sRGB 格式（保证输出 gamma 正确）。
 */
inline DXGI_FORMAT swapchain_rtv_format(PixelFormat fmt) noexcept {
    switch (fmt) {
    case PixelFormat::BGRA8_UNorm:
    case PixelFormat::BGRA8_UNorm_sRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case PixelFormat::RGBA8_UNorm:
    case PixelFormat::RGBA8_UNorm_sRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    default:
        return to_dxgi_format(fmt);
    }
}

} // namespace mine::gfx::d3d11
