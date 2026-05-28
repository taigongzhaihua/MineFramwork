/**
 * @file DemoWindow.h
 * @brief DemoWindow — Window 组件包装类（手写，模拟 mmlc 生成的 .g.h 文件结构）。
 *
 * 对应 MML 声明（DemoWindow.mml）：
 * @code{.mml}
 * @module app;
 * @using mine.ui.controls.*;
 *
 * component DemoWindow : Window {
 *     window.title:     "MineFramework - 控件交互演示";
 *     window.size:      { width: 800; height: 700; };
 *
 *     signal closeRequested();
 *
 *     Grid { ... }
 * }
 * @endcode
 *
 * @note mine::ui::Window 是 Pimpl 值类型基础设施，不可派生。
 *       因此本类通过聚合 win_（组合模式）实现，而非继承。
 *       与 Slint `inherits Window`、WinUI 3 的生成策略相同。
 *       参见 docs/04-precompiler.md §4.4.2 和 docs/07-windowing.md §7.9。
 */
#pragma once

#include <functional>
#include <cstdio>

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/LayoutAll.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/style/StyleAll.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/math/Color.h>
#include <mine/math/Thickness.h>
#include <mine/paint/Brush.h>
#include <mine/text/FontFace.h>

namespace app {

/**
 * @brief 控件演示窗口（Window 组件包装类）。
 *
 * 真实 mmlc 输出中，本类名为 DemoWindow，生成文件为 DemoWindow.g.h/DemoWindow.g.cpp。
 * 本文件为手写模拟，结构与 mmlc 生成代码完全一致。
 *
 * 析构安全顺序（声明顺序反向）：
 *   win_ 最先析构（swapchain + 原生窗口释放）→ UI 元素随后析构
 */
class DemoWindow {
public:
    /** 构造：仅调用 _configure()（窗口 pending 属性配置），UI 树尚未构建。 */
    DemoWindow();
    ~DemoWindow() = default;

    /** 不可复制，不可移动（持有值类型 UI 元素）。 */
    DemoWindow(const DemoWindow&)            = delete;
    DemoWindow& operator=(const DemoWindow&) = delete;

    // ── Window 生命周期委托 ──────────────────────────────────────────────────

    /** 显示窗口。首次调用触发 win_ 懒初始化，通过 IWindowContext 自动注册为主窗口。 */
    void show()  { win_.show(); }
    /** 隐藏窗口，保持图形资源。 */
    void hide()  { win_.hide(); }
    /** 关闭窗口并释放图形资源。 */
    void close() { win_.close(); }
    /** 窗口是否已关闭（关闭后不得再访问本对象）。 */
    [[nodiscard]] bool is_closed() const noexcept { return win_.is_closed(); }
    /** 访问底层 Window 基础设施（供 Application 内部追踪使用）。 */
    [[nodiscard]] mine::ui::Window& window() noexcept { return win_; }

    // ── 属性（模拟 MINE_PROP；待 mine.reflect 反射宏实现后替换）──────────────

    /** 设置计数状态文本，同时触发重绘。 */
    void set_status_text(const char* text);

    // ── 信号（模拟 MINE_SIGNAL；待信号系统实现后替换为 Signal<> 成员）──────────

    /**
     * @brief 注册"关闭请求"信号的监听器（对应 MML `signal closeRequested()`）。
     *
     * 用户点击"退出"按钮时触发，调用方在 Application::on_startup 中订阅并调用 quit()。
     */
    void set_on_close_requested(std::function<void()> fn) {
        on_close_requested_ = std::move(fn);
    }

    // ── 资源注入（过渡期接口；待 ResourceDictionary 实现后移除）──────────────

    /**
     * @brief 注入字体并完成 UI 树构建（_build / _bind / _states）。
     *
     * @param font 默认系统字体指针（nullptr 允许，控件使用系统回退渲染）
     *
     * @note 在真实 mmlc 生成代码中，字体通过全局 ResourceDictionary 在构造函数
     *       内自动解析，调用方无需显式传递。此接口仅为当前过渡阶段使用。
     * @pre 必须在 show() 之前调用一次。
     */
    void setup(mine::text::FontFace* font);

private:
    // ── 生成方法（对应 mmlc 生成的私有辅助函数）───────────────────────────────

