/**
 * @file Window.h
 * @brief UI 窗口类 —— 将视觉树与平台窗口及渲染管线桥接。
 *
 * Window 是 mine.ui.window 模块的核心类，职责：
 *   1. 持有并转发 platform::IWindow 的生命周期事件（Resized / DpiChanged / Closed）。
 *   2. 在 Resized / DpiChanged 时驱动两遍布局（Measure + Arrange）并重新渲染。
 *   3. 持有对渲染基础设施的引用（IDevice / IQueue / IRenderer），
 *      自行创建并管理 ISwapchain。
 *   4. 在内容根变化或渲染脏时通过 render() 将视觉树绘制到交换链后缓冲并 Present。
 *
 * 构造方式（两种路径）：
 *   A. 无参构造（推荐）：Window 以 pending 状态创建，
 *      通过 set_title/set_size 等属性配置，调用 show() 时懒初始化：
 *      Application 的 IWindowContext 会自动创建原生窗口并绑定图形资源。
 *      适合将 Window 作为应用类的值类型成员直接声明。
 *
 *   B. 带参构造（框架内部用）：Application::create_window(WindowDesc) 内部使用，
 *      立即绑定已创建的原生窗口和图形设备，一步完成初始化。
 *
 * 所有权规则：
 *   - 路径 A（pending/show）：Window 自身持有 platform::IWindow 的所有权（OwnedPtr）。
 *   - 路径 B（create_window）：Application 持有 platform::IWindow，Window 仅持有指针。
 *   - device / queue / renderer 始终由 Application 管理生命周期，Window 仅持有指针。
 *   - content（UIElement*）由应用层管理，Window 不拥有其生命周期。
 *   - ISwapchain 由 Window 自行创建并独占持有（OwnedPtr）。
 *
 * 线程安全：所有方法须在 UI 线程（创建本对象的线程）调用。
 *
 * 依赖：mine.platform.abi / mine.gfx.rhi / mine.paint / mine.ui.visual / mine.ui.layout
 */

#pragma once

#include <mine/ui/window/Api.h>
#include <mine/ui/property/DependencyObject.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/core/StringView.h>
#include <mine/core/Pimpl.h>
#include <mine/core/Variant.h>
#include <mine/math/Size.h>
#include <mine/math/Rect.h>

#include <functional>

// 前向声明，避免将大型头文件拉入公共接口
namespace mine::platform   { class IWindow; enum class WindowKind : int; enum class WindowCornerPreference : int; }
namespace mine::gfx        { class IDevice; class IQueue; }
namespace mine::paint       { class IRenderer; }
namespace mine::ui          { class UIElement; }
namespace mine::ui::input   { class InputRouter; }

namespace mine::ui {

/**
 * @brief UI 窗口。
 *
 * 推荐用法（路径 A，无参构造 + show() 懒初始化）：
 * @code
 *   struct MyApp : public mine::ui::app::Application {
 *       ui::Window win_;  // 值成员，无参构造（pending 状态）
 *
 *       void on_startup(int, char**) override {
 *           win_.set_title("我的窗口");
 *           win_.set_size({1024.0f, 768.0f});
 *           win_.set_content(&root);
 *           win_.show();  // 首次 show 时自动创建原生窗口并绑定图形资源
 *       }
 *   };
 * @endcode
 *
 * 框架内部用法（路径 B，create_window(WindowDesc)）：
 * @code
 *   auto* win = application->create_window(desc);  // 立即初始化
 * @endcode
 */
class MINE_UI_WINDOW_API Window : public DependencyObject {
public:
    // ── 依赖属性 ─────────────────────────────────────────────────────────────

    /**
     * @brief 窗口数据上下文属性（Variant 存储 ViewModel 指针或值）。
     *
     * MVVM 绑定时，绑定表达式从此属性解析 ViewModel。
     * inherits = true：属性变更时通过视觉子树自动向下传播（Inherited 优先级），
     * 子控件无需手动设置 DataContext，即可自动继承窗口级 ViewModel。
     * 默认值为空 Variant（无数据上下文）。
     */
    static const DependencyProperty& DataContextProperty;

