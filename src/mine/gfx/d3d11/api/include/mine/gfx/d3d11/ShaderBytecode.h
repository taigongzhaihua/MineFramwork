/**
 * @file ShaderBytecode.h
 * @brief D3D11 预编译着色器字节码公开包装头。
 *
 * 真实的字节码数组由 scripts/build_shaders.ps1 生成到：
 *   build/.generated/mine/gfx/d3d11/ShaderBytecode.h
 *
 * 这里提供一个稳定的公开包含路径：
 *   #include <mine/gfx/d3d11/ShaderBytecode.h>
 *
 * 这样 mine.paint 与 IntelliSense 都无需直接依赖 generated include 目录配置。
 */

#pragma once

#if __has_include("../../../../../../../../../build/.generated/mine/gfx/d3d11/ShaderBytecode.h")
#include "../../../../../../../../../build/.generated/mine/gfx/d3d11/ShaderBytecode.h"
#else
#error "未找到生成的 ShaderBytecode.h，请先构建 mine.gfx.d3d11 或运行 scripts/build_shaders.ps1"
#endif
