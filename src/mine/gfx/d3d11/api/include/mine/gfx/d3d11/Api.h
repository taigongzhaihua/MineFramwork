/**
 * @file Api.h
 * @brief mine.gfx.d3d11 模块导出宏定义。
 */

#pragma once

#ifdef MINE_GFX_D3D11_BUILD_SHARED
#  ifdef MINE_GFX_D3D11_EXPORTS
#    define MINE_GFX_D3D11_API __declspec(dllexport)
#  else
#    define MINE_GFX_D3D11_API __declspec(dllimport)
#  endif
#else
#  define MINE_GFX_D3D11_API
#endif
