/**
 * @file CustomChromeWindow.h
 * @brief CustomChromeWindow —— 自定义无边框标题栏演示窗口。
 *
 * 演示 WindowChrome 特性：
 *   - IsCustomChrome = true：隐藏系统标题栏，整个窗口区域划为客户区
 *   - CaptionHeight  = 0.0f：不使用系统 HTCAPTION 区域（按钮可正常点击）
 *   - ResizeBorderThickness = 6px：系统自动处理四边/四角 resize 命中测试
 *   - CornerPreference = Round：Windows 11 圆角（旧系统自动忽略）
 *
 * 视觉树结构：
 *   root_grid_（垂直 Grid，2 行）
 *   ├── Row 0 [48px]：title_bar_grid_（水平 Grid，2 列）
 *   │   ├── Col 0 [1*]：drag_region_（TextBlock，拖拽区域）
 *   │   └── Col 1 [Auto]：btn_panel_（水平 StackPanel）
 *   │         ├── btn_minimize_（─）
 *   │         ├── btn_maximize_（□）
 *   │         └── btn_close_（✕）
 *   └── Row 1 [1*]：content_panel_（垂直 StackPanel，演示说明）
 *
 * 拖拽实现：drag_region_ 上的 MouseDownEvent 处理函数在鼠标左键按下时
 * 发送 WM_NCLBUTTONDOWN(HTCAPTION)，触发系统接管拖拽操作。
 */
#pragma once

#include <mine/ui/window/Window.h>
#include <mine/ui/layout/LayoutAll.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/controls/Button.h>
#include <mine/ui/controls/Border.h>
#include <mine/ui/event/RoutedEventArgs.h>
#include <mine/text/FontFace.h>
#include <mine/math/Thickness.h>
#include <mine/paint/Brush.h>

namespace app {

/**
 * @brief 自定义无边框标题栏演示窗口。
 *
 * 继承 mine::ui::Window，不依赖 MVVM/DI，
 * 适合用于演示纯 WindowChrome API 的使用方式。
 */
class CustomChromeWindow final : public mine::ui::Window {
public:
    /** 默认构造：设置窗口属性（标题、尺寸、WindowChrome 参数）。 */
    CustomChromeWindow();

    ~CustomChromeWindow() override;

    /**
     * @brief 注入字体资源并构建视觉树。
     *
     * 在 show() 之前调用，通常在 Application::on_startup() 中执行。
     * @param font 应用默认字体（可为 nullptr，控件降级使用后备字体）
     */
    void setup(mine::text::FontFace* font);

private:
    /** 构建完整视觉树（由 setup() 调用）。 */
    void build_(mine::text::FontFace* font);

    // ── 静态事件处理函数（RoutedEventHandlerFn 函数指针，兼容 add_handler）──

    /** 标题栏拖拽区域鼠标左键按下 → 发送 WM_NCLBUTTONDOWN 触发系统拖拽。 */
    static void s_on_drag_mouse_down(void* sender,
                                      mine::ui::RoutedEventArgs& args,
                                      void* ud);

    /** 最小化按钮点击。 */
    static void s_on_minimize_click(void* sender,
                                     mine::ui::RoutedEventArgs& args,
                                     void* ud);

    /** 最大化/还原按钮点击。 */
    static void s_on_maximize_click(void* sender,
                                     mine::ui::RoutedEventArgs& args,
                                     void* ud);

    /** 关闭按钮点击。 */
    static void s_on_close_click(void* sender,
                                  mine::ui::RoutedEventArgs& args,
                                  void* ud);

    // ── UI 元素（值成员，生命周期与窗口一致）─────────────────────────────────

    // 根布局：垂直 Grid（行 0=标题栏 48px，行 1=内容区 1*）
    mine::ui::Grid root_grid_;

    // 区域背景容器（Grid/StackPanel 无 set_background，通过 Border 包裹实现）
    mine::ui::Border title_bar_border_;  ///< 标题栏背景 #1F1F1F
    mine::ui::Border content_border_;    ///< 内容区背景 #191919

    // 标题栏区域：水平 Grid（列 0=拖拽区 1*，列 1=按钮组 Auto）
    mine::ui::Grid       title_bar_grid_;
    mine::ui::TextBlock  drag_region_;    ///< 可拖拽区域（图标 + 标题文字）
    mine::ui::StackPanel btn_panel_;      ///< 控制按钮容器（水平）
    mine::ui::Button     btn_minimize_;   ///< 最小化按钮 ─
    mine::ui::Button     btn_maximize_;   ///< 最大化/还原按钮 □
    mine::ui::Button     btn_close_;      ///< 关闭按钮 ✕

    // 内容区：垂直 StackPanel + 说明文字
    mine::ui::StackPanel content_panel_;
    mine::ui::TextBlock  desc_title_;     ///< 演示区标题
    mine::ui::TextBlock  desc_lines_[5];  ///< 特性说明条目
};

} // namespace app
