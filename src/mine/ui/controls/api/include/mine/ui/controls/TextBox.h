/**
 * @file TextBox.h
 * @brief TextBox —— 单行文本输入控件。
 *
 * 实现规范（MD3 Filled Text Field 风格）：
 *   - Normal / Hovered / Focused / Disabled 四种视觉状态
 *   - 边框颜色过渡动画（VSM Storyboard 驱动，120ms EaseInOut）
 *   - 键盘光标定位：Left / Right / Home / End
 *   - 字符删除：Backspace（删光标前）/ Delete（删光标后）
 *   - 字符插入：TextInputEvent 派发的 UTF-32 码点转为 UTF-8 插入缓冲
 *   - 光标闪烁：AnimationClock 驱动，500ms 周期（仅获焦时可见）
 *   - 占位文字（PlaceholderProperty）：无文本时显示，颜色较淡
 *
 * 焦点管理：
 *   TextBox 在构造函数中调用 set_focusable(true)。
 *   InputRouter::dispatch_mouse_event 在 MouseDown 命中可聚焦元素时
 *   自动调用 set_keyboard_focus()，无需 TextBox 显式引用 Window。
 */

#pragma once

#include <mine/ui/controls/Api.h>
#include <mine/ui/controls/TextBlock.h>
#include <mine/ui/visual/Control.h>
#include <mine/ui/event/RoutedEvent.h>
#include <mine/ui/property/DependencyProperty.h>
#include <mine/containers/InlineString.h>
#include <mine/paint/Brush.h>
#include <mine/math/Thickness.h>
#include <mine/core/StringView.h>

namespace mine::paint { class Canvas; }
namespace mine::ui::input { class MouseEventArgs; class KeyEventArgs; class TextInputEventArgs; }

namespace mine::ui {

/**
 * @brief 单行文本输入控件（MD3 Filled Text Field 风格）。
 *
 * 直接继承 Control（而非 ContentControl），TextBox 自行管理文字缓冲。
 * 所有可视属性均通过 DependencyProperty 管理（单一真相源）。
 * 光标闪烁和边框过渡均由 AnimationClock 统一驱动。
 */
class MINE_UI_CONTROLS_API TextBox : public Control {
public:
    // ── 路由事件 ───────────────────────────────────────────────────────────

    /**
     * @brief 文本内容变更事件（Bubble 策略）。
     *
     * 在用户插入/删除字符后派发。EventArgs 为基类 RoutedEventArgs，
     * 调用方通过 text() 读取最新内容。
     */
    static const RoutedEvent& TextChangedEvent();

    // ── 依赖属性 ───────────────────────────────────────────────────────────

    /**
     * @brief 文本内容属性（InlineString）。
     *
     * 与 text_buf_ 缓存双向同步。直接通过 set_text() 或 DependencyProperty
     * 系统写入均可。
     */
    static const DependencyProperty& TextProperty;

    /**
     * @brief 占位文字属性（InlineString）。
     *
     * 当 TextProperty 为空时显示，颜色由 PlaceholderForegroundProperty 控制。
     */
    static const DependencyProperty& PlaceholderProperty;

    /**
     * @brief 只读模式属性（bool，默认 false）。
     *
     * true 时禁止字符输入和退格/删除操作；光标仍可见但不闪烁，
     * 不禁用选择（当前阶段无文本选择功能，留作后续扩展）。
     */
    static const DependencyProperty& IsReadOnlyProperty;

    /**
     * @brief 接受回车键属性（bool，默认 false）。
     *
     * false（单行模式）：Enter 键不插入换行符，控件保持单行；
     * true（多行模式）：Enter 键插入换行符 \n，支持多行文本显示和编辑，
     *                   Up/Down 键在行间移动光标。
     */
    static const DependencyProperty& AcceptsReturnProperty;

    /**
     * @brief 自动换行模式属性（TextWrapping，默认 Wrap）。
     *
     * NoWrap：不自动换行，单行显示；
     * Wrap：按字符边界自动换行；
     * WrapAtWord：按单词边界自动换行。
     */
    static const DependencyProperty& TextWrappingProperty;

    /**
     * @brief 背景画刷属性（Variant 存储 paint::Brush）。
     *
     * 默认：MD3 Surface #FFFBFE。
     */
    static const DependencyProperty& BackgroundProperty;

    /**
     * @brief 文字前景画刷（MD3 On Surface #1C1B1F）。
     */
    static const DependencyProperty& ForegroundProperty;

    /**
     * @brief 边框颜色属性（MD3 Outline #79747E）。
     *
     * VSM Storyboard 在状态切换时通过 Animation(P1) 对此属性插值：
     *   Normal   : #79747E
     *   Hovered  : #1C1B1F
     *   Focused  : #6750A4（MD3 Primary，边框加粗至 2px）
     *   Disabled : Transparent
     */
    static const DependencyProperty& BorderColorProperty;

