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
    case PixelFormat::R8_UNorm:           return DXGI_FORMAT_R8_UNORM;
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
 * @brief 获取交换链使用的实际格式。
 *
 * DXGI FLIP_DISCARD 交换链只支持以下格式：
 *   DXGI_FORMAT_B8G8R8A8_UNORM、DXGI_FORMAT_R8G8B8A8_UNORM、
 *   DXGI_FORMAT_R16G16B16A16_FLOAT、DXGI_FORMAT_R10G10B10A2_UNORM
 * 不支持 TYPELESS 格式，因此使用 UNORM（非 sRGB）格式创建交换链，
 * 再用 sRGB 格式创建 RTV（D3D11 允许对 UNORM 资源使用 sRGB 视图）。
 */
inline DXGI_FORMAT swapchain_typeless_format(PixelFormat fmt) noexcept {
    switch (fmt) {
    case PixelFormat::BGRA8_UNorm:
    case PixelFormat::BGRA8_UNorm_sRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;   // FLIP 模式不接受 TYPELESS
    case PixelFormat::RGBA8_UNorm:
    case PixelFormat::RGBA8_UNorm_sRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
        return to_dxgi_format(fmt);
    }
}

/**
 * @brief 获取交换链 RTV 使用的格式。
 *
 * 交换链后缓冲由 DXGI 以 UNORM 格式创建（FLIP 模式只支持非 TYPELESS 格式）。
 * D3D11 标准不保证能对 UNORM 资源创建 UNORM_SRGB 视图（需 D3D11.1 extended cast）。
 * 为保证兼容性，RTV 格式与交换链格式一致，均使用 UNORM，
 * sRGB 伽马校正由显示器硬件/合成器处理，或留给后续渲染管线。
 */
inline DXGI_FORMAT swapchain_rtv_format(PixelFormat fmt) noexcept {
    switch (fmt) {
    case PixelFormat::BGRA8_UNorm:
    case PixelFormat::BGRA8_UNorm_sRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM;   // 与 swapchain_typeless_format 保持一致
    case PixelFormat::RGBA8_UNorm:
    case PixelFormat::RGBA8_UNorm_sRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
        return to_dxgi_format(fmt);
    }
}

} // namespace mine::gfx::d3d11
