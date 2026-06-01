/**
 * @file DemoWindow.g.h
 * @brief DemoWindowBase —— 由 mmlc 从 DemoWindow.mml 自动生成，请勿手动修改。
 *
 * 对应 MML 声明（DemoWindow.mml）：
 * @code{.mml}
 * @module app;
 * @using mine.ui.controls.*;
 *
 * component DemoWindow : Window {
 *     window.title: "MineFramework - 控件交互演示";
 *     window.size:  { width: 800; height: 700; };
 *
 *     signal closeRequested();
 *
 *     method on_count_clicked();   // code-behind 必须实现
 *     method on_reset_clicked();   // code-behind 必须实现
 *
 *     Grid {
 *         // ...
 *         TextBlock #id=status_label_ { ... }  // #id → protected 引用
 *     }
 * }
 * @endcode
 *
 * 生成的 DemoWindowBase 继承自 mine::ui::Window（真正的 is-a Window，多态链完整）。
 * 用户在 DemoWindow.h / DemoWindow.cpp 中继承本类实现 code-behind。
 *
 * 析构安全：~DemoWindowBase() 第一句调用 close()，渲染循环停止在所有
 * 数据成员析构之前；基类 ~Window() 析构时已是 no-op。
 *
 * 参见 docs/04-precompiler.md §4.4.2 和 docs/07-windowing.md §7.9。
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
 * @brief 控件演示窗口 Base 类（mmlc 生成）。
 *
 * DemoWindowBase : public mine::ui::Window（真正的 is-a Window）。
 * mmlc 保证析构安全：~DemoWindowBase() 第一句调用 close()。
 *
 * code-behind 类 DemoWindow final : DemoWindowBase 必须实现 method 纯虚函数：
 *   - on_count_clicked()
 *   - on_reset_clicked()
 */
class DemoWindowBase : public mine::ui::Window {
public:
    /** 构造：调用 _configure()（窗口 pending 属性配置）。UI 树在 setup() 中构建。 */
    DemoWindowBase();

    /**
     * @brief 析构：第一句调用 close()，确保渲染循环停止在数据成员析构之前。
     *
     * mmlc 固定生成此析构模式，无需关注数据成员声明顺序。
     */
    ~DemoWindowBase() override;

    /** 不可复制，不可移动（持有值类型 UI 元素）。 */
    DemoWindowBase(const DemoWindowBase&)            = delete;
    DemoWindowBase& operator=(const DemoWindowBase&) = delete;

    // ── 信号（对应 MML `signal closeRequested()`）──────────────────────────────
    // 待 mine.reflect 信号宏实现后替换为 MINE_SIGNAL(closeRequested)

    /**
     * @brief 注册"关闭请求"信号监听器。
     *
     * 调用方在 Application::on_startup 中订阅并调用 quit()。
     * 退出按钮的 click 事件在 _build() 中直接绑定到此信号（MML 声明式绑定）。
     */
    void set_on_close_requested(std::function<void()> fn) {
        on_close_requested_ = std::move(fn);
    }

    // ── method（对应 MML `method xxx()`，code-behind 必须 override）─────────────

    /** "计数 +1" 按钮点击 —— 业务逻辑由 code-behind 实现。 */
    virtual void on_count_clicked() = 0;
    /** "重  置" 按钮点击 —— 业务逻辑由 code-behind 实现。 */
    virtual void on_reset_clicked() = 0;

    // ── 资源注入（过渡期接口；待 ResourceDictionary 实现后移除）──────────────

    /**
     * @brief 注入字体并完成 UI 树构建（_build / _bind / _states）。
     *
     * @param font 默认系统字体（nullptr 允许，控件使用系统回退渲染）
     * @pre 必须在 show() 之前调用一次。
     * @note 真实 mmlc 生成代码中，字体通过全局 ResourceDictionary 在构造函数
     *       内自动解析；此接口仅为当前过渡阶段使用。
     */
    void setup(mine::text::FontFace* font);

protected:
    // ── #id 元素（protected 引用，code-behind 可直接访问）──────────────────────
    //    对应 MML `TextBlock #id=status_label_ { ... }`
    //    等效于 WPF code-behind 中通过 x:Name 访问命名控件

