/**
 * @file JsonParser.h
 * @brief JSON 解析器（mine.config 内部使用，不暴露）。
 *
 * 支持的 JSON 子集：
 *   - null  → ConfigValueType::Null
 *   - true/false → ConfigValueType::Bool
 *   - 整数（无小数点/指数）→ ConfigValueType::Integer
 *   - 浮点（含小数点或指数）→ ConfigValueType::Float
 *   - 字符串 "..." → ConfigValueType::String（支持常见转义序列）
 *   - 对象 {...}  → 递归展平为点分隔键（如 {"a":{"b":1}} → "a.b"=1）
 *   - 数组 [...]  → 跳过（不支持）
 *
 * 不支持：
 *   - \uXXXX Unicode 转义序列
 *   - 数组类型的配置值
 *   - 注释（JSON 标准不支持）
 */

#pragma once

#include <mine/config/ConfigValue.h>
#include <mine/core/Status.h>
#include <mine/core/StringView.h>

namespace mine::config::detail {

// ─────────────────────────────────────────────────────────────────────────────
// 解析回调类型
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 每解析到一个键值对时调用的回调。
 *
 * @param user_data  用户自定义数据指针
 * @param key        完整的点分隔键名（如 "window.width"）
 * @param value      解析得到的 ConfigValue（传入时已完整构造）
 */
using ParseCallback = void(*)(void* user_data,
                               mine::core::StringView key,
                               ConfigValue value);

// ─────────────────────────────────────────────────────────────────────────────
// parse_json：解析 JSON 文本
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief 将 JSON 文本解析为扁平的键值对序列。
 *
 * 嵌套对象会被递归展平为点分隔路径。数组会被跳过。
 *
 * @param json      完整的 JSON 字符串（null 终止，不要求）
 * @param callback  每找到一个键值对时的回调
 * @param user_data 传递给回调的用户数据
 * @return Status::Ok        解析成功
 * @return Status::ParseError JSON 格式错误
 */
[[nodiscard]] mine::core::Status parse_json(
    mine::core::StringView json,
    ParseCallback          callback,
    void*                  user_data) noexcept;

} // namespace mine::config::detail
