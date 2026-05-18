/**
 * @file Gfx.h
 * @brief mine.gfx.rhi 模块伞形头文件。
 *
 * 包含此头文件即可引入完整的 RHI 抽象接口。
 * 具体实现由链接的后端库（mine.gfx.d3d11 / mine.gfx.d3d12 / ...）提供。
 */

#pragma once

#include "Api.h"
#include "GfxTypes.h"
#include "IBuffer.h"
#include "ICommandList.h"
#include "IDevice.h"
#include "IFence.h"
#include "IPipeline.h"
#include "IQueue.h"
#include "ISwapchain.h"
#include "ITexture.h"
#include "GfxHost.h"
