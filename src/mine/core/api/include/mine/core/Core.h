/**
 * @file Core.h
 * @brief mine.core 模块的一站式伞形头文件。
 *
 * 包含此头文件即引入 mine.core 的所有公共类型：
 *   - 断言与诊断宏（Assert.h）
 *   - 操作状态（Status.h、Result.h）
 *   - 字符串与内存视图（StringView.h、Span.h）
 *   - 类型标识（TypeId.h）
 *   - 内存管理（Allocator.h、Memory.h、Pimpl.h）
 *   - 类型擦除容器（Variant.h）
 *
 * 如需精细控制编译时间，可只包含具体子头文件。
 */

#pragma once

#include <mine/core/Api.h>
#include <mine/core/ModuleTag.h>
#include <mine/core/Assert.h>
#include <mine/core/Status.h>
#include <mine/core/Result.h>
#include <mine/core/StringView.h>
#include <mine/core/Span.h>
#include <mine/core/TypeId.h>
#include <mine/core/Allocator.h>
#include <mine/core/Memory.h>
#include <mine/core/Pimpl.h>
#include <mine/core/Variant.h>
