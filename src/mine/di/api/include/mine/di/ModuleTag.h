/**
 * @file ModuleTag.h
 * @brief mine.di 模块名称常量标签。
 */

#pragma once

#include <string_view>

namespace mine::di {

/// 模块短名，用于日志、反射注册等场景
inline constexpr std::string_view kModuleName = "mine.di";

} // namespace mine::di
