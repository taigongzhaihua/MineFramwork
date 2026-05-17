/**
 * @file ModuleTag.h
 * @brief 提供 mine.core 模块的常量标识。
 */

#pragma once

namespace mine::core {

/**
 * @brief 当前模块的稳定名称常量。
 *
 * 该常量可用于日志、诊断输出、工具链注册和运行时元信息关联，避免在多个
 * 位置重复硬编码模块名字符串。
 */
inline constexpr const char* kModuleName = "mine.core";

} // namespace mine::core