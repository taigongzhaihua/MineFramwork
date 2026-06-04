/**
 * @file CounterWindow.h
 * @brief CounterWindow —— MVVM 计数器演示窗口。
 *
 * 本类在正式工具链实现后对应 mmlc 从 CounterWindow.mml 生成的 .g.h（Base 类）
 * 与手写 code-behind 的合并形态。当前阶段全部手写，以便清晰演示 MVVM 分层：
 *
 *   ┌─────────────────────────────────────────────────────────────────────────┐
 *   │  CounterWindow（View 层）                                               │
 *   │                                                                         │
 *   │  vm_               CounterViewModel（ViewModel 层，值成员）              │
 *   │  DataContext       Window::DataContextProperty 持有 &vm_（Variant），    │
 *   │                   inherits=true 自动向整棵视觉子树传播                   │
 *   │  count_label_      TextBlock，内置 bindings_ 存储绑定到 count_text       │
 *   │  hint_label_       TextBlock，内置 bindings_ 存储绑定到 hint_text        │
 *   │                                                                         │
 *   │  用户点击按钮 → s_on_click_* → vm_.xxx_cmd_.execute({})                  │
 *   │                             → ViewModel 更新属性 → 发出通知              │
 *   │                             → FrameworkElement 内置 binding 重新求值     │
 *   │                             → TextBlock 更新文字                        │
 *   │                             → render() 重绘窗口                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 *
 * 绑定生命周期由 FrameworkElement 内置 bindings_ 存储管理，
 * 无需在 Window 层声明任何 BindingExpression 成员变量。
 */

#pragma once

#include <functional>
#include <cstdio>

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/LayoutAll.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBox.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/text/FontFace.h>
#include <mine/math/Thickness.h>
#include <mine/math/Color.h>
#include <mine/paint/Brush.h>

#include "CounterViewModel.h"

namespace app {

/**
 * @brief MVVM 计数器演示窗口（View 层）。
 *
 * 窗口持有 ViewModel 作为值成员，并通过 BindingExpression 将
 * ViewModel 的可观察属性绑定到 TextBlock 控件的 DependencyProperty。
 *
 * 按钮点击事件通过静态路由桩转发到 ViewModel 的命令（RelayCommand），
 * 不在 View 层编写任何业务逻辑。
 */
class CounterWindow final : public mine::ui::Window {
public:
    CounterWindow();
    ~CounterWindow() override;

    CounterWindow(const CounterWindow&)            = delete;
    CounterWindow& operator=(const CounterWindow&) = delete;

    /**
     * @brief 注入字体并完成 UI 树构建与绑定激活。
     *
     * 须在 show() 前调用一次：
     *   1. build_(font)  — 构建视觉树（控件实例化、布局参数设置）
     *   2. bind_()       — 激活 BindingExpression（订阅 ViewModel 属性变更）
     *
     * @param font 字体对象（可为 nullptr，控件退回内部默认渲染）
     */
    void setup(mine::text::FontFace* font);

    /** 注册关闭请求信号监听器（App 层订阅后调用 quit()）。 */
    void set_on_close_requested(std::function<void()> fn) {
        on_close_requested_ = std::move(fn);
    }

private:
    // ────────────────────────────────────────────────────────────────────────────
    // 成员声明顺序：ViewModel 先声明，UI 元素后声明。
    // 析构时逆序：UI 元素先析构（其内置 bindings_ 自动 detach），ViewModel 后析构。
    // ────────────────────────────────────────────────────────────────────────────

    /// ViewModel（先声明，后析构；元素析构时 ViewModel 仍存活，unsubscribe 安全）
    CounterViewModel vm_;

    // ── UI 元素（构成视觉树，由 build_() 组装）──────────────────────────────

    mine::ui::StackPanel body_panel_;       ///< 根垂直排列面板
    mine::ui::TextBlock  header_label_;     ///< 标题栏文字
    mine::ui::Border     counter_card_;     ///< 计数信息卡片容器
    mine::ui::StackPanel counter_panel_;    ///< 计数卡片内部垂直面板
    mine::ui::TextBlock  count_label_;      ///< 主计数显示（绑定 count_text）
    mine::ui::TextBlock  hint_label_;       ///< 提示说明（绑定 hint_text）

    // ── TextBox 双向绑定演示区 ─────────────────────────────────────────────
    mine::ui::TextBlock  input_prompt_;     ///< TextBox 提示标签
    mine::ui::TextBox    input_box_;        ///< 输入框（双向绑定 input_text）
    mine::ui::TextBlock  echo_label_;       ///< 回显标签（绑定 echo_text）

    mine::ui::StackPanel btn_row_;          ///< 按钮行（水平排列）
    mine::ui::Button     btn_inc_;          ///< [+1] 按钮 → Command 绑定 increment_cmd
    mine::ui::Button     btn_dec_;          ///< [-1] 按钮 → Command 绑定 decrement_cmd
    mine::ui::Button     btn_reset_;        ///< [重置] 按钮 → Command 绑定 reset_cmd
    mine::ui::Button     btn_quit_;         ///< [退出] 按钮 → 触发关闭信号

    /// 关闭请求信号（App 层注册）
    std::function<void()> on_close_requested_;

    // ── 内部构建方法 ──────────────────────────────────────────────────────────

    /** 构建视觉树（实例化所有控件，设置布局和外观参数）。 */
    void build_(mine::text::FontFace* font);

    /**
     * @brief 激活数据绑定（WPF 风格 element.set_binding()，零显式 ViewModel 引用）。
     *
     * 文字绑定等价于 WPF：
     *   count_label_.SetBinding(TextProperty, new Binding("count_text"));
     *
     * Command 绑定等价于 WPF：
     *   btn_inc_.SetBinding(Button.CommandProperty, new Binding("increment_cmd"));
     *
     * 内部自动从控件 DataContext（由 Window::set_data_context() + inherits 机制传播）
     * 解析 INotifyPropertyChanged 指针，按属性名取出 ICommand*，写入 CommandProperty；
     * Button 收到 CommandProperty 变更后自动订阅 can_execute_changed 并刷新 is_enabled_；
     * 用户点击时 Button 内部自动调用 ICommand::execute()，无需任何路由桩代码。
     */
    void bind_();

    // ── 静态事件路由桩（仅保留关闭按钮，业务命令已改为 Command 绑定）────────────

    /** [退出] 按钮点击：触发 on_close_requested_ 信号。 */
    static void s_on_click_quit(void* sender, mine::ui::RoutedEventArgs& args,
                                void* user_data);
};

} // namespace app
