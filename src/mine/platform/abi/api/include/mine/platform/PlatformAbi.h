/**
 * @file PlatformAbi.h
 * @brief mine.platform.abi 模块伞形头文件。
 *
 * 包含此头文件即可访问 mine.platform.abi 的全部公共接口：
 *   - NativeHandle              — 平台原生窗口句柄
 *   - WindowKind                — 窗口类型枚举
 *   - WindowState               — 窗口显示状态枚举
 *   - WindowEvent               — 窗口事件数据
 *   - IWindowEventSink          — 事件接收器接口
 *   - IWindowEventSource        — 事件分发器接口
 *   - ScreenInfo                — 显示器信息结构体
 *   - IScreenManager            — 多显示器管理接口
 *   - IClipboard                — 系统剪贴板接口
 *   - IMEService                — 输入法服务接口
 *   - WindowDesc                — 窗口创建参数
 *   - IWindow                   — 窗口接口
 *   - IApplicationHost          — 应用程序宿主接口
 */

#pragma once

#include <mine/platform/Api.h>
#include <mine/platform/ModuleTag.h>
#include <mine/platform/NativeHandle.h>
#include <mine/platform/WindowKind.h>
#include <mine/platform/WindowState.h>
#include <mine/platform/WindowEvent.h>
#include <mine/platform/IWindowEventSink.h>
#include <mine/platform/IWindowEventSource.h>
#include <mine/platform/ScreenInfo.h>
#include <mine/platform/IScreenManager.h>
#include <mine/platform/IClipboard.h>
#include <mine/platform/IMEService.h>
#include <mine/platform/WindowDesc.h>
#include <mine/platform/IWindow.h>
#include <mine/platform/IApplicationHost.h>
#include <mine/platform/PlatformHost.h>