    /**
     * @brief 占位文字颜色属性（MD3 On Surface Variant #49454F）。
     */
    static const DependencyProperty& PlaceholderForegroundProperty;

    /**
     * @brief 内边距属性（Thickness，默认 {16, 8, 16, 8}）。
     */
    static const DependencyProperty& PaddingProperty;

    /**
     * @brief 圆角半径属性（float，默认 4.0f）。
     */
    static const DependencyProperty& CornerRadiusProperty;

    // ── 生命周期 ───────────────────────────────────────────────────────────

    TextBox();
    ~TextBox() override;

    TextBox(const TextBox&)            = delete;
    TextBox& operator=(const TextBox&) = delete;
    TextBox(TextBox&&)                 = default;
    TextBox& operator=(TextBox&&)      = default;

    // ── 属性访问 ───────────────────────────────────────────────────────────

    /**
     * @brief 读取当前文字内容（UTF-8）。
     */
    [[nodiscard]] core::StringView text() const noexcept;

    /**
     * @brief 读取当前选中字符串（UTF-8）。
     *
     * 无选区时返回空视图。
     */
    [[nodiscard]] core::StringView selected_text() const noexcept;

    /**
     * @brief 设置文字内容并将光标移到末尾。
     */
    void set_text(core::StringView text);

    /**
     * @brief 读取占位文字（UTF-8）。
     */
    [[nodiscard]] core::StringView placeholder() const noexcept;

    /**
     * @brief 设置占位文字。
     */
    void set_placeholder(core::StringView placeholder);

    /**
     * @brief 读取只读标志。
     */
    [[nodiscard]] bool is_read_only() const noexcept;

    /**
     * @brief 设置只读模式。
     */
    void set_read_only(bool read_only) noexcept;

    /**
     * @brief 读取 AcceptsReturn 标志（多行模式）。
     */
    [[nodiscard]] bool accepts_return() const noexcept;

    /**
     * @brief 设置是否接受 Enter 键（启用多行模式）。
     */
    void set_accepts_return(bool accepts) noexcept;

    /**
     * @brief 读取自动换行模式。
     */
    [[nodiscard]] TextWrapping text_wrapping() const noexcept;

    /**
     * @brief 设置自动换行模式。
     */
    void set_text_wrapping(TextWrapping mode) noexcept;

    /**
     * @brief 是否启用（影响 Disabled 视觉状态）。
     */
    [[nodiscard]] bool is_enabled() const noexcept;

    /**
     * @brief 设置启用/禁用状态。
     */
    void set_enabled(bool enabled) noexcept;

    /**
     * @brief 设置渲染字体（void* 指向 text::FontFace*）。
     */
    void set_font_face(void* font_face) noexcept;

    /**
     * @brief 设置字号（逻辑像素，默认 14.0f）。
     */
    void set_font_size(float size_px) noexcept;

protected:
    // ── 布局 ──────────────────────────────────────────────────────────────

    [[nodiscard]] math::Size measure_override(math::Size available) override;
    void on_render(paint::Canvas& canvas) override;
    void on_arrange(math::Rect final_rect) override;

    // ── 视觉状态 ──────────────────────────────────────────────────────────

    [[nodiscard]] ControlVisualState compute_visual_state() const override;
    void on_visual_state_changed(ControlVisualState old_state,
                                 ControlVisualState new_state) override;

private:
    // ── 事件处理路由静态方法 ──────────────────────────────────────────────

    static void on_mouse_enter_router(void*, RoutedEventArgs&, void* ud);
    static void on_mouse_leave_router(void*, RoutedEventArgs&, void* ud);
    static void on_mouse_down_router (void*, RoutedEventArgs&, void* ud);
    static void on_mouse_move_router (void*, RoutedEventArgs&, void* ud);
    static void on_mouse_up_router   (void*, RoutedEventArgs&, void* ud);
    static void on_key_down_router   (void*, RoutedEventArgs&, void* ud);
    static void on_text_input_router (void*, RoutedEventArgs&, void* ud);
    static void on_got_focus_router  (void*, RoutedEventArgs&, void* ud);
    static void on_lost_focus_router (void*, RoutedEventArgs&, void* ud);

    // ── 实际事件处理逻辑 ──────────────────────────────────────────────────

    void on_mouse_enter();
    void on_mouse_leave();
    void on_mouse_down(input::MouseEventArgs& args);
    void on_mouse_move(input::MouseEventArgs& args);
    void on_mouse_up  (input::MouseEventArgs& args);
    void on_key_down  (input::KeyEventArgs& args);
    void on_text_input(input::TextInputEventArgs& args);

    // ── 光标闪烁 AnimationClock 回调 ─────────────────────────────────────

    /**
     * @brief AnimationClock tick 回调（静态方法）。
     *
     * 每帧累加 dt 计时，每 500ms 翻转 cursor_visible_，并调用 invalidate_render。
     * 当 is_focused_ == false 时返回 false，AnimationClock 自动注销。
     *
     * @param handle 指向 TextBox 实例的 void*
     * @param dt     距上一帧的时间（秒）
     * @return true  继续接收 tick；false 注销
     */
    static bool anim_tick_callback(void* handle, float dt) noexcept;