    mine::ui::TextBlock& status_label_ { status_label_s_ };  ///< 当前计数状态标签

    // ── code-behind 辅助接口 ──────────────────────────────────────────────────

    /**
     * @brief 触发 closeRequested 信号（code-behind 可在需要时调用）。
     *
     * 退出按钮的点击处理在 _build() 内直接绑定（声明式），通常无需 code-behind 调用。
     */
    void emit_close_requested() {
        if (on_close_requested_) { on_close_requested_(); }
    }

private:
    // ── 生成方法（对应 mmlc 生成的私有辅助函数）───────────────────────────────

    /** 配置窗口 pending 属性（title / size 等），在构造函数中调用。 */
    void _configure();
    /** 构建完整视觉树，最终调用 set_content()（继承自 Window，无 win_. 前缀）。 */
    void _build(mine::text::FontFace* font);
    /** 安装属性绑定（当前演示为手动刷新模式）。 */
    void _bind();
    /** 安装组件级 VisualStateManager 状态机（当前演示为空）。 */
    void _states();

    // ── 事件处理（静态路由桩，通过虚方法分派到 code-behind）─────────────────────

    /** "计数 +1" 按钮点击 → 调用 on_count_clicked()（多态分派）。 */
    static void s_on_click_count(void* sender, mine::ui::RoutedEventArgs& args, void* user_data);
    /** "重  置" 按钮点击 → 调用 on_reset_clicked()（多态分派）。 */
    static void s_on_click_reset(void* sender, mine::ui::RoutedEventArgs& args, void* user_data);
    /** "退  出" 按钮点击 → 触发 closeRequested 信号（MML 声明式绑定，无需 method）。 */
    static void s_on_click_quit(void* sender, mine::ui::RoutedEventArgs& args, void* user_data);
    /** 切换模板按钮点击 → 在 tmpl_green_ 和 tmpl_orange_ 之间切换。 */
    static void s_on_switch_tmpl(void* sender, mine::ui::RoutedEventArgs& args, void* user_data);
    // ── 信号存储 ─────────────────────────────────────────────────────────────────

    std::function<void()> on_close_requested_;

    // ── 数据成员（私有存储体）────────────────────────────────────────────────────
    //
    // 注意：无 win_ 成员——Window 本身就是 this（继承而非组合）。
    // 析构安全由 ~DemoWindowBase() 首句 close() 保证，无需依赖成员声明顺序。

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

    // #id 元素存储体（通过 protected 引用暴露给 code-behind）
    mine::ui::TextBlock  status_label_s_; ///< 计数状态显示（status_label_ 引用此成员）

    // Style 演示区
    mine::ui::TextBlock       style_section_; ///< 区域分隔标题
    mine::ui::TextBlock       style_info_;    ///< 演示说明文字
    mine::ui::Button          btn_styled_;    ///< Style 驱动的绿色按钮
    mine::ui::style::Style    demo_style_;    ///< 绿色主题 Style 对象

    // ControlTemplate 演示区
    mine::ui::TextBlock              tmpl_section_;      ///< 区域分隔标题
    mine::ui::TextBlock              tmpl_info_;         ///< 演示说明文字
    mine::ui::Button                 btn_tmpl_;          ///< 使用 ControlTemplate DP 的演示按鈕
    mine::ui::Button                 btn_switch_tmpl_;   ///< 切换模板按鈕
    mine::ui::style::ControlTemplate tmpl_green_;        ///< 绿色圆角模板（A）
    mine::ui::style::ControlTemplate tmpl_orange_;       ///< 橙色矩形模板（B）
    mine::ui::style::ControlTemplate tmpl_switch_;       ///< btn_switch_tmpl_ 深灰色模板（与 tmpl_green_/tmpl_orange_ 同路径）
    bool                             tmpl_is_green_ = true; ///< 当前按鈕使用的是哪个模板
};

} // namespace app