    /**
     * @brief 窗口内边距属性（四边各自独立，逻辑像素）。
     *
     * 控制内容根相对于窗口客户区的内边距。布局时内容根的 measure / arrange
     * 均在 client_rect.deflated(Padding) 的区域内进行：
     *
     *   client_rect (0,0,W,H)
     *   └─ padding.left/top/right/bottom 向内收缩
     *       └─ content_rect → 传递给 content_->measure() + arrange()
     *
     * 默认值为 Thickness{}（四边均为 0，无内边距）。
     * 变更时自动触发重新布局与渲染（与窗口尺寸变化行为一致）。
     */
    static const DependencyProperty& PaddingProperty;

    // ── 自定义 Chrome 属性（窗口 NC 区域控制，类似 WPF WindowChrome）────────────

    /**
     * @brief 是否启用自定义 Chrome（bool，默认 false）。
     *
     * true 时隐藏系统标题栏/边框，将整个窗口区域（含原 NC 区域）划归客户区，
     * 由应用自行绘制标题栏内容。同时接管 WM_NCCALCSIZE / WM_NCHITTEST 处理。
     * false 时恢复系统默认标题栏行为。
     *
     * @note 仅对 WindowKind::Normal（WS_OVERLAPPEDWINDOW 风格）有意义；
     *       Popup / Splash 等无边框窗口无需此属性。
     */
    static const DependencyProperty& IsCustomChromeProperty;

    /**
     * @brief 可拖拽标题栏区域高度（float，逻辑像素，默认 32.0f）。
     *
     * IsCustomChrome = true 时，窗口顶部该高度范围内的区域被识别为 HTCAPTION，
     * 用户可在此处拖拽移动窗口、双击最大化/还原。
     * 0 表示不提供可拖拽区域，应用须自行调用系统拖拽 API（如 ReleaseCapture + WM_NCLBUTTONDOWN）。
     */
    static const DependencyProperty& CaptionHeightProperty;

    /**
     * @brief 可调整大小的边框厚度（Thickness，逻辑像素，默认四边 4.0f）。
     *
     * IsCustomChrome = true 时，窗口四边内侧该厚度范围内的区域被识别为 resize 区域
     * （HTLEFT / HTTOP / HTRIGHT / HTBOTTOM 及四角）。
     * 当窗口最大化时此设置自动失效。
     */
    static const DependencyProperty& ResizeBorderThicknessProperty;

    /**
     * @brief 窗口圆角首选项（WindowCornerPreference 枚举值 int，默认 SystemDefault）。
     *
     * 由平台层映射到对应系统 API（Windows 11: DWM_WINDOW_CORNER_PREFERENCE）。
     * 在不支持圆角首选项的旧系统（Windows 10 及以下）上此属性被忽略。
     */
    static const DependencyProperty& CornerPreferenceProperty;

    /**
     * @brief DWM 玻璃帧延伸厚度（Thickness，逻辑像素，默认全零）。
     *
     * 将 DWM 毛玻璃半透明帧延伸到客户区对应厚度的范围内。
     *   - 全为 0（默认）：不延伸，客户区无玻璃效果
     *   - 正值：向客户区延伸对应宽度
     *   - 任一边 < 0（如 Thickness::uniform(-1.0f)）：延伸覆盖整个客户区（全窗口玻璃）
     *
     * 仅在 DWM 组合开启时生效（Windows 8 以上默认开启）。
     */
    static const DependencyProperty& GlassFrameThicknessProperty;

