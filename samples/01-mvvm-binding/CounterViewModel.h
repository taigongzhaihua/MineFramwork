/**
 * @file CounterViewModel.h
 * @brief CounterViewModel —— 计数器演示 ViewModel。
 *
 * 演示 MVVM 模式的核心要素：
 *   - 继承 ViewModelBase（实现 INotifyPropertyChanged）
 *   - MINE_OBSERVABLE 宏声明可观察属性（自动 getter/setter + 变更通知
 *     + 属性 getter 自动注册到反射表，供 BindingExpression::bind() 按名读取）
 *   - RelayCommand 封装用户操作意图（execute + can_execute + 状态通知）
 *
 * ViewModel 对 View 完全无感知，不引用任何 UI 类型。
 * View 层通过 BindingExpression::bind() 按属性名建立绑定，实现数据驱动更新。
 *
 * 绑定流程（以 count_text 为例）：
 *   vm_.set_count_text(new_text)                        [ViewModel 侧]
 *     → ObservableObject::raise("count_text")
 *       → INotifyPropertyChanged 订阅者回调
 *         → BindingExpression::re_evaluate()             [绑定层]
 *           → src.get_property("count_text")             ← 属性名反射
 *             → core::Variant{count_text}
 *               → count_label_.set_value(TextProperty,   [View 侧]
 *                             value, TemplateBind)
 *                 → TextBlock::on_text_changed()
 *                   → invalidate_render() → render()
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
    /// 宏自动注册 getter：get_property("count_text") → Variant{count_text()}
    MINE_OBSERVABLE(mine::containers::InlineString, count_text, "当前计数：0")

    /// 操作提示文字（绑定到 hint_label TextBlock::TextProperty）
    /// 宏自动注册 getter：get_property("hint_text") → Variant{hint_text()}
    MINE_OBSERVABLE(mine::containers::InlineString, hint_text, "点击下方按钮改变计数")

    /// 输入文字回显（绑定到 echo_label TextBlock::TextProperty）
    /// 用于展示双向绑定效果：TextBox 输入 → ViewModel → 回显标签
    MINE_OBSERVABLE(mine::containers::InlineString, echo_text, "（输入文字将实时回显在这里）")

private:
    // ── 用户输入文字（手动实现以添加业务逻辑）────────────────────────────────

    /// 用户输入文字字段（TextBox 双向绑定演示）
    mine::containers::InlineString mine_prop_input_text_{};

    /// input_text getter 注册哑元（构造时自动注册到反射表）
    bool mine_reg_input_text_{ (this->register_property_getter(
        "input_text",
        [this]() noexcept -> mine::core::Variant {
            return mine::core::Variant{ mine_prop_input_text_ };
        }), true) };

    /// input_text setter 注册哑元（构造时自动注册到反射表）
    bool mine_reg_setter_input_text_{ (this->register_property_setter(
        "input_text",
        [this](const mine::core::Variant& v) noexcept {
            if (v.has<mine::containers::InlineString>()) {
                this->set_input_text(v.get<mine::containers::InlineString>());
            }
        }), true) };

public:
    /**
     * @brief input_text getter（公开访问）。
     */
    [[nodiscard]] const mine::containers::InlineString& input_text() const noexcept {
        return mine_prop_input_text_;
    }

    /**
     * @brief input_text setter（TwoWay 绑定自动调用，包含业务逻辑）。
     *
     * TwoWay 绑定会自动调用此方法回写 TextBox 输入的新值。
     * ViewModel 内部自动格式化并更新 echo_text，保持业务逻辑封装。
     */
    void set_input_text(mine::containers::InlineString value) noexcept {
        // 调用基类 set() 更新字段并触发 "input_text" 属性变更通知
        this->set<mine::containers::InlineString>(
            mine_prop_input_text_, value, "input_text");

        // ViewModel 内部业务逻辑：格式化回显文字
        char buf[256];
        std::snprintf(buf, sizeof(buf), "实时回显：%.*s",
                     static_cast<int>(value.size()), value.data());
        
        // 更新回显文字属性（触发 "echo_text" 属性变更通知 → View 自动刷新）
        set_echo_text(mine::containers::InlineString{buf});
    }

    // ── 命令（MINE_COMMAND 宏自动注册 getter，支持 set_binding(Button::CommandProperty, "cmd_name")）

    /**
     * @brief 计数加一命令（上限 10）。
     *
     * MINE_COMMAND 展开后：
     *   - 公开字段 `increment_cmd_`（RelayCommand）
     *   - 构造时自动注册 get_property("increment_cmd") → Variant{ICommand*}
     * 视图层通过 btn_inc_.set_binding(Button::CommandProperty, "increment_cmd") 绑定。
     */
    MINE_COMMAND(increment_cmd)

    /// 计数减一命令（下限 0）
    MINE_COMMAND(decrement_cmd)

    /// 重置为零命令
    MINE_COMMAND(reset_cmd)

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
        // 注意：命令 getter 已由 MINE_COMMAND 宏在构造时自动注册，无需手动调用
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