    // ── UTF-8 辅助方法 ────────────────────────────────────────────────────

    /**
     * @brief 在 cursor_pos_ 处插入 UTF-32 字符（转 UTF-8 写入 text_buf_）。
     *
     * 若当前有选中区间，先删除选区再插入。
     */
    void insert_char(uint32_t char_utf32);

    /**
     * @brief 删除光标前一个 UTF-8 字符（Backspace 行为）。
     */
    void delete_char_before();

    /**
     * @brief 删除光标后一个 UTF-8 字符（Delete 行为）。
     */
    void delete_char_after();

    /**
     * @brief 光标向左移动一个 UTF-8 字符（Left 键）。
     */
    void move_cursor_left();

    /**
     * @brief 光标向右移动一个 UTF-8 字符（Right 键）。
     */
    void move_cursor_right();

    /**
     * @brief 计算字体行高（有字体则用 FontFace::line_height，否则估算）。
     */
    [[nodiscard]] float effective_line_height() const noexcept;

    /**
     * @brief 测量 UTF-8 字符串片段的渲染宽度。
     */
    [[nodiscard]] float measure_text_width(const char* utf8, uint32_t len) const noexcept;

    // ── 选择区间辅助方法 ─────────────────────────────────────────────────

    /**
     * @brief 当前是否有选中区间（cursor_pos_ != sel_anchor_）。
     */
    [[nodiscard]] bool has_selection() const noexcept;

    /**
     * @brief 返回选区起始字节偏移（较小值）。
     */
    [[nodiscard]] uint32_t sel_start() const noexcept;

    /**
     * @brief 返回选区结束字节偏移（较大值）。
     */
    [[nodiscard]] uint32_t sel_end() const noexcept;

    /**
     * @brief 删除当前选中区间的文字，并将 cursor_pos_ 和 sel_anchor_ 置于区间起点。
     */
    void delete_selection();

    /**
     * @brief 根据点击 x 坐标（相对于文字区域左边缘）计算最近的字节偏移。
     *
     * 使用字符中点判断光标落点，返回对应 UTF-8 序列的起始字节偏移。
     */
    [[nodiscard]] uint32_t cursor_pos_from_x(float click_x) const noexcept;

    /**
     * @brief 根据点击坐标（相对于文字区域左上角）计算最近的字节偏移（多行模式）。
     *
     * 先根据 y 确定行号，再在该行内根据 x 确定字符位置。
     */
    [[nodiscard]] uint32_t cursor_pos_from_xy(float click_x, float click_y) const noexcept;

    /**
     * @brief 光标向上移动一行（多行模式 Up 键）。
     */
    void move_cursor_up();

    /**
     * @brief 光标向下移动一行（多行模式 Down 键）。
     */
    void move_cursor_down();

    // ── 剪贴板支持 ────────────────────────────────────────────────────────

    /**
     * @brief 将选中文字（无选区时复制全部）写入系统剪贴板。
     *
     * 仅在 _WIN32 编译时有实际效果（通过 Win32 API 直接操作剪贴板）。
     */
    void copy_to_clipboard() const;

    /**
     * @brief 从系统剪贴板读取 Unicode 文字并粘贴到当前光标位置。
     *
     * 若有选区先删除选区；粘贴后 cursor_pos_ 移到粘贴内容末尾。
     * 只读模式不执行任何操作。
     */
    void paste_from_clipboard();

    // ── 私有字段 ──────────────────────────────────────────────────────────

    containers::InlineString text_buf_;         ///< 实际文字内容缓冲（UTF-8）
    containers::InlineString placeholder_buf_;  ///< 占位文字缓冲（UTF-8）

    bool  is_read_only_     = false;  ///< 只读模式
    bool  is_hovered_       = false;  ///< 鼠标悬停状态
    bool  is_focused_       = false;  ///< 键盘焦点状态
    bool  is_enabled_       = true;   ///< 启用/禁用状态
    bool  is_mouse_selecting_ = false; ///< 鼠标左键拖拽选择进行中

    uint32_t cursor_pos_         = 0;      ///< 光标字节偏移（在 text_buf_ 中）
    uint32_t sel_anchor_         = 0;      ///< 选择锚点字节偏移（Shift/点击选择的固定端；无选区时等于 cursor_pos_）
    float    cursor_target_x_    = 0.0f;   ///< Up/Down 键移动时保持的目标 x 坐标（像素）
    bool     cursor_visible_     = true;   ///< 光标当前是否可见（闪烁控制）
    float    cursor_blink_accum_ = 0.0f;   ///< 光标闪烁累计时间（秒）

    void*  font_face_   = nullptr;  ///< 字体（text::FontFace*，可为 nullptr）
    float  font_size_px_ = 14.0f;   ///< 字号（逻辑像素）
};

} // namespace mine::ui
