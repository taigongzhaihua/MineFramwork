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
 *   │  count_bind_       BindingExpression：vm_.count_text → count_label_     │
 *   │  hint_bind_        BindingExpression：vm_.hint_text  → hint_label_       │
 *   │                                                                         │
 *   │  用户点击按钮 → s_on_click_* → vm_.xxx_cmd_.execute({})                  │
 *   │                             → ViewModel 更新属性 → 发出通知              │
 *   │                             → BindingExpression 重新求值                │
 *   │                             → TextBlock 更新文字                        │
 *   │                             → render() 重绘窗口                         │
 *   └─────────────────────────────────────────────────────────────────────────┘
 *
 * 成员声明顺序的析构安全保证（逆序析构）：
 *   声明顺序：vm_ → UI 元素 → count_bind_/hint_bind_
 *   析构顺序：hint_bind_ → count_bind_ → UI 元素 → vm_
 *   → 绑定先于 ViewModel 析构，unsubscribe 调用时 ViewModel 仍存活
 */

#pragma once

#include <functional>
#include <cstdio>

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/LayoutAll.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/ui/binding/Binding.h>
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
    // 成员声明顺序：ViewModel 先声明，绑定后声明。
    // 析构时逆序：绑定先析构（unsubscribe），ViewModel 后析构（安全）。
    // ────────────────────────────────────────────────────────────────────────────

    /// ViewModel（先声明，后析构；必须早于绑定对象存活）
    CounterViewModel vm_;

    // ── UI 元素（构成视觉树，由 build_() 组装）──────────────────────────────

    mine::ui::StackPanel body_panel_;       ///< 根垂直排列面板
    mine::ui::TextBlock  header_label_;     ///< 标题栏文字
    mine::ui::TextBlock  count_label_;      ///< 主计数显示（绑定 count_text）
    mine::ui::TextBlock  hint_label_;       ///< 提示说明（绑定 hint_text）
    mine::ui::StackPanel btn_row_;          ///< 按钮行（水平排列）
    mine::ui::Button     btn_inc_;          ///< [+1] 按钮 → vm_.increment_cmd_
    mine::ui::Button     btn_dec_;          ///< [-1] 按钮 → vm_.decrement_cmd_
    mine::ui::Button     btn_reset_;        ///< [重置] 按钮 → vm_.reset_cmd_
    mine::ui::Button     btn_quit_;         ///< [退出] 按钮 → 触发关闭信号

    // ── 绑定表达式（后声明，先析构；保证 unsubscribe 时 vm_ 仍存活）──────────

    /// 绑定 vm_.count_text → count_label_.TextProperty（OneWay）
    mine::ui::BindingExpression count_bind_;
    /// 绑定 vm_.hint_text → hint_label_.TextProperty（OneWay）
    mine::ui::BindingExpression hint_bind_;

    /// 关闭请求信号（App 层注册）
    std::function<void()> on_close_requested_;

    // ── 内部构建方法 ──────────────────────────────────────────────────────────

    /** 构建视觉树（实例化所有控件，设置布局和外观参数）。 */
    void build_(mine::text::FontFace* font);

    /**
     * @brief 激活数据绑定：构造 BindingExpression 并 attach 到目标控件。
     *
     * 两条绑定：
     *   count_bind_: vm_.count_text (INPC "count_text") → count_label_ TextProperty
     *   hint_bind_:  vm_.hint_text  (INPC "hint_text")  → hint_label_ TextProperty
     *
     * attach() 后绑定立即求值一次并更新目标，后续 ViewModel 属性变更时自动触发。
     */
    void bind_();

    // ── 静态事件路由桩（add_handler 注册，调用 ViewModel 命令）────────────────

    /** [+1] 按钮点击：执行 vm_.increment_cmd_，调用 render() 重绘。 */
    static void s_on_click_inc(void* sender, mine::ui::RoutedEventArgs& args,
                                void* user_data);
    /** [-1] 按钮点击：执行 vm_.decrement_cmd_，调用 render() 重绘。 */
    static void s_on_click_dec(void* sender, mine::ui::RoutedEventArgs& args,
                                void* user_data);
    /** [重置] 按钮点击：执行 vm_.reset_cmd_，调用 render() 重绘。 */
    static void s_on_click_reset(void* sender, mine::ui::RoutedEventArgs& args,
                                  void* user_data);
    /** [退出] 按钮点击：触发 on_close_requested_ 信号。 */
    static void s_on_click_quit(void* sender, mine::ui::RoutedEventArgs& args,
                                 void* user_data);
};

} // namespace app
