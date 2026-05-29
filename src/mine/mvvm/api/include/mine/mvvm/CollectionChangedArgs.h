/**
 * @file CollectionChangedArgs.h
 * @brief 集合变更通知参数类型定义。
 *
 * 配合 INotifyCollectionChanged 使用，描述集合发生的具体变更类型和位置。
 */

#pragma once

#include <cstdint>

namespace mine::mvvm {

/**
 * @brief 集合变更动作类型。
 */
enum class CollectionChangedAction : uint8_t {
    Add,     ///< 新增项（在 new_index 位置插入 count 个元素）
    Remove,  ///< 删除项（从 old_index 位置删除 count 个元素）
    Replace, ///< 替换项（old_index == new_index，替换 1 个元素）
    Move,    ///< 移动项（从 old_index 移到 new_index，count 个元素）
    Reset,   ///< 集合重置（Clear()，建议重新读取全部数据）
};

/**
 * @brief 集合变更通知参数。
 *
 * 当 action 为 Reset 时，new_index/old_index/count 均无效，忽略即可。
 */
struct CollectionChangedArgs {
    CollectionChangedAction action{ CollectionChangedAction::Reset };
    int new_index{ -1 };  ///< 新位置索引（Add/Replace/Move 有效）
    int old_index{ -1 };  ///< 旧位置索引（Remove/Replace/Move 有效）
    int count{ 0 };       ///< 受影响的元素数量（通常为 1）
};

} // namespace mine::mvvm
