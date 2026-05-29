/**
 * @file TomlParser.h
 * @brief TOML 解析器（mine.config 内部使用，不暴露）。
 *
 * 支持的 TOML 子集：
 *   - 注释：# ...
 *   - 节头：[section]、[section.sub]
 *   - 键值对：key = value
 *   - 字符串：基本字符串 "..."（支持转义）和字面字符串 '...'（不转义）
 *   - 整数：123, -42
 *   - 浮点：3.14, -1.5e10
 *   - 布尔：true, false
 *
 * 不支持：
 *   - 数组 [1, 2, 3]
 *   - 内联表 {key = value}
 *   - 多行字符串 """...""" / '''...'''
 *   - 日期/时间类型
 *   - 点分隔的内联键（如 a.b = value 写法；节头形式 [a.b] 支持）
 */

#pragma once

#include <mine/config/ConfigValue.h>
#include <mine/core/Status.h>
#include <mine/core/StringView.h>

namespace mine::config::detail {

// 使用与 JsonParser.h 相同的回调类型（在同一命名空间）
// ParseCallback 已在 JsonParser.h 中定义；若单独包含本文件则重新声明

#ifndef MINE_CONFIG_PARSE_CALLBACK_DEFINED
#define MINE_CONFIG_PARSE_CALLBACK_DEFINED
using ParseCallback = void(*)(void* user_data,
                               mine::core::StringView key,
                               ConfigValue value);
#endif

// ─────────────────────────────────────────────────────────────────────────────
// parse_toml：解析 TOML 文本
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 将 TOML 文本解析为扁平的键值对序列。
 *
 * 节头 [section] 作为键前缀添加到后续键名之前。
 * 遇到不支持的语法时，跳过该行并继续解析。
 *
 * @param toml      完整的 TOML 字符串
 * @param callback  每找到一个键值对时的回调
 * @param user_data 传递给回调的用户数据
 * @return Status::Ok        解析成功（包括部分跳过的情况）
 * @return Status::ParseError TOML 格式严重错误（如非法字符）
 */
[[nodiscard]] mine::core::Status parse_toml(
    mine::core::StringView toml,
    ParseCallback          callback,
    void*                  user_data) noexcept;

} // namespace mine::config::detail
