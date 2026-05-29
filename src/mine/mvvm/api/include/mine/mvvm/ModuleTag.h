/**
 * @file ModuleTag.h
 * @brief mine.mvvm 模块名称常量标签。
 */

#pragma once

#include <string_view>

namespace mine::mvvm {

/// 模块短名，用于日志、反射注册等场景
inline constexpr std::string_view kModuleName = "mine.mvvm";

} // namespace mine::mvvm