    /**
     * @brief 无参构造（pending 状态）。
     *
     * 窗口以 pending 状态创建，尚未关联原生窗口和图形资源。
     * 可在 show() 之前通过 set_title / set_size / set_resizable 等属性配置窗口参数。
     * 首次调用 show() 时，通过 IWindowContext 完成懒初始化：
     *   - 创建平台原生窗口（platform::IWindow）
     *   - 创建 ISwapchain 并订阅窗口事件
     *   - 安装 tick_and_render 回调（由 Application 实现的 IWindowContext 负责）
     *   - 若尚无主窗口，自动将本窗口设为主窗口
     *
     * @pre Application::run() 已被调用（即在 on_startup() 回调内调用 show()）
     */
    Window();

    /**
     * @brief 带参构造（立即初始化），框架内部路径（Application::create_window 使用）。
     *
     * @param native_window 平台窗口，生命周期由外部（Application）管理
     * @param device        图形设备，用于创建交换链，生命周期由外部管理
     * @param queue         图形命令队列，用于 resize 前 wait_idle，生命周期由外部管理
     * @param renderer      2D 渲染器，用于提交 Canvas DrawCall，生命周期由外部管理
     */
    Window(platform::IWindow& native_window,
           gfx::IDevice&      device,
           gfx::IQueue&       queue,
           paint::IRenderer&  renderer);

    /**
     * @brief 虚析构（支持 mmlc 生成的 XxxBase : public Window 继承链）。
     *
     * DemoWindowBase 等生成类的析构体第一句调用 close()，基类析构时已是 no-op。
     */
    virtual ~Window();

    // ── 数据上下文接口 ────────────────────────────────────────────────────────

    /**
     * @brief 设置窗口数据上下文（以 Local 优先级写入 DataContextProperty）。
     *
     * 若当前已有内容根（content() != nullptr），DataContext 变更回调会立即
     * 以 Inherited 优先级将新值写入内容根，视觉子树的 inherits=true 传播
     * 机制随后将其向下推送到整棵子树。
     *
     * @param ctx 上下文值（通常是 INotifyPropertyChanged* 包装为 Variant<void*>）
     */
    void set_data_context(const core::Variant& ctx);

    /**
     * @brief 返回当前数据上下文（DataContextProperty 的生效值）。
     */
    [[nodiscard]] const core::Variant& data_context() const noexcept;

    // ── 内边距接口 ────────────────────────────────────────────────────────────

    /**
     * @brief 设置窗口内边距（以 Local 优先级写入 PaddingProperty）。
     *
     * 窗口已初始化时立即触发重新布局与渲染；
     * pending 状态下写入 DP 存储，首次 show() 后布局时自动生效。
     *
     * @param padding 四边内边距（left/top/right/bottom，逻辑像素，通常 >= 0）
     */
    void set_padding(const math::Thickness& padding);

    /**
     * @brief 返回当前内边距（PaddingProperty 的生效值）。
     */
    [[nodiscard]] math::Thickness padding() const noexcept;

    // ── 自定义 Chrome 访问器 ─────────────────────────────────────────────────

    /**
     * @brief 启用或禁用自定义 Chrome（以 Local 优先级写入 IsCustomChromeProperty）。
     *
     * 变更立即生效：若窗口已初始化，平台层将实时调整 NC 区域处理方式。
     *
     * @param enabled true 启用自定义标题栏，false 恢复系统默认标题栏
     */
    void set_custom_chrome(bool enabled);

    /**
     * @brief 返回是否启用自定义 Chrome（IsCustomChromeProperty 的生效值）。
     */
    [[nodiscard]] bool is_custom_chrome() const noexcept;

    /**
     * @brief 设置可拖拽标题栏区域高度（以 Local 优先级写入 CaptionHeightProperty）。
     *
     * @param height 逻辑像素高度，通常为 30 ~ 48，默认 32
     */
    void set_caption_height(float height);

    /**
     * @brief 返回当前标题栏可拖拽区域高度（CaptionHeightProperty 的生效值）。
     */
    [[nodiscard]] float caption_height() const noexcept;