    /** 配置窗口 pending 属性（title / size 等），在构造函数中调用。 */
    void _configure();
    /** 构建完整视觉树，最终调用 win_.set_content()。 */
    void _build(mine::text::FontFace* font);
    /** 安装属性绑定（当前演示为手动刷新模式）。 */
    void _bind();
    /** 安装组件级 VisualStateManager 状态机（当前演示为空）。 */
    void _states();

    // ── 事件处理（静态路由桩，模拟 mmlc 生成的 lambda 展开后形式）─────────────

    /** "计数 +1" 按钮点击处理。 */
    static void s_on_click_count(void* sender,
                                 mine::ui::RoutedEventArgs& args,
                                 void* user_data);
    /** "重  置" 按钮点击处理。 */
    static void s_on_click_reset(void* sender,
                                 mine::ui::RoutedEventArgs& args,
                                 void* user_data);
    /** "退  出" 按钮点击处理（触发 closeRequested 信号）。 */
    static void s_on_click_quit(void*  sender,
                                mine::ui::RoutedEventArgs& args,
                                void*  user_data);

    /** 根据 click_count_ 刷新状态标签文本并触发重绘。 */
    void update_status_();

    // ── 运行时状态 ────────────────────────────────────────────────────────────

    int click_count_ = 0;                      ///< 当前点击计数
    std::function<void()> on_close_requested_; ///< closeRequested 信号监听器

    // ── 数据成员：UI 元素必须声明在 win_ 之前 ────────────────────────────────
    //
    // C++ 析构按声明的逆序进行。将 win_ 声明为最后一个成员，确保：
    //   1. win_ 最先析构 → 渲染循环停止 + swapchain 释放 + 原生窗口关闭
    //   2. UI 元素随后析构 → 无悬空指针、无 UAF 风险
    //
    // mmlc 强制执行此顺序，禁止将 Window 声明在 UI 元素之前。

    // 布局树
    mine::ui::Grid       root_grid_;   ///< 根 Grid（2 行 × 1 列）
    mine::ui::StackPanel body_panel_;  ///< 内容区垂直 StackPanel（Row 1）
    mine::ui::StackPanel btn_row_;     ///< 按钮行水平 StackPanel

    // 标题与说明
    mine::ui::TextBlock  header_text_; ///< 蓝色背景标题栏（Row 0）
    mine::ui::TextBlock  subtitle_;    ///< 交互说明文字

    // 主交互按钮
    mine::ui::Button     btn_count_;   ///< "计数 +1"（蓝色主操作）
    mine::ui::Button     btn_reset_;   ///< "重  置"（灰色辅助）
    mine::ui::Button     btn_quit_;    ///< "退  出"（红色危险操作）

    // 计数状态显示
    mine::ui::TextBlock  status_label_; ///< 当前计数文字

    // Style 演示区
    mine::ui::TextBlock       style_section_; ///< 区域分隔标题
    mine::ui::TextBlock       style_info_;    ///< 演示说明文字
    mine::ui::Button          btn_styled_;    ///< Style 驱动的绿色按钮
    mine::ui::style::Style    demo_style_;    ///< 绿色主题 Style 对象

    // ControlTemplate 演示区
    mine::ui::TextBlock  tmpl_section_; ///< 区域分隔标题
    mine::ui::TextBlock  tmpl_info_;    ///< 演示说明文字
    mine::ui::Button     btn_tmpl_;     ///< 使用自定义模板根的按钮
    mine::ui::TextBlock  tmpl_label_;   ///< 模板根 Border 的子元素（DemoWindow 拥有）

    // ── win_ 最后声明 = 最先析构 ─────────────────────────────────────────────
    mine::ui::Window win_;
};

} // namespace app
