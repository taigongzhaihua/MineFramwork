/**
 * @file CounterViewModel.h
 * @brief CounterViewModel —— 计数器演示 ViewModel。
 *
 * 演示 MVVM 模式的核心要素：
 *   - 继承 ViewModelBase（实现 INotifyPropertyChanged）
 *   - MINE_OBSERVABLE 宏声明可观察属性（自动 getter/setter + 变更通知）
 *   - RelayCommand 封装用户操作意图（execute + can_execute + 状态通知）
 *
 * ViewModel 对 View 完全无感知，不引用任何 UI 类型。
 * View 层通过 BindingExpression 订阅 ViewModel 的属性变更，实现数据驱动更新。
 *
 * 绑定流程：
 *   vm_.set_count_text(new_text)                     [ViewModel 侧]
 *     → ObservableObject::raise("count_text")
 *       → INotifyPropertyChanged 订阅者回调
 *         → BindingExpression::re_evaluate()          [绑定层]
 *           → getter() → core::Variant{count_text}
 *             → count_label_.set_value(TextProperty,  [View 侧]
 *                           value, TemplateBind)
 *               → TextBlock::on_text_changed()
 *                 → invalidate_render() → render()
 */

#pragma once

#include <cstdio>

#include <mine/mvvm/Mvvm.h>
#include <mine/containers/InlineString.h>

namespace app {

/**
 * @brief 计数器 ViewModel。
 *
 * 封装计数器的全部业务逻辑，暴露三个可观察属性供 View 绑定：
 *   - count_text：格式化计数字符串，绑定到主显示 TextBlock
 *   - hint_text：状态提示字符串，绑定到提示 TextBlock
 *   - count：原始整数值（通常不直接绑定，由 update_display_() 派生格式化文字）
 *
 * 命令（increment/decrement/reset）由 View 层的按钮点击事件调用，
 * 命令内部修改 count 并刷新显示文字，触发绑定链将新值同步到 UI。
 */
class CounterViewModel : public mine::mvvm::ViewModelBase {
public:
    // ── 可观察属性 ─────────────────────────────────────────────────────────────

    /// 当前计数值（整数，供计算属性和 can_execute 使用）
    MINE_OBSERVABLE(int, count, 0)

    /// 格式化显示文字（绑定到 count_label TextBlock::TextProperty）
    MINE_OBSERVABLE(mine::containers::InlineString, count_text, "当前计数：0")

    /// 操作提示文字（绑定到 hint_label TextBlock::TextProperty）
    MINE_OBSERVABLE(mine::containers::InlineString, hint_text, "点击下方按钮改变计数")

    // ── 命令（move-only，须在构造函数初始化列表中完成初始化）──────────────────

    mine::ui::RelayCommand increment_cmd_;  ///< 计数加一（上限 10）
    mine::ui::RelayCommand decrement_cmd_;  ///< 计数减一（下限 0）
    mine::ui::RelayCommand reset_cmd_;      ///< 重置为零

    /**
     * @brief 构造 CounterViewModel，初始化命令并设置初始显示状态。
     *
     * 命令 lambda 捕获 this 指针。由于 ObservableObject 删除了移动操作，
     * CounterViewModel 实例地址在整个生命周期内固定不变，lambda 中的 this 始终有效。
     */
    CounterViewModel()
        : increment_cmd_{
            /* execute */
            [this](const mine::core::Variant&) noexcept { do_increment_(); },
            /* can_execute */
            [this](const mine::core::Variant&) noexcept -> bool { return count() < 10; }
          }
        , decrement_cmd_{
            [this](const mine::core::Variant&) noexcept { do_decrement_(); },
            [this](const mine::core::Variant&) noexcept -> bool { return count() > 0; }
          }
        , reset_cmd_{
            [this](const mine::core::Variant&) noexcept { do_reset_(); },
            [this](const mine::core::Variant&) noexcept -> bool { return count() != 0; }
          }
    {
        // 根据初始 count=0 设置初始显示文字
        update_display_();
    }

private:
    // ── 命令执行实现 ──────────────────────────────────────────────────────────

    /** 执行加一操作（count < 10 时有效）。 */
    void do_increment_() noexcept {
        set_count(count() + 1);
        update_display_();
    }

    /** 执行减一操作（count > 0 时有效）。 */
    void do_decrement_() noexcept {
        set_count(count() - 1);
        update_display_();
    }

    /** 重置计数为零（count != 0 时有效）。 */
    void do_reset_() noexcept {
        set_count(0);
        update_display_();
    }

    /**
     * @brief 根据当前 count 刷新格式化文字，并通知命令可执行状态改变。
     *
     * 每次 count 改变后调用：
     *   1. 调用 set_count_text() → 触发 "count_text" 属性变更通知
     *      → BindingExpression 重新求值 → TextBlock 刷新
     *   2. 调用 set_hint_text() → 触发 "hint_text" 属性变更通知（同上）
     *   3. 调用 notify_can_execute_changed() → UI 重新查询命令是否可执行
     */
    void update_display_() noexcept {
        // 更新主计数显示文字
        char buf[64];
        std::snprintf(buf, sizeof(buf), "当前计数：%d", count());
        set_count_text(mine::containers::InlineString{buf});

        // 根据边界值提供不同的提示文字
        if (count() == 0) {
            set_hint_text(mine::containers::InlineString{
                "已归零 — 点击 [+1] 开始计数"});
        } else if (count() == 10) {
            set_hint_text(mine::containers::InlineString{
                "已达上限 10 — 点击 [重置] 清零"});
        } else {
            char hint[96];
            std::snprintf(hint, sizeof(hint),
                "有效范围 0-10，当前 %d 次", count());
            set_hint_text(mine::containers::InlineString{hint});
        }

        // 通知命令可执行状态已改变（View 据此启用或禁用按钮）
        increment_cmd_.notify_can_execute_changed();
        decrement_cmd_.notify_can_execute_changed();
        reset_cmd_.notify_can_execute_changed();
    }
};

} // namespace app