    /**
     * @brief 设置可调整大小的边框厚度（以 Local 优先级写入 ResizeBorderThicknessProperty）。
     *
     * @param thickness 各边厚度（逻辑像素），通常为 4 ~ 8
     */
    void set_resize_border_thickness(const math::Thickness& thickness);

    /**
     * @brief 返回当前可调整大小边框厚度（ResizeBorderThicknessProperty 的生效值）。
     */
    [[nodiscard]] math::Thickness resize_border_thickness() const noexcept;

    /**
     * @brief 设置窗口圆角首选项（以 Local 优先级写入 CornerPreferenceProperty）。
     *
     * @param pref 圆角首选项（SystemDefault / DoNotRound / Round / RoundSmall）
     */
    void set_corner_preference(platform::WindowCornerPreference pref);

    /**
     * @brief 返回当前圆角首选项（CornerPreferenceProperty 的生效值）。
     */
    [[nodiscard]] platform::WindowCornerPreference corner_preference() const noexcept;

    /**
     * @brief 设置 DWM 玻璃帧延伸厚度（以 Local 优先级写入 GlassFrameThicknessProperty）。
     *
     * @param thickness 各边延伸厚度（逻辑像素）；任一边 < 0 则为全窗口玻璃模式
     */
    void set_glass_frame_thickness(const math::Thickness& thickness);

    /**
     * @brief 返回当前 DWM 玻璃帧延伸厚度（GlassFrameThicknessProperty 的生效值）。
     */
    [[nodiscard]] math::Thickness glass_frame_thickness() const noexcept;

    /**
     * @brief 以编程方式发起窗口拖拽，类似 WPF 的 Window.DragMove()。
     *
     * 在自定义标题栏（IsCustomChrome = true）场景下，于可拖拽区域的
     * MouseDownEvent 处理函数中调用此方法，即可触发系统接管窗口移动：
     *
     * @code
     *   // 在 MouseDownEvent 处理函数中：
     *   if (margs.button() == MouseButton::Left) {
     *       self->drag();
     *       args.set_handled(true);
     *   }
     * @endcode
     *
     * 内部实现（Win32）：ReleaseCapture() + PostMessageW(WM_NCLBUTTONDOWN, HTCAPTION)。
     * 系统接管拖拽后支持 Windows 11 Snap Layout（拖至屏幕顶部可触发分屏菜单）。
     *
     * @pre 窗口必须已初始化（show() 已调用），否则为空操作并记录断言。
     */
    void drag();

    // ── 内容根 ───────────────────────────────────────────────────────────────

    /**
     * @brief 设置窗口的 UI 内容根元素。
     *
     * 设置后立即触发一次布局（Measure + Arrange）与渲染。
     * 传入 nullptr 则清除内容根（窗口仅绘制背景色）。
     *
     * @param element 新的内容根元素（非拥有指针，调用方负责生命周期）
     */
    void set_content(ui::UIElement* element);

    /**
     * @brief 获取当前内容根元素（可能为 nullptr）。
     */
    [[nodiscard]] ui::UIElement* content() const noexcept;

    // ── 平台窗口委托 ─────────────────────────────────────────────────────────

    /// 显示窗口。
    /// pending 状态下首次调用时触发懒初始化（创建原生窗口、绑定图形资源）。
    void show();

    /// 隐藏窗口（不销毁）
    void hide();

    /// 请求关闭窗口（会触发 Closing → Closed 事件链）
    void close();

    /**
     * @brief 设置窗口标题（UTF-8 字符串）。
     *
     * pending 状态下：缓存值，首次 show() 时应用到原生窗口。
     * 已初始化后：立即调用 native_window_.set_title()。
     */
    void set_title(core::StringView title);

    /**
     * @brief 设置窗口客户区逻辑像素尺寸。
     *
     * pending 状态下：缓存值，首次 show() 时应用到原生窗口。
     * 已初始化后：立即调用 native_window_.set_size()。
     */
    void set_size(math::Size size);

