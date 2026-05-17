/**
 * @file Containers.h
 * @brief mine.containers 模块伞形头文件。
 *
 * 包含此头文件即可使用 mine.containers 模块的全部公共 API：
 *   - Vector<T>         — 动态数组
 *   - SmallVector<T, N> — 带内联缓冲区的小对象优化动态数组
 *   - HashMap<K, V>     — Robin Hood 开放寻址哈希映射表
 *   - InlineString      — 带 SSO 的拥有式 UTF-8 字符串
 *   - IntrusiveList<T>  — 侵入式双向链表
 *   - Hash<T>           — 键哈希函数模板（含常见类型特化）
 *   - Equal<T>          — 键相等比较函数模板
 */

#pragma once

#include <mine/containers/Api.h>
#include <mine/containers/Hash.h>
#include <mine/containers/Vector.h>
#include <mine/containers/SmallVector.h>
#include <mine/containers/HashMap.h>
#include <mine/containers/InlineString.h>
#include <mine/containers/IntrusiveList.h>

// ── InlineString 的 Hash 特化 ─────────────────────────────────────────────
// 在 InlineString 与 Hash 都可见后才能定义此特化。

namespace mine::containers {

template<>
struct Hash<InlineString> {
    [[nodiscard]] size_t operator()(const InlineString& s) const noexcept {
        return Hash<mine::core::StringView>{}(
            mine::core::StringView{s.data(), s.size()});
    }
};

} // namespace mine::containers