    /**
     * @brief 设置窗口是否允许用户调整大小（默认 true）。
     *
     * 仅在 pending 状态下（show() 之前）有效，初始化后无法更改。
     */
    void set_resizable(bool resizable);

    /**
     * @brief 设置窗口初始位置是否由系统决定（默认 true）。
     *
     * 仅在 pending 状态下（show() 之前）有效，初始化后无法更改。
     */
    void set_auto_position(bool auto_position);

    /**
     * @brief 设置窗口类型（Normal / Dialog 等）（默认 Normal）。
     *
     * 仅在 pending 状态下（show() 之前）有效，初始化后无法更改。
     */
    void set_kind(platform::WindowKind kind);

    /**
     * @brief 获取窗口当前客户区逻辑像素尺寸。
     */
    [[nodiscard]] math::Size size() const;

    /**
     * @brief 获取窗口当前 DPI（通常为 96.0 * 缩放比）。
     */
    [[nodiscard]] float dpi() const;

    /**
     * @brief 窗口是否已关闭（Closed 事件触发后为 true）。
     *
     * 关闭后不得再调用本对象的任何方法。
     */
    [[nodiscard]] bool is_closed() const noexcept;

    // ── 平台窗口访问 ─────────────────────────────────────────────────────────

    /**
     * @brief 获取底层平台窗口引用（供 Application 访问事件源等）。
     */
    [[nodiscard]] platform::IWindow& native_window() noexcept;

    // ── 输入路由 ──────────────────────────────────────────────────────────────

    /**
     * @brief 获取窗口内置的输入路由器。
     *
     * 窗口创建时已自动将 InputRouter 订阅到原生窗口事件源；
     * set_content() 时也会自动设置路由根节点与默认键盘焦点。
     * 调用方可通过此接口设置自定义键盘焦点元素等高级需求。
     */
    [[nodiscard]] input::InputRouter& input_router() noexcept;

    /**
     * @brief 注册输入事件处理完毕后的回调（用于触发动画 tick + 重绘）。
     *
     * 每次鼠标/键盘事件经 InputRouter 路由完成后，Window 会调用此回调。
     * Application::create_window() 内部自动安装 tick_and_render 作为回调，
     * 业务代码通常不需要直接调用此方法。
     *
     * @param fn 回调函数（nullptr 表示清除回调）
     */
    void set_on_input_processed(std::function<void()> fn);

    // ── 渲染 ─────────────────────────────────────────────────────────────────

    /**
     * @brief 驱动一次完整的布局 + 渲染 + Present。
     *
     * 通常由内部平台事件自动触发（Resized / DpiChanged / 渲染脏区）；
     * 应用层也可在内容变化后手动调用以立即刷新画面。
     *
     * 若窗口已关闭（is_closed() == true），此函数为空操作。
     */
    void render();

private:
    /**
     * @brief PaddingProperty 静态变更回调。
     *
     * 在 set_value(PaddingProperty, ...) 触发生效值变更时自动调用。
     * 若窗口已初始化且未关闭，立即驱动重新布局与渲染。
     */
    static void s_on_padding_changed(DependencyObject*         sender,
                                     const DependencyProperty& prop,
                                     const core::Variant&      old_v,
                                     const core::Variant&      new_v) noexcept;

    /**
     * @brief Chrome 相关 DP 属性的统一变更回调。
     *
     * 当 IsCustomChromeProperty / CaptionHeightProperty / ResizeBorderThicknessProperty /
     * CornerPreferenceProperty / GlassFrameThicknessProperty 任一发生变更时调用。
     * 将整个 Chrome 配置打包为 WindowChromeDesc 并提交到平台层。
     */
    static void s_on_chrome_changed(DependencyObject*         sender,
                                    const DependencyProperty& prop,
                                    const core::Variant&      old_v,
                                    const core::Variant&      new_v) noexcept;

    struct Impl;
    core::Pimpl<Impl> p_;
};

} // namespace mine::ui
